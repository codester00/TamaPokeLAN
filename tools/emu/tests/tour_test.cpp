// The Battle Zone's tournament (v4.06, postgame-plan § 2). The draw (15
// distinct drawable opponents, generated fillers when too few packs, the
// strongest seeded to meet the player late), the AI-vs-AI simulator, and a
// whole run through the zone's screens and fights: levels capped at each
// opponent's best, the other matches fought in the background, opponents'
// PP carried, switching from the semi-final, BP, the record, the save, a
// reboot mid-battle as the loss, a broken team, a loss, QUIT.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "zone.h"
#include "tournament.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <set>
uint32_t g_seed = 0x70A; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
void uiTileGo(int i);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
void plazaBtnRect(uint8_t i, int *x, int *y, int *w, int *h);
void zoneRowRect(uint8_t i, int *x, int *y, int *w, int *h);
void pickGoPos(int *x, int *y);
void tourLoad();
bool tourBegin(uint16_t mask);
uint8_t btlMenuNow();
void btlCellCenter(int i, int *x, int *y);
uint8_t btlSwitchOpts(uint8_t out[TRAINER_TEAM_MAX]);
void btlSwitchCell(uint8_t k, int *x, int *y);
extern Pet pet;
extern uint16_t gRegionArt;
extern bool plazaOpen, zoneOpen, battleOpen, btlZone, btlHard, pickOpen;
extern uint8_t zoneMode, btlMenu, btlMsgCount, btlSquadN, pickTrainer, pickZoneKind, btlFoeSquadN;
extern Combatant btlYou, btlFoe, btlSquad[], btlFoeSquad[];
extern ZoneRun zoneRun;
extern ZoneFoe zoneFoe;
extern char btlMsg[8][40];
extern TourSave tour;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void toPlaza() { for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i); }
static void tapPlaza(uint8_t i) { int x, y, w, h; plazaBtnRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapRow(uint8_t i) { int x, y, w, h; zoneRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapFight() { int x, y; pickGoPos(&x, &y); onTap((int16_t)x, (int16_t)y); }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); battleTap(x, y); }
static void clearMsgs() { for (int i = 0; i < 60 && btlMsgCount && battleOpen; i++) battleTap(233, 300); }
static void fightOut(bool win) {
  for (int t = 0; t < 400 && battleOpen; t++) {
    clearMsgs();
    if (!battleOpen) break;
    if (btlMenuNow() == 4) {
      uint8_t o[6]; uint8_t n = btlSwitchOpts(o);
      for (uint8_t k = 0; k < n; k++)
        if (!btlSquad[o[k]].fainted()) { int x, y; btlSwitchCell(k, &x, &y); battleTap(x + 80, y + 27); break; }
      continue;
    }
    if (win) { btlFoe.hp = 1; btlYou.hp = btlYou.maxHp; } else btlYou.hp = 1;
    btlMenu = 1;
    int slot = 0;                       // PP carries through the run: a move with some left
    while (slot < 3 && (!btlYou.moves[slot] || !btlYou.pp[slot])) slot++;
    tapCell(slot);
  }
}
static PartyMon mon(int16_t dex, uint8_t lvl) {
  PartyMon m; m.dex = dex; m.level = lvl;
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 31;
  m.moves[0] = MV_TACKLE;
  m.uid = pet.mintUid();
  return m;
}
static uint8_t aliveCount() { uint8_t n = 0; for (int i = 0; i < 16; i++) if (tour.alive & (1u << i)) n++; return n; }
static bool onePerPair(uint8_t size) {    // exactly one alive in every block of `size`
  for (int b = 0; b < 16; b += size) {
    int n = 0;
    for (int i = b; i < b + size; i++) if (tour.alive & (1u << i)) n++;
    if (n != 1) return false;
  }
  return true;
}
static void startRun() {
  zoneOpen = plazaOpen = true; zoneMode = 4;
  tapRow(0);
  squadMask = 0x03;
  tapFight();
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the draw -------------------------------------------------------------
  { uint16_t a[16], b[16], c[16];
    tourDraw(1234, a); tourDraw(1234, b); tourDraw(999, c);
    ck(!memcmp(a, b, sizeof(a)), "the same seed, the same draw");
    ck(memcmp(a, c, sizeof(a)) != 0, "another seed, another draw");
    std::set<uint16_t> seen; bool drawable = true;
    for (int i = 1; i < 16; i++) { seen.insert(a[i]); if (!tourSeatDrawable(a[i]) || (a[i] & TOUR_GEN)) drawable = false; }
    ck(a[0] == TOUR_PLAYER && seen.size() == 15 && !seen.count(TOUR_PLAYER), "the player first, then 15 distinct opponents");
    ck(drawable, "all real seats, every creature drawable, with every pack");
    uint8_t top = 0, low = 255;
    for (int i = 1; i < 16; i++) { uint8_t l = tourBestLevel(a[i], 1234); if (l > top) top = l; if (l < low) low = l; }
    ck(tourBestLevel(a[8], 1234) == top, "the strongest is seed 2, across the bracket: met only in the final");
    ck(tourBestLevel(a[1], 1234) == low, "the weakest is seed 16: the player's first opponent");
    ck(tourBestLevel(a[2], 1234) >= tourBestLevel(a[1], 1234) && tourBestLevel(a[3], 1234) >= tourBestLevel(a[1], 1234),
       "the second round's possible opponents are no weaker than the first's");
    ZoneFoe f;
    uint16_t lance = tourSeatId(0, 12);
    ck(tourFoe(lance, 0, 5, 19, f) && f.count == TRAINER_SETS[0].list[12].count && f.level[0] == TRAINER_SETS[0].list[12].team[0].level &&
       f.iv == HARD_IV && f.tr == hardTraining(0, 12) && f.smart && !f.switching,
       "a seat brings its own team at its roster levels, IV 31, its hard training, the smart AI");
    ck(!tourSwitches(0) && !tourSwitches(1) && tourSwitches(2) && tourSwitches(3), "opponents switch in the semi-final and the final");
    tourFoe(lance, 2, 5, 19, f);
    ck(f.switching, "(and carry it)");
    uint16_t keep = gRegionArt;
    int nDraw = 0, packR = -1;                  // one pack that gives some real seats, but fewer than 15
    for (uint8_t pr = 0; pr < GYM_REGIONS && packR < 0; pr++) {
      gRegionArt = (uint16_t)(1u << pr);
      nDraw = 0;
      for (uint8_t r = 0; r < GYM_REGIONS; r++) for (uint8_t t = 0; t < TRAINER_COUNT; t++) if (tourSeatDrawable(tourSeatId(r, t))) nDraw++;
      if (nDraw > 0 && nDraw < 15) packR = pr;
    }
    uint16_t k[16]; tourDraw(77, k);
    int gens = 0; bool ok = true;
    for (int i = 1; i < 16; i++) {
      if (k[i] & TOUR_GEN) { gens++; if (!tourFoe(k[i], 0, 77, 19, f) || !f.count) ok = false;
                             for (int m = 0; m < f.count; m++) if (!speciesShowable(f.dex[m])) ok = false; }
      else if (!tourSeatDrawable(k[i])) ok = false;
    }
    printf("(only region %d's pack: %d drawable seats)\n", packR, nDraw);
    ck(packR >= 0 && gens == 15 - nDraw && ok, "with one pack: every drawable seat, then generated trainers, all drawable");
    gRegionArt = keep; }

  // --- 2. the background matches -----------------------------------------------
  { int strongWins = 0; bool changed = false;
    for (int r = 0; r < 20; r++) {
      Combatant a[2], b[2];
      Pet x; x.dbgHatchAs(150, false); x.ageMinutes = 79UL * MINUTES_PER_LEVEL; x.relearnFromLevel();
      combatantFromPet(a[0], x); a[1] = a[0];
      Pet y; y.dbgHatchAs(19, false); y.ageMinutes = 9UL * MINUTES_PER_LEVEL; y.relearnFromLevel();
      combatantFromPet(b[0], y); b[1] = b[0];
      battleFullPp(a[0]); battleFullPp(a[1]); battleFullPp(b[0]); battleFullPp(b[1]);
      uint8_t pp0 = a[0].pp[0] + a[0].pp[1] + a[0].pp[2] + a[0].pp[3];
      if (battleSimulate(a, 2, b, 2, true, false, false) == 0) strongWins++;
      if (a[0].pp[0] + a[0].pp[1] + a[0].pp[2] + a[0].pp[3] < pp0 && b[0].fainted()) changed = true;
    }
    ck(strongWins >= 19, "AI against AI: a level 80 MEWTWO pair beats a level 10 RATTATA pair");
    ck(changed, "the teams come back changed: fainted, PP spent"); }

  // --- 3. a whole tournament ----------------------------------------------------
  pet.dbgHatchAs(150, false);
  pet.ageMinutes = 79UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  party.slots[0] = mon(9, 90);
  for (uint8_t r = 0; r < GYM_REGIONS; r++) pet.winBadge(r, TRAINER_COUNT - 1, false);
  pet.wallet = 500000;
  pet.bp = 0;
  uint16_t wins0 = pet.zoneTourWins;
  toPlaza();
  tapPlaza(3);
  tapRow(0);
  ck(!strcmp(scr(), "zone") && zoneMode == 4, "TOURNAMENT opens its page");
  tapRow(0);
  ck(!strcmp(scr(), "pick") && pickTrainer == 0xFE && pickZoneKind == ZONE_TOURNAMENT, "ENTER opens the team picker");
  uint32_t w0 = pet.wallet;
  squadMask = 0x03;
  tapFight();
  ck(tour.active && battleOpen && btlZone && pet.wallet == w0 - ZONE_FEE[ZONE_TOURNAMENT], "FIGHT: the fee, the draw, the first match");
  ck(tour.fighting == 1 && nvs().count("tour") && nvs()["tour"][3] == 1, "saved as fighting before the battle");
  uint8_t cap = tourBestLevel(tour.bracket[1], tour.seed);
  ck(btlSquadN == 2 && btlSquad[0].level == (cap < 80 ? cap : 80) && btlSquad[1].level == (cap < 90 ? cap : 90),
     "our levels capped at the opponent's best");
  ck(btlHard && !zoneFoe.switching, "the round of 16: the smart AI, no switching");
  fightOut(true);
  ck(!battleOpen && !strcmp(scr(), "zone") && zoneMode == 4, "a win comes back to the tournament page");
  ck(tour.round == 1 && aliveCount() == 8 && onePerPair(2) && (tour.alive & 1), "the other seven matches were fought: 8 left, one from each pair");
  ck(pet.bp == ZONE_BP_WIN[ZONE_TOURNAMENT], "and the win paid BP");
  bool used = false;
  for (int i = 1; i < 16; i++) if (tour.alive & (1u << i))
    for (int k = 0; k < 6; k++) for (int m = 0; m < 4; m++) if (tour.oppUsed[i][k][m]) used = true;
  ck(used, "the winners carry the PP they used");
  { TourSave before = tour; tour = TourSave(); tourLoad();
    ck(tour.active && !memcmp(&tour, &before, sizeof(tour)), "a reboot between rounds keeps the tournament as it was"); }

  int8_t opp = tourPlayerOpponent(tour);
  tapRow(0);
  ck(battleOpen && !zoneFoe.switching, "the quarter-final: still no switching");
  bool ppMatch = true;
  for (uint8_t k = 0; k < btlFoeSquadN; k++) {
    Combatant full = btlFoeSquad[k]; battleFullPp(full);
    for (int m = 0; m < 4; m++) if (btlFoeSquad[k].pp[m] + tour.oppUsed[opp][k][m] != full.pp[m] && btlFoeSquad[k].pp[m] != 0) ppMatch = false;
  }
  ck(ppMatch, "the opponent comes in with the PP it had left");
  fightOut(true);
  ck(tour.round == 2 && aliveCount() == 4 && onePerPair(4), "quarter-finals done: 4 left");
  tapRow(0);
  ck(battleOpen && zoneFoe.switching, "the semi-final: the opponent switches");
  fightOut(true);
  ck(tour.round == 3 && aliveCount() == 2 && onePerPair(8), "the final: 2 left, one from each half");
  tapRow(0);
  ck(battleOpen && zoneFoe.switching, "the final: switching too");
  fightOut(true);
  ck(!tour.active && tour.result == TOUR_RES_CHAMPION && pet.zoneTourWins == wins0 + 1, "four wins: champion, and the record");
  ck(pet.bp == 4 * ZONE_BP_WIN[ZONE_TOURNAMENT] + ZONE_TOUR_CHAMP_BP && !zoneRun.active,
     "BP for every win and the champion's bonus (v4.09); the run is over");

  // --- 4. a reboot mid-battle is the loss --------------------------------------
  startRun();
  ck(tour.active && battleOpen, "(a new run)");
  battleOpen = false; btlZone = false; zoneRun.active = false;
  tourLoad();
  ck(!tour.active && tour.result == TOUR_RES_OUT && tour.resultRound == 0, "a reboot mid-battle: knocked out in the round of 16");

  // --- 5. a loss -------------------------------------------------------------------
  startRun();
  fightOut(false);
  ck(!tour.active && tour.result == TOUR_RES_OUT && !zoneRun.active && !strcmp(scr(), "zone"), "a loss knocks the player out");

  // --- 6. a broken team, QUIT, one run at a time ---------------------------------------
  startRun(); fightOut(true);
  ck(tour.active && tour.round == 1, "(a run, one round in)");
  ck(!tourBegin(0x03), "only one tournament at a time");
  party.slots[0] = PartyMon();
  tapRow(0);
  ck(!battleOpen && !tour.active && tour.result == TOUR_RES_BROKEN, "a member gone: CONTINUE ends the run and says why");
  party.slots[0] = mon(9, 90);
  startRun(); fightOut(true);
  tapRow(2);
  ck(tour.active, "QUIT asks first");
  tapRow(2);
  ck(!tour.active && tour.result == TOUR_RES_OUT, "the second tap ends it");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
