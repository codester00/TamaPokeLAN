// The Battle Zone's shell (v4.03, postgame-plan step 2). The gate (every
// champion) and the code's way in with no marks; the prize counter's BP; the
// run's fee; a zone fight: SET style, no bag, the PP it uses kept in the run's
// own ledger and never the PP book, SKETCH not kept, no blackout, a win paying
// BP and a loss ending the run; the gauntlet's best streak; and BP and the
// records surviving a reload through each half of the save. Driven through the
// firmware's own screens and fights.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "pp.h"
#include "codes.h"
#include "zone.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t g_seed = 0x20E; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
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
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
bool zoneAccessible();
bool zoneRunBegin(uint8_t kind, uint16_t mask);
bool zoneStartFight(const ZoneFoe &f);
void codeApply(uint8_t id);
const char *btlMainOpt(uint8_t i, bool *live);
bool battleStyleShift();
uint8_t btlMenuNow();
void btlCellCenter(int i, int *x, int *y);
uint8_t btlSwitchOpts(uint8_t out[TRAINER_TEAM_MAX]);
void btlSwitchCell(uint8_t k, int *x, int *y);
extern Pet pet;
extern Inventory bag;
extern bool plazaOpen, zoneOpen, zoneViaCode, battleOpen, btlZone, btlOver, blackoutOpen;
extern uint8_t zoneMode, zoneConfirm, btlMenu, btlMsgCount, btlSquadN, btlSquadAt;
extern Combatant btlYou, btlFoe, btlSquad[];
extern char btlMsg[8][40];
extern ZoneRun zoneRun;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void toPlaza() { for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i); }
static void tapPlaza(uint8_t i) { int x, y, w, h; plazaBtnRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapRow(uint8_t i) { int x, y, w, h; zoneRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapYes() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); onTap(233, (a + b) / 2); }
static void tapNo() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); onTap(233, (c + d) / 2); }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); battleTap(x, y); }
static void clearMsgs() { for (int i = 0; i < 60 && btlMsgCount && battleOpen; i++) battleTap(233, 300); }
// Fights it out with move 0, choosing the first standing one after a faint.
static void fightOut(bool weaken) {
  for (int t = 0; t < 200 && battleOpen; t++) {
    clearMsgs();
    if (!battleOpen) break;
    if (btlMenuNow() == 4) {
      uint8_t o[6]; uint8_t n = btlSwitchOpts(o);
      for (uint8_t k = 0; k < n; k++)
        if (!btlSquad[o[k]].fainted()) { int x, y; btlSwitchCell(k, &x, &y); battleTap(x + 80, y + 27); break; }
      continue;
    }
    if (weaken) btlYou.hp = 1;
    btlMenu = 1;
    tapCell(0);
  }
}
static ZoneFoe foe(int16_t dex, uint8_t lvl, uint8_t n, uint8_t iv, uint8_t tr) {
  ZoneFoe f{};
  snprintf(f.name, sizeof(f.name), "%s", "ACE");
  f.count = n; f.iv = iv; f.tr = tr;
  for (uint8_t i = 0; i < n; i++) { f.dex[i] = dex; f.level[i] = lvl; }
  return f;
}
static void team() {
  pet.dbgHatchAs(150, false);   // MEWTWO
  pet.ageMinutes = 79UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  for (int i = 0; i < 2; i++) {
    PartyMon m; m.dex = 9; m.level = 60;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
    m.moves[0] = MV_TACKLE;
    m.uid = pet.mintUid();
    party.slots[i] = m;
  }
  squadMask = 0x07;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  team();

  // --- 1. the gate ------------------------------------------------------------
  toPlaza();
  ck(!zoneAccessible(), "a new player has no way in");
  tapPlaza(3);
  ck(!strcmp(scr(), "plaza"), "and the PLAZA's zone button refuses");
  ck(!zoneRunBegin(ZONE_GAUNTLET, squadMask), "nor can a run be started");

  // --- 2. the code: once, with no marks ----------------------------------------
  uint32_t cheats = pet.cheatsUsed; uint16_t rules = pet.rulesUsed;
  codeApply(CODE_ZONE);
  ck(!strcmp(scr(), "zone") && zoneAccessible(), "the code opens the zone");
  ck(pet.cheatsUsed == cheats && pet.rulesUsed == rules, "and marks nothing");
  onSwipeV(1);
  ck(!strcmp(scr(), "plaza") && !zoneAccessible(), "leaving lapses it: once, this visit");

  // the real gate: every champion, either difficulty
  for (uint8_t r = 0; r < GYM_REGIONS; r++) pet.winBadge(r, TRAINER_COUNT - 1, r & 1);
  tapPlaza(3);
  ck(!strcmp(scr(), "zone") && zoneMode == 0, "every champion: the PLAZA's button opens it");

  // --- 3. the prize counter ----------------------------------------------------
  pet.bp = 0;
  tapRow(2);
  ck(zoneMode == 1, "PRIZES opens the counter");
  tapRow(0);
  ck(zoneConfirm == 0xFF, "with no BP a prize is refused before it is asked");
  pet.bp = 120;
  uint8_t had = bag.count(ZONE_PRIZES[0].item);
  tapRow(0);
  ck(zoneConfirm == 0, "with enough it asks first");
  tapNo();
  ck(pet.bp == 120 && bag.count(ZONE_PRIZES[0].item) == had, "NO takes nothing");
  tapRow(0); tapYes();
  ck(pet.bp == 120 - ZONE_PRIZES[0].bp && bag.count(ZONE_PRIZES[0].item) == had + 1,
     "YES trades the BP for the prize");
  onSwipeV(1);
  ck(zoneMode == 0 && zoneOpen, "down: back to the landing page");

  // --- 4. the fee --------------------------------------------------------------
  pet.wallet = ZONE_FEE[ZONE_GAUNTLET] - 1;
  ck(!zoneRunBegin(ZONE_GAUNTLET, squadMask) && pet.wallet == ZONE_FEE[ZONE_GAUNTLET] - 1,
     "short of the fee: no run, nothing charged");
  pet.wallet = ZONE_FEE[ZONE_GAUNTLET] + 500;
  ck(zoneRunBegin(ZONE_GAUNTLET, squadMask) && pet.wallet == 500 && zoneRun.n == 3,
     "the fee starts a run with the team");
  ck(!zoneRunBegin(ZONE_GAUNTLET, squadMask), "and only one run at a time");

  // --- 5. a zone fight -----------------------------------------------------------
  uint8_t pp0 = zoneRun.pp[0][0];
  uint16_t bp0 = pet.bp;
  ck(zoneStartFight(foe(10, 3, 1, 0, 0)) && btlZone && battleOpen, "a zone fight starts");
  ck(!strcmp(scr(), "battle"), "(the battle screen)");
  bool live;
  btlMainOpt(1, &live);
  ck(!live, "no bag in the zone");
  ck(!battleStyleShift(), "SET style: no free switch");
  fightOut(false);
  ck(!battleOpen && !strcmp(scr(), "zone") && !blackoutOpen, "a win comes back to the zone");
  ck(pet.bp == bp0 + ZONE_BP_WIN[ZONE_GAUNTLET] && zoneRun.wins == 1, "and pays its BP");
  ck(zoneRun.pp[0][0] < pp0, "the PP it used is in the run's ledger");
  ck(ppLeftSlot(pet.uid, 0, pet.moves[0], 0) == ppMaxFor(pet.moves[0], 0) && !ppDirty(),
     "and never in the PP book");
  uint8_t ppAfter = zoneRun.pp[0][0];
  zoneStartFight(foe(10, 3, 1, 0, 0));
  ck(btlYou.pp[0] == ppAfter && btlYou.hp == btlYou.maxHp, "the next fight: full HP, the PP it had left");
  fightOut(false);
  ck(zoneRun.wins == 2, "(a second win)");

  // --- 6. a loss ends the run: the best streak, no blackout ---------------------------
  zoneStartFight(foe(150, 100, 1, 31, 100));
  fightOut(true);
  ck(!zoneRun.active && !blackoutOpen && !strcmp(scr(), "zone"), "a loss ends the run, back in the zone, no blackout");
  ck(pet.zoneBestStreak == 2, "and the gauntlet's best streak is its wins");

  // --- 7. SKETCH is not kept from the zone ------------------------------------------
  pet.dbgHatchAs(235, false);    // SMEARGLE
  pet.ageMinutes = 29UL * MINUTES_PER_LEVEL;
  memset(pet.moves, 0, sizeof(pet.moves));
  pet.moves[0] = MV_SKETCH;
  squadMask = 0x01;
  pet.wallet = ZONE_FEE[ZONE_GAUNTLET];
  ck(zoneRunBegin(ZONE_GAUNTLET, squadMask), "(a run with SMEARGLE)");
  // SKETCH has one PP, so turn 1's must have something to copy: a weak foe,
  // and our Smeargle slowed so the foe moves first
  zoneStartFight(foe(79, 5, 1, 0, 0));   // SLOWPOKE
  btlYou.base[SI_SPE] = 1;
  clearMsgs(); btlMenu = 1; tapCell(0);
  clearMsgs();
  ck(btlYou.moves[0] != MV_SKETCH, "SKETCH copied the foe's move in the fight");
  ck(pet.moves[0] == MV_SKETCH, "but the creature itself still knows SKETCH");
  battleOpen = false; btlZone = false; zoneRun.active = false;

  // --- 8. the save ---------------------------------------------------------------------
  {
    Pet a; a.begin();
    a.bp = 4321; a.zoneTourWins = 7; a.zoneBestStreak = 33;
    a.saveNow();
    Pet b; b.begin();
    ck(b.bp == 4321 && b.zoneTourWins == 7 && b.zoneBestStreak == 33, "a reload keeps BP and the records");
    std::vector<uint8_t> legacy = nvs()["zone"];
    nvs().erase("zone");
    Pet c; c.begin();
    ck(c.bp == 4321 && c.zoneTourWins == 7 && c.zoneBestStreak == 33, "the checkpoint alone carries them");
    nvs()["zone"] = legacy;
    nvs().erase("plyA"); nvs().erase("plyB");
    Pet d; d.begin();
    ck(d.bp == 4321 && d.zoneTourWins == 7 && d.zoneBestStreak == 33, "and so does the legacy key alone");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
