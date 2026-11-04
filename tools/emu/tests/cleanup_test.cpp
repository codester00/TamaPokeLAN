// v3.96's fixes, through the screens: the BACK pills (one rect for drawing and
// tapping, a finger's height, a dead band above), the training games' cost (in
// proportion to what they paid, nothing for nothing, refused when tired), every
// training game's result text (what it paid, joy and bond too, and it fits in
// all six languages), and TRANSFORM in a fight against a creature on the board.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "moves.h"
#include "types.h"
#include "pp.h"
#include "i18n.h"
#include "party.h"
#include "pcstore.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>

uint32_t g_seed = 0x396;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void battleTap(int16_t x, int16_t y);
int btlCellIndexAt(int16_t x, int16_t y);
void startBattle(int16_t dex, uint8_t lvl);
void backPillRect(uint8_t which, int *x, int *y, int *w, int *h);
int backZoneY(uint8_t which);
int backfillTransform();
extern Party party;
int trainGameAt(int16_t x, int16_t y);
void trainTap(int16_t x, int16_t y);
void resultGainLines(char *stats, size_t ns, char *joyLine, size_t nj, uint8_t mask);
int uiSafeHalfWidth(int y);
void mgPress(int16_t x, int16_t y, uint32_t now);
void mgStep(uint32_t now);
extern Pet pet;
extern Arduino_Canvas *gfx;
extern bool galleryOpen, galleryPick, galleryDirty, jumpOpen, trainOpen, sackOpen, spdOpen, gameOpen, battleOpen;
extern uint8_t galleryPage, galleryRegion, trainPage, miniGame, gameScore, rpickPage, btlMenu, btlMsgCount;
extern int16_t galleryDetail;
extern uint16_t sackHits, spdHits, mgPoints;
extern uint32_t sackUntil, spdUntil, gameUntil, trainTiredUntil, mgOverUntil, mgLast, rockBeatAt;
extern uint8_t rockBeat;
extern uint8_t resGain[3], resJoy, resBond;
extern Combatant btlYou, btlFoe;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static std::string printed() {
  std::string log; Arduino_Canvas::textLog = &log; galleryDirty = true; render(); Arduino_Canvas::textLog = nullptr;
  return log;
}
enum { BACK_RPICK, BACK_DEX, BACK_JUMP };

// --- the BACK pills
static void dexScreen(int which, bool page) {
  galleryOpen = true; galleryPick = which == BACK_RPICK; galleryDetail = page ? 25 : 0;
  galleryRegion = 0; galleryPage = 0; rpickPage = 0; jumpOpen = which == BACK_JUMP; galleryDirty = true;
}
static bool wentBack(int which, bool page) {   // did the last tap go up one level?
  if (which == BACK_RPICK) return !galleryOpen;
  if (which == BACK_JUMP) return !jumpOpen;
  return page ? galleryDetail == 0 : galleryPick;
}
static void backPill(int which, bool page, const char *nm) {
  char m[120];
  int x, y, w, h;
  backPillRect((uint8_t)which, &x, &y, &w, &h);
  snprintf(m, sizeof(m), "%s: BACK is a finger's height (%d px)", nm, h);
  ck(h >= 44, m);
  // both rounded ends on the glass
  float r = h / 2.0f, cy = y + r;
  float d1 = hypotf(x + r - 233, cy - 233) + r, d2 = hypotf(x + w - r - 233, cy - 233) + r;
  snprintf(m, sizeof(m), "%s: the pill sits on the round panel", nm);
  ck(d1 <= 233 && d2 <= 233, m);
  dexScreen(which, page); render();
  uint16_t px = gfx->buffer()[(y + h / 2) * 466 + x + 4];
  snprintf(m, sizeof(m), "%s: it is drawn where its rect says", nm);
  ck(px == 0xFFFF, m);   // UI_WHITE
  const int pts[5][2] = { { x + w / 2, y + h / 2 }, { x + 3, y + h / 2 }, { x + w - 3, y + h / 2 },
                          { x + w / 2, y + 2 }, { x + w / 2, y + h - 2 } };
  bool all = true;
  for (auto &p : pts) { dexScreen(which, page); render(); onTap((int16_t)p[0], (int16_t)p[1]); all &= wentBack(which, page); }
  snprintf(m, sizeof(m), "%s: the centre and every edge of the pill go back", nm);
  ck(all, m);
  if (page) return;                     // a creature's page: any tap goes back, by design
  dexScreen(which, page); render();
  onTap(233, (int16_t)(backZoneY((uint8_t)which) - 3));
  snprintf(m, sizeof(m), "%s: a tap just short of the pill does not go back", nm);
  ck(!wentBack(which, page), m);
}

// --- the training list
static bool startRow(int idx) {
  trainOpen = true; trainPage = (uint8_t)(idx / 3);
  for (int16_t y = 60; y < 400; y += 4)
    if (trainGameAt(233, y) == idx) { trainTap(233, y); return true; }
  return false;
}
static void fresh() {
  pet.energy = pet.fullness = 100; pet.joy = 50; pet.trAtk = pet.trDef = pet.trSpe = 0;
  sackOpen = spdOpen = gameOpen = trainOpen = false; miniGame = 0;
}
static void endTimed(uint32_t until) { emuAdvanceMs(until - millis() + 50); render(); }
static bool shows(const std::string &log, uint8_t gain, StrId name) {
  char b[40]; snprintf(b, sizeof(b), T(S_PLUS_FMT), gain, T(name));
  return log.find(b) != std::string::npos;
}
static uint8_t cost(uint8_t pts) { return (uint8_t)((pts * 12 + 17) / 18); }

int main() {
  emuVirtualClock(true);
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  pet.ageMinutes = 40UL * MINUTES_PER_LEVEL;
  while (pet.hasLearnOffer()) pet.declineLearn();

  // 1. BACK
  backPill(BACK_RPICK, false, "the Pokedex chooser");
  backPill(BACK_DEX, false, "the Pokedex grid");
  backPill(BACK_DEX, true, "a creature's page");
  backPill(BACK_JUMP, false, "GO TO #");
  galleryOpen = jumpOpen = false;

  // 2. the bag (ATK): 40 hits = 10 points
  fresh(); ck(startRow(0) && sackOpen, "(the bag opens from the list)");
  sackHits = 40; uint8_t j0 = pet.joy;
  endTimed(sackUntil);
  std::string log = printed();
  ck(pet.energy == 100 - cost(10) && pet.trAtk == 10, "the bag: 10 points cost 7 energy");
  ck(shows(log, 10, S_TR_ATK) && shows(log, (uint8_t)(pet.joy - j0), S_ST_JOY) && resJoy == pet.joy - j0,
     "the bag's result says the ATK and the joy it paid");
  sackOpen = false;
  fresh(); startRow(0); sackHits = 3; endTimed(sackUntil); sackOpen = false;
  ck(pet.energy == 100 && pet.trAtk == 0, "the bag: nothing earned, nothing charged");

  // 3. the reaction test (SPE): 20 hits = 10 points
  fresh(); ck(startRow(1) && spdOpen, "(the reaction test opens from the list)");
  spdHits = 20; endTimed(spdUntil); log = printed();
  ck(pet.energy == 100 - cost(10) && shows(log, 10, S_TR_SPE), "the reaction test: 7 energy, and it says +10 SPE");
  spdOpen = false;

  // 4. the ball (DEF): score 20 = 10 points; it never said its DEF before
  fresh(); ck(startRow(2) && gameOpen, "(the ball game opens from the list)");
  gameScore = 20; endTimed(gameUntil); log = printed();
  ck(pet.energy == 100 - cost(10) && pet.trDef == 10, "the ball: 7 energy, not 20 for a good round");
  ck(shows(log, 10, S_TR_DEF), "the ball's result says the DEF it paid");
  gameOpen = false;
  fresh(); startRow(2); gameScore = 0; endTimed(gameUntil); gameOpen = false;
  ck(pet.energy == 100, "the ball: a round with no score costs nothing");

  // 5. Rock Smash, every beat: 18 points, split
  fresh(); ck(startRow(3) && miniGame, "(Rock Smash opens from the list)");
  for (int b = 0; b < 40 && !mgOverUntil; b++) {
    uint32_t at = rockBeatAt;
    for (uint32_t t = mgLast + 85; t < at; t += 85) mgStep(t);
    mgPress(233, 236, at); mgStep(at + 1);
  }
  for (uint32_t t = mgLast + 85; !mgOverUntil && t < mgLast + 5000; t += 85) mgStep(t);
  emuAdvanceMs(mgLast > millis() ? mgLast - millis() : 0);
  log = printed();
  ck(mgOverUntil && pet.energy == 100 - cost(18), "Rock Smash: a full session costs 12, more than a single paying 10");
  ck(shows(log, 9, S_TR_ATK) && shows(log, 9, S_TR_SPE) && log.find(T(S_ST_JOY)) != std::string::npos,
     "Rock Smash's result says both stats and the joy");
  miniGame = 0;

  // 6. too tired
  fresh(); pet.energy = TRAIN_MIN_ENERGY - 1;
  startRow(0);
  ck(!sackOpen && trainOpen && trainTiredUntil, "below 15 energy a training game will not start");
  ck(printed().find(T(S_TOO_TIRED)) != std::string::npos, "and the list says TOO TIRED");
  startRow(7);                         // Berry Catch: a joy game
  extern bool catchOpen;
  ck(catchOpen, "a joy game still opens: that is how a tired creature is cheered up");
  catchOpen = false;
  fresh(); pet.energy = TRAIN_MIN_ENERGY; startRow(0);
  ck(sackOpen, "at 15 it trains");
  sackOpen = false;

  // 7. every result line fits, in every language, at the worst numbers
  bool fits = true; char why[160] = "";
  const uint8_t masks[] = { TR_STR, TR_DEF, TR_SPE, TR_STR | TR_SPE, TR_DEF | TR_SPE, TR_STR | TR_DEF };
  for (int l = 0; l < LANG_COUNT; l++) {
    setLang((Lang)l);
    resGain[0] = resGain[1] = resGain[2] = 18; resJoy = 30; resBond = 3;
    for (uint8_t mk : masks) {
      char st[64], jl[64];
      resultGainLines(st, sizeof(st), jl, sizeof(jl), mk);
      int halfS = uiSafeHalfWidth(246 + 16) - 8, halfJ = uiSafeHalfWidth(286 + 16) - 8;
      if ((int)strlen(st) * 6 > halfS || (int)strlen(jl) * 6 > halfJ) {
        fits = false; snprintf(why, sizeof(why), " (lang %d: '%s' / '%s')", l, st, jl);
      }
    }
  }
  setLang(LANG_EN);
  char m[220]; snprintf(m, sizeof(m), "every result line fits the panel in all six languages%s", why);
  ck(fits, m);

  // 8. TRANSFORM, in the engine
  {
    Combatant d, c;
    d.dex = 132; d.level = 50; d.maxHp = d.hp = 120; for (auto &b : d.base) b = 60;
    d.moves[0] = MV_TRANSFORM; d.ppOn = true; d.pp[0] = MOVE_TBL[MV_TRANSFORM].pp;
    c.dex = 6; c.level = 50; c.maxHp = c.hp = 160; for (int k = 0; k < SI_COUNT; k++) c.base[k] = (uint16_t)(100 + k);
    c.moves[0] = MV_TACKLE; c.moves[1] = MV_SURF; c.stage[0] = 2; c.ppOn = true;
    TurnLog lg;
    battleAct(d, c, MV_TRANSFORM, lg);
    bool copied = lg.transformed && d.dex == 6 && d.moves[0] == MV_TACKLE && d.moves[1] == MV_SURF &&
                  d.base[4] == 104 && d.stage[0] == 2 && d.hp == 120 && d.maxHp == 120;
    ck(copied, "TRANSFORM copies look, typing, stats bar HP, stages and moves");
    ck(d.pp[0] == MOVE_TBL[MV_TACKLE].pp && d.pp[1] == MOVE_TBL[MV_SURF].pp,
       "each borrowed move at the move table's PP");
    ck(typeEffVsDex(T_WATER, d.dex) == typeEffVsDex(T_WATER, 6), "and it takes hits as what it copied");
    battleAct(d, c, MV_TACKLE, lg);
    TurnLog lg2; Combatant d2 = battleStored(d);
    ck(d2.dex == 132 && d2.moves[0] == MV_TRANSFORM && d2.moves[1] == 0 && d2.pp[0] == MOVE_TBL[MV_TRANSFORM].pp - 1 &&
       d2.base[0] == 60 && d2.stage[0] == 0 && !d2.trueDex,
       "leaving the field it is a Ditto again, its TRANSFORM spent once");
    Combatant e = d2; e.moves[0] = MV_TRANSFORM;
    battleAct(e, d, MV_TRANSFORM, lg2);
    ck(lg2.failed && e.dex == 132, "it fails on a creature that is itself transformed");
  }

  // 9. TRANSFORM in a fight on the board: the live pet is a Ditto
  {
    pet.dbgHatchAs(132, false);
    pet.ageMinutes = 29UL * MINUTES_PER_LEVEL;
    for (int i = 0; i < MOVE_SLOTS; i++) pet.moves[i] = 0;
    pet.checkLearnGates();
    ck(pet.moves[0] == MV_TRANSFORM, "a Ditto from before v3.96 learns TRANSFORM");
    pet.energy = pet.fullness = pet.joy = pet.hygiene = 100; pet.sleeping = false;
    ppRefillAll();
    startBattle(6, 30);
    ck(battleOpen, "(a fight with a Charizard)");
    int16_t fx = -1, fy = -1, mx = -1, my = -1;
    for (int16_t y = 200; y < 466 && fx < 0; y += 3) for (int16_t x = 0; x < 466; x += 3)
      if (btlCellIndexAt(x, y) == 0) { fx = x + 10; fy = y + 10; break; }
    btlMenu = 0; btlMsgCount = 0;
    battleTap(fx, fy);                                     // FIGHT
    for (int16_t y = 200; y < 466 && mx < 0; y += 3) for (int16_t x = 0; x < 466; x += 3)
      if (btlCellIndexAt(x, y) == 0) { mx = x + 10; my = y + 10; break; }
    battleTap(mx, my);                                     // TRANSFORM
    bool said = false;
    extern char btlMsg[8][40];
    for (uint8_t i = 0; i < btlMsgCount; i++) said |= strstr(btlMsg[i], "transformed into CHARIZARD") != nullptr;
    ck(btlYou.dex == 6 && btlYou.trueDex == 132 && said, "the Ditto fights as the Charizard, and the fight says so");
    // v3.98: the sprite becomes the Charizard with the line that says so, not
    // before it and not never (it stayed a Ditto on the board)
    extern int16_t btlShowDex[2];
    bool beforeOk = true, after = false;
    for (int guard = 0; guard < 10 && btlMsgCount; guard++) {
      bool head = strstr(btlMsg[0], "transformed into") != nullptr;
      render();
      if (head) { after = btlShowDex[0] == 6; break; }
      beforeOk &= btlShowDex[0] == 132;
      battleTap(233, 233);
    }
    ck(beforeOk && after, "the Ditto's sprite changes to the Charizard on the line that says so");
    ck(strstr(btlYou.name, "CHARIZARD") == nullptr && btlYou.name[0], "and its name stays its own, as in the games");
    ck(ppLeftSlot(pet.uid, 0, MV_TRANSFORM, 0) == MOVE_TBL[MV_TRANSFORM].pp - 1 && pet.moves[0] == MV_TRANSFORM &&
       pet.speciesId == 132, "the pet keeps its own move, its TRANSFORM spent once in the PP book");
    battleOpen = false;
  }

  // 10. every stored Ditto gets TRANSFORM at boot (v3.97), not only once raised
  {
    auto ditto = [](uint32_t serial, uint8_t mv0) {
      PartyMon m; m.dex = 132; m.level = 30; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20; m.stateVersion = 1;
      m.uid.mac[0] = 0xD1; m.uid.serial = serial; m.uid.salt = 7; m.moves[0] = mv0; return m;
    };
    for (auto &m : party.slots) m = PartyMon();
    for (auto &m : party.box) m = PartyMon();
    party.slots[0] = ditto(9001, 0);
    party.slots[1] = ditto(9002, MV_TACKLE);     // already knows something: left alone
    party.box[0] = ditto(9003, 0);
    party.save(); party.boxSave();
    while (pcCount(132)) { PartyMon x; pcTake(132, 0, &x); }
    pcPut(ditto(9004, 0)); pcPut(ditto(9005, 0));
    pet.dbgHatchAs(4, false);                    // the live pet is no Ditto here
    int n = backfillTransform();
    PartyMon a, b; pcGet(132, 0, a); pcGet(132, 1, b);
    ck(n == 4 && party.slots[0].moves[0] == MV_TRANSFORM && party.box[0].moves[0] == MV_TRANSFORM &&
       a.moves[0] == MV_TRANSFORM && b.moves[0] == MV_TRANSFORM,
       "every moveless Ditto, in the party, the leftovers and Bill's PC, learns TRANSFORM at boot");
    ck(party.slots[1].moves[0] == MV_TACKLE && party.slots[1].moves[1] == 0, "a Ditto that knows a move is left alone");
    Party p2; p2.begin(); p2.loadBox("boxl");
    ck(p2.slots[0].moves[0] == MV_TRANSFORM && p2.box[0].moves[0] == MV_TRANSFORM, "and it is saved");
    ck(backfillTransform() == 0, "a second boot gives nothing again");
    // and it is the BOOT that does it: a moveless one saved, then the board starts
    party.slots[2] = ditto(9006, 0); party.save();
    setup();
    ck(party.slots[2].moves[0] == MV_TRANSFORM, "the boot itself teaches it");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
