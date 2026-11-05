// The Battle Hall (v4.07, postgame-plan § 4). The rules (a shuffled ladder of
// all 18 types; a foe of the rung's type near the pet in base stats, starting
// below it and climbing; the money), and a run through the PLAZA's cell and
// the Hall's screen: the live pet alone, the energy each fight costs, the real
// PP book, the money, no items and no blackout, the save, a reboot mid-battle
// as the loss, a changed pet ending the run, QUIT, and the mark for clearing
// all 18 -- which travels banked, reloaded, traded and over LAN.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "pp.h"
#include "hall.h"
#include "link.h"
#include "trade.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <set>
uint32_t g_seed = 0x4A11; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void onTap(int16_t x, int16_t y);
void uiTileGo(int i);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
void plazaBtnRect(uint8_t i, int *x, int *y, int *w, int *h);
void zoneRowRect(uint8_t i, int *x, int *y, int *w, int *h);
void bhallLoad();
const char *btlMainOpt(uint8_t i, bool *live);
uint8_t btlMenuNow();
void btlCellCenter(int i, int *x, int *y);
extern Pet pet;
extern bool plazaOpen, bhallOpen, battleOpen, btlHall, blackoutOpen;
extern uint8_t btlMenu, btlMsgCount, btlSquadN;
extern Combatant btlYou, btlFoe;
extern HallSave bhall;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void toPlaza() { for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i); }
static void tapPlaza(uint8_t i) { int x, y, w, h; plazaBtnRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapRow(uint8_t i) { int x, y, w, h; zoneRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); battleTap(x, y); }
static void clearMsgs() { for (int i = 0; i < 60 && btlMsgCount && battleOpen; i++) battleTap(233, 300); }
static void fightOut(bool win) {
  for (int t = 0; t < 300 && battleOpen; t++) {
    clearMsgs();
    if (!battleOpen) break;
    if (win) { btlFoe.hp = 1; btlYou.hp = btlYou.maxHp; }
    else { btlYou.hp = 1; btlFoe.maxHp = btlFoe.hp = 999; }   // v4.10's foes are weaker: make the loss certain
    btlMenu = 1;
    int slot = 0;
    while (slot < 3 && (!btlYou.moves[slot] || !btlYou.pp[slot])) slot++;
    tapCell(slot);
  }
}
static int bstOf(int16_t d) { const DexEntry &e = DEX_TBL[d]; return e.bHp + e.bAtk + e.bDef + e.bSpA + e.bSpD + e.bSpe; }
static void raise(int16_t dex, uint8_t lvl) {
  pet.dbgHatchAs(dex, false);
  pet.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the rules ------------------------------------------------------------
  { std::set<uint8_t> t; for (uint8_t r = 0; r < HALL_RUNGS; r++) t.insert(bhallTypeAt(77, r));
    ck(t.size() == 18, "a run meets all 18 types, each once");
    bool differs = false; for (uint8_t r = 0; r < HALL_RUNGS; r++) if (bhallTypeAt(77, r) != bhallTypeAt(78, r)) differs = true;
    ck(differs && bhallTypeAt(77, 5) == bhallTypeAt(77, 5), "in its own order: the same seed, the same order"); }
  { HallFoe a, b, z;
    ck(bhallFoe(77, 0, 6, 50, a) && bhallFoe(77, 0, 6, 50, b) && a.dex == b.dex, "the same rung and seed, the same foe");
    const DexEntry &e = DEX_TBL[a.dex];
    uint8_t ty = bhallTypeAt(77, 0);
    ck((e.type1 == ty || e.type2 == ty) && speciesShowable(a.dex) && e.rarity != R_LEGENDARIO,
       "the foe has the rung's type, can be drawn, is no legendary");
    int target = bstOf(6) * HALL_BST_PCT / 100;   // v4.10: species a little weaker than the pet
    int closer = 0, gap = abs(bstOf(a.dex) - target);
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const DexEntry &f = DEX_TBL[d];
      if ((f.type1 == ty || f.type2 == ty) && speciesShowable(d) && f.rarity != R_LEGENDARIO && abs(bstOf(d) - target) < gap) closer++;
    }
    ck(closer < HALL_POOL && HALL_BST_PCT < 100, "and is among the species of that type nearest HALL_BST_PCT of the pet's base stats");
    ck(a.level == 50 - HALL_UNDER && a.iv == HALL_IV_LO && a.tr == HALL_TR_LO && !a.smart,
       "the first rung starts below: HALL_UNDER levels under, the lowest IV and training, the simple AI");
    bhallFoe(77, HALL_RUNGS - 1, 6, 50, z);
    ck(z.level == 50 && z.iv == HALL_IV_HI && z.iv < 31 && z.tr == HALL_TR_HI && z.tr < 100 && z.smart,
       "the last rung: level with the pet, moderate IVs (never perfect), ~90% training, the smart AI");
    bool never = true;
    for (uint8_t r = 0; r < HALL_RUNGS; r++) for (uint8_t lv : { 5, 50, 100 }) {
      HallFoe h; if (bhallFoe(77, r, 6, lv, h) && h.level > lv) never = false; }
    ck(never, "a foe never outlevels the pet, at any rung (owner, v4.10)");
    bool climbs = true; HallFoe p = a;
    for (uint8_t r = 1; r < HALL_RUNGS; r++) { HallFoe q; bhallFoe(77, r, 6, 50, q);
      if (q.level < p.level || q.iv < p.iv || q.tr < p.tr || (p.smart && !q.smart)) climbs = false; p = q; }
    ck(climbs, "and every rung climbs");
    bool noLegend = true;                       // MEWTWO: a legendary's base stats, so legendaries would be nearest
    for (uint32_t sd = 1; sd < 30; sd++) for (uint8_t r = 0; r < HALL_RUNGS; r++) {
      HallFoe q; if (bhallFoe(sd, r, 150, 70, q) && DEX_TBL[q.dex].rarity == R_LEGENDARIO) noLegend = false; }
    ck(noLegend, "never a legendary, even against a legendary's base stats");
    ck(bhallPrize(0, 50) == (uint32_t)HALL_PAY * 50 * HALL_CLASS && bhallPrize(17, 50) > bhallPrize(0, 50), "a little money a rung, rising");
    ck(bhallPrize(17, 50) < trainerPrize(0, 7, true, true) * 50 / TRAINER_SETS[0].list[7].team[TRAINER_SETS[0].list[7].count - 1].level,
       "less than a gym leader at the same level"); }

  // --- 2. a run, through the screens -------------------------------------------------
  raise(6, 50);                                // CHARIZARD
  { PartyMon pm; pm.dex = 9; pm.level = 50; pm.ivAtk = pm.ivDef = pm.ivSpe = pm.ivHp = 20; pm.moves[0] = MV_TACKLE;
    pm.uid = pet.mintUid(); party.slots[0] = pm; }   // a party member, who must stay out
  toPlaza();
  tapPlaza(2);
  ck(!strcmp(scr(), "bhall") && !bhall.active, "the PLAZA's BATTLE HALL cell opens the Hall");
  tapRow(0);
  ck(bhall.active && bhall.rung == 0 && bhall.uid == pet.uid, "START begins a run for the live pet");
  uint8_t e0 = pet.energy;
  uint32_t w0 = pet.wallet;
  tapRow(0);
  ck(battleOpen && btlHall && btlSquadN == 1 && btlYou.uid == pet.uid, "the first rung: the live pet alone");
  ck(pet.energy == e0 - HALL_ENERGY_COST, "and the fight costs energy");
  { uint8_t ty = bhallTypeAt(bhall.seed, 0);
    ck(DEX_TBL[btlFoe.dex].type1 == ty || DEX_TBL[btlFoe.dex].type2 == ty, "against the rung's type"); }
  ck(bhall.fighting == 1 && nvs().count("bhall") && nvs()["bhall"][3] == 1, "saved as fighting before the battle");
  { bool live; btlMainOpt(1, &live); ck(!live, "no bag in the Hall"); }
  uint8_t foeLvl = btlFoe.level;
  fightOut(true);
  ck(!battleOpen && !strcmp(scr(), "bhall") && bhall.rung == 1 && !bhall.fighting, "a win: back in the Hall, the next rung");
  ck(pet.wallet == w0 + bhallPrize(0, foeLvl), "and the rung's money");
  { bool spent = false; for (int k = 0; k < 4; k++) if (pet.moves[k] && ppLeftSlot(pet.uid, k, pet.moves[k], ppUpsAt(pet.ppUps, k)) < ppMaxFor(pet.moves[k], ppUpsAt(pet.ppUps, k))) spent = true;
    ck(spent, "the PP used is the PP book's, as in a gym fight"); }
  { HallSave before = bhall; bhall = HallSave(); bhallLoad();
    ck(bhall.active && !memcmp(&bhall, &before, sizeof(bhall)), "a reboot between rungs keeps the run"); }

  // --- 3. too tired ----------------------------------------------------------------------
  pet.energy = HALL_ENERGY_COST - 1;
  tapRow(0);
  ck(!battleOpen && bhall.active && bhall.rung == 1 && pet.energy == HALL_ENERGY_COST - 1, "too tired: no fight, nothing spent, the run waits");
  pet.energy = 100;

  // --- 4. a reboot mid-battle is the loss --------------------------------------------------
  tapRow(0);
  ck(battleOpen, "(the second rung)");
  battleOpen = false; btlHall = false;
  bhallLoad();
  ck(!bhall.active && bhall.result == HALL_RES_LOST && bhall.resultRung == 1, "a reboot mid-battle: lost at that rung");

  // --- 5. a loss ends the run, with no blackout -------------------------------------------
  bhallOpen = true; plazaOpen = true;
  tapRow(0); tapRow(0);
  ck(battleOpen && bhall.rung == 0, "(a new run)");
  fightOut(false);
  ck(!bhall.active && bhall.result == HALL_RES_LOST && !blackoutOpen && !strcmp(scr(), "bhall"), "a loss ends the run and never blacks out");

  // --- 6. a changed pet ends the run, QUIT asks twice -------------------------------------
  tapRow(0);
  ck(bhall.active, "(a run)");
  raise(9, 50);                                // another creature is live now
  render();
  ck(!bhall.active && bhall.result == HALL_RES_CHANGED, "changing the live pet ends the run");
  tapRow(0);
  tapRow(2);
  ck(bhall.active, "QUIT asks first");
  tapRow(2);
  ck(!bhall.active, "and the second tap ends it");

  // --- 7. all 18: the mark -------------------------------------------------------------------
  tapRow(0);
  uint16_t clears0 = bhall.clears;
  bhall.rung = HALL_RUNGS - 1;
  tapRow(0);
  ck(battleOpen, "(the 18th rung)");
  fightOut(true);
  ck(!bhall.active && bhall.result == HALL_RES_CLEARED && bhall.clears == clears0 + 1, "the 18th win clears the Hall");
  ck(pet.hallMark && (pet.monFlags() & PMON_HALL), "and marks the creature");
  { Pet b; b.begin(); ck(b.hallMark, "the mark is saved with it"); }
  PartyMon m = pet.toPartyMon();
  ck(m.flags & PMON_HALL, "banked, it keeps the mark");
  ck(tradeMonValid(m), "a trade accepts it");
  Combatant c; combatantFromParty(c, m);
  LinkMon lm; linkMonFrom(lm, c);
  Combatant back; linkMonTo(back, lm);
  ck(c.hall && back.hall, "and a LAN rival sees it");
  ck(TRADE_PROTO >= 5 && LINK_PROTO >= 13, "the protocols moved, so an older board refuses rather than drops it");
  pet.newEgg();
  ck(!pet.hallMark, "the next egg starts unmarked");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
