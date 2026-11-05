// The Battle Zone's gauntlet (v4.04, postgame-plan § 3). The rules (type,
// level cap -- which locks a creature above it out -- evolution tier, team
// size), obeyed by the challengers too; easy first challengers that ramp up;
// the daily rules from the day; challengers that depend only on (rules, round,
// seed); the custom-rules screen and the team picker refusing a creature that
// does not fit; the run saved between battles, a reboot mid-battle counting as
// the loss, a team that broke the rules ending the run, QUIT asking twice; the
// day's 2x going to the first daily run that beats a challenger, for its whole
// life; and the records. Driven through the firmware's own screens and fights.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "zone.h"
#include "gauntlet.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <set>
uint32_t g_seed = 0x6A7; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
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
void gauntRuleRect(uint8_t i, int *x, int *y, int *w, int *h);
void pickCellPos(uint8_t i, int *x, int *y);
void pickGoPos(int *x, int *y);
void gauntLoad();
bool gauntletBegin(const GauntletRules &r, bool daily, uint16_t mask);
bool gauntletNextFight();
uint8_t btlMenuNow();
void btlCellCenter(int i, int *x, int *y);
uint8_t btlSwitchOpts(uint8_t out[TRAINER_TEAM_MAX]);
void btlSwitchCell(uint8_t k, int *x, int *y);
extern Pet pet;
extern bool plazaOpen, zoneOpen, battleOpen, btlZone, btlHard, blackoutOpen, pickOpen;
extern bool gauntBroken, gauntQuitArmed;
extern uint8_t zoneMode, btlMenu, btlMsgCount, btlSquadN, pickTrainer, pickPage;
extern Combatant btlYou, btlFoe, btlSquad[];
extern ZoneRun zoneRun;
extern ZoneFoe zoneFoe;
extern GauntletSave gaunt;
extern GauntletRules gauntCustom;
extern uint16_t squadMask;

static const uint8_t AVATARS_N = 19;   // avatars.h, which only the sketch includes
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void toPlaza() { for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i); }
static void tapPlaza(uint8_t i) { int x, y, w, h; plazaBtnRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapRow(uint8_t i) { int x, y, w, h; zoneRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapRule(uint8_t i) { int x, y, w, h; gauntRuleRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); battleTap(x, y); }
static void clearMsgs() { for (int i = 0; i < 60 && btlMsgCount && battleOpen; i++) battleTap(233, 300); }
// candidate n: 0 the pet, k party slot k-1 (all exist here, so cell = n)
static void tapCandidate(uint8_t n) {
  pickPage = (uint8_t)(n / 6);
  int x, y; pickCellPos((uint8_t)(n % 6), &x, &y);
  onTap((int16_t)(x + 40), (int16_t)(y + 30));
}
static void tapFight() { int x, y; pickGoPos(&x, &y); onTap((int16_t)x, (int16_t)y); }
// Fights it out with move 0; `win` leaves the foe on 1 HP, else ours.
static void fightOut(bool win) {
  for (int t = 0; t < 300 && battleOpen; t++) {
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
    tapCell(0);
  }
}
static PartyMon mon(int16_t dex, uint8_t lvl) {
  PartyMon m; m.dex = dex; m.level = lvl;
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
  m.moves[0] = MV_TACKLE;
  m.uid = pet.mintUid();
  return m;
}
static void resetBattle() { battleOpen = false; btlZone = false; btlMsgCount = 0; }

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the rules ------------------------------------------------------------------
  ck(speciesStage(1) == 0 && speciesStage(2) == 1 && speciesStage(3) == 2, "BULBASAUR basic, IVYSAUR middle, VENUSAUR final");
  ck(speciesStage(133) == 0 && speciesStage(172) == 0 && speciesStage(25) == 1, "EEVEE and PICHU basic, PIKACHU middle");
  ck(speciesStage(128) == 2 && speciesStage(150) == 2, "a creature that never evolves is final");
  { GauntletRules r; r.type = T_FIRE;
    ck(gauntletSpeciesFits(r, 4) && !gauntletSpeciesFits(r, 7), "a type rule: CHARMANDER yes, SQUIRTLE no");
    r.type = GAUNTLET_TYPE_ANY; r.tier = GT_NOLEGEND;
    ck(gauntletSpeciesFits(r, 9) && !gauntletSpeciesFits(r, 150), "NO LEGENDS refuses MEWTWO");
    r.tier = GT_BASIC;
    ck(gauntletSpeciesFits(r, 7) && !gauntletSpeciesFits(r, 9), "BASIC: SQUIRTLE yes, BLASTOISE no"); }

  // --- 2. the daily rules ---------------------------------------------------------------
  { GauntletRules a = gauntletDaily(20000), b = gauntletDaily(20000);
    ck(!memcmp(&a, &b, sizeof(a)), "the day's rules are the day's: the same twice");
    std::set<int> kinds; bool enough = true;
    for (int d = 20000; d < 20060; d++) {
      GauntletRules r = gauntletDaily(d);
      kinds.insert(r.type * 1000 + r.levelCap * 10 + r.tier);
      int n = 0;
      for (int16_t s = 1; s <= DEX_COUNT; s++)
        if (gauntletSpeciesFits(r, s) && speciesShowable(s) && DEX_TBL[s].rarity != R_LEGENDARIO) n++;
      if (n < r.size) enough = false;
    }
    ck(kinds.size() >= 20, "and they change from day to day");
    ck(enough, "every day's rules leave a challenger a team"); }

  // --- 3. challengers --------------------------------------------------------------------
  { GauntletRules r; r.type = T_WATER; r.levelCap = 50; r.tier = GT_FINAL; r.size = 4;
    ZoneFoe a, b, c;
    ck(gauntletChallenger(r, 5, 1234, 80, AVATARS_N, a) && gauntletChallenger(r, 5, 1234, 80, AVATARS_N, b) &&
       !memcmp(&a, &b, sizeof(a)), "the same rules, round and seed: the same challenger");
    bool differs = false, obeys = true;
    for (uint16_t k = 0; k < 12; k++) {
      gauntletChallenger(r, k, 1234, 80, AVATARS_N, c);
      if (memcmp(c.dex, a.dex, sizeof(a.dex))) differs = true;
      if (c.count != 4) obeys = false;
      for (uint8_t i = 0; i < c.count; i++) {
        if (!gauntletSpeciesFits(r, c.dex[i]) || DEX_TBL[c.dex[i]].rarity == R_LEGENDARIO || c.level[i] > 50) obeys = false;
        for (uint8_t j = 0; j < i; j++) if (c.dex[j] == c.dex[i]) obeys = false;
      }
      if (c.avatar < 3 || c.avatar >= AVATARS_N) obeys = false;
    }
    ck(differs, "another round, another challenger");
    ck(obeys, "challengers obey the rules: water, final, no legends, Lv 50 at most, four, no repeats");
    gauntletChallenger(r, 0, 1234, 80, AVATARS_N, c);
    ck(c.level[0] == 50 - GAUNTLET_LEVEL_RAMP && c.iv == 10 && c.tr == 0 && !c.smart,
       "the first challenger is easy: under the cap, IV 10, no training, the plain AI");
    gauntletChallenger(r, 4, 1234, 80, AVATARS_N, c);
    ck(c.level[0] == 50 - GAUNTLET_LEVEL_RAMP + 4, "a level closer every round");
    gauntletChallenger(r, GAUNTLET_LEVEL_RAMP, 1234, 80, AVATARS_N, c);
    ck(c.level[0] == 50, "at the cap once the ramp is done");
    r.levelCap = 0;
    gauntletChallenger(r, 0, 9, 63, AVATARS_N, c);
    ck(c.level[0] == 63 - GAUNTLET_LEVEL_RAMP, "no cap: they start under the team's best level");
    gauntletChallenger(r, 30, 9, 63, AVATARS_N, c);
    ck(c.level[0] == 63, "and reach it");
    bool rising = true;
    for (uint16_t k = 1; k < 40; k++) if (gauntletIv(k) < gauntletIv(k - 1) || gauntletTr(k) < gauntletTr(k - 1)) rising = false;
    ck(rising && gauntletIv(40) == 31 && gauntletTr(40) == 100, "they grow round by round, to IV 31 and full training");
    ck(gauntletTr(0) == 0 && gauntletTr(1) == 0 && gauntletTr(3) > 0, "no training for the first two");
    ck(!gauntletSmart(0) && !gauntletSmart(4) && gauntletSmart(5), "the smart AI from the sixth");
    ck(gauntletBp(4, true) == 2 * gauntletBp(4, false), "2x is twice the BP"); }

  // --- 4. the team, and into the zone -------------------------------------------------------
  pet.dbgHatchAs(150, false);   // MEWTWO
  pet.ageMinutes = 79UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  party.slots[0] = mon(9, 60);   // BLASTOISE, over the cap
  party.slots[1] = mon(9, 40);
  party.slots[2] = mon(7, 20);   // SQUIRTLE: not final
  party.slots[3] = mon(3, 45);   // VENUSAUR
  for (uint8_t r = 0; r < GYM_REGIONS; r++) pet.winBadge(r, TRAINER_COUNT - 1, false);
  pet.wallet = 500000;
  toPlaza();
  tapPlaza(3);
  ck(!strcmp(scr(), "zone"), "(the zone)");
  tapRow(1);
  ck(zoneMode == 2 && !gaunt.active, "GAUNTLET opens its page");

  // --- 5. the custom rules screen --------------------------------------------------------
  tapRow(1);
  ck(zoneMode == 3, "CUSTOM opens the rules");
  for (int i = 0; i < 2; i++) tapRule(1);
  for (int i = 0; i < 3; i++) tapRule(2);
  for (int i = 0; i < 5; i++) tapRule(3);
  tapRule(0);
  ck(gauntCustom.levelCap == 50 && gauntCustom.tier == GT_FINAL && gauntCustom.size == 2 && gauntCustom.type == 0,
     "each cell cycles its rule: LV 50, FINAL, 2 ON 2, the first type");
  for (int i = 0; i < TYPE_COUNT; i++) tapRule(0);
  ck(gauntCustom.type == GAUNTLET_TYPE_ANY, "the type wraps back to ANY");
  for (int i = 0; i < 6; i++) tapRule(3);
  ck(gauntCustom.size == 2, "the team size wraps 1-6");
  onSwipeV(1);
  ck(zoneMode == 2, "down: back to the gauntlet page");
  tapRow(1);
  tapRule(4);
  ck(!strcmp(scr(), "pick") && pickTrainer == 0xFE, "START opens the team picker");

  // --- 6. the picker --------------------------------------------------------------------
  squadMask = 0;
  tapCandidate(3);
  ck(!(squadMask & 0x08), "a creature that breaks the rules cannot be picked (SQUIRTLE, not final)");
  tapCandidate(1);
  tapCandidate(0);
  ck(squadMask == 0, "nor can one over the level cap (BLASTOISE L60, MEWTWO L80): no scaling down");
  tapCandidate(2);
  uint32_t wallet0 = pet.wallet;
  tapFight();
  ck(!gaunt.active && !battleOpen && pet.wallet == wallet0, "one of two: FIGHT refuses, nothing charged");
  tapCandidate(4);
  tapFight();
  ck(gaunt.active && battleOpen && btlZone, "two of two: the run starts with its first fight");
  ck(pet.wallet == wallet0 - ZONE_FEE[ZONE_GAUNTLET], "and the fee is paid");
  ck(zoneFoe.count == 2 && zoneFoe.level[0] == 50 - GAUNTLET_LEVEL_RAMP, "the challenger brings two, under the cap");
  ck(btlSquadN == 2 && btlSquad[0].level == 40 && btlSquad[1].level == 45, "ours at their own levels");
  ck(!btlHard, "the first challenger plays the plain AI");
  ck(gaunt.fighting == 1 && nvs().count("gnt") && nvs()["gnt"][3] == 1, "the run is saved as fighting before the battle");

  // --- 7. a win, saved -----------------------------------------------------------------------
  uint16_t bp0 = pet.bp;
  fightOut(true);
  ck(!battleOpen && !strcmp(scr(), "zone") && zoneMode == 2, "a win comes back to the gauntlet page");
  ck(gaunt.wins == 1 && !gaunt.fighting && pet.bp == bp0 + gauntletBp(0, false), "one win, paid 1x (custom rules)");
  uint8_t ppSaved = gaunt.pp[0][0];
  ck(ppSaved < btlSquad[0].pp[0] + 1 && ppSaved < 35, "the PP it used is saved with the run");
  { GauntletSave before = gaunt;
    gaunt = GauntletSave();
    gauntLoad();
    ck(gaunt.active && gaunt.wins == 1 && !memcmp(&gaunt, &before, sizeof(gaunt)), "a reboot between battles keeps the run as it was"); }

  // --- 8. CONTINUE: the next challenger, the PP carried ------------------------------------
  ZoneFoe expect;
  gauntletChallenger(gaunt.rules, 1, gaunt.seed, 50, AVATARS_N, expect);
  tapRow(0);
  ck(battleOpen && !memcmp(zoneFoe.dex, expect.dex, sizeof(expect.dex)), "CONTINUE meets the challenger the seed says");
  ck(btlSquad[0].pp[0] == ppSaved, "with the PP the run had left");

  // --- 9. a reboot mid-battle is the loss -----------------------------------------------------
  pet.zoneBestStreak = 0;
  resetBattle(); zoneRun.active = false;
  gauntLoad();
  ck(!gaunt.active && pet.zoneBestStreak == 1, "a reboot mid-battle ends the run; its streak is the record");

  // --- 10. a team that broke the rules ------------------------------------------------------
  zoneOpen = true; zoneMode = 2;   // a fight closes the zone; back on its page
  ck(gauntletBegin(gauntCustom, false, 0x14), "(a new run)");
  party.slots[1] = PartyMon();          // BLASTOISE released
  tapRow(0);
  ck(!battleOpen && !gaunt.active && gauntBroken, "a member gone: CONTINUE ends the run and says why");
  party.slots[1] = mon(9, 40);
  ck(gauntletBegin(gauntCustom, false, 0x14), "(another)");
  party.slots[1].dex = 8;               // now a WARTORTLE: not final
  ck(!gauntletNextFight() && !gaunt.active, "a member no longer fitting the rules ends it too");
  party.slots[1].dex = 9;
  ck(gauntletBegin(gauntCustom, false, 0x14), "(and another)");
  party.slots[3].level = 51;            // grew past the cap
  ck(!gauntletNextFight() && !gaunt.active, "and so does one over the level cap");
  party.slots[3].level = 45;

  // --- 11. one at a time, and QUIT twice ------------------------------------------------------
  { uint32_t w = pet.wallet;
    ck(!gauntletBegin(gauntCustom, false, 0x12) && !gaunt.active && pet.wallet == w,
       "the start refuses one over the cap too (BLASTOISE L60), nothing charged"); }
  ck(gauntletBegin(gauntCustom, false, 0x14), "(a run)");
  ck(!gauntletBegin(gauntCustom, false, 0x14), "only one run at a time");
  tapRow(1);
  ck(gaunt.active && zoneMode == 2, "with a run on the page offers no new one");
  tapRow(2);
  ck(gaunt.active && gauntQuitArmed, "QUIT asks first");
  tapRow(2);
  ck(!gaunt.active, "and the second tap ends the run");

  // --- 12. the day's 2x -------------------------------------------------------------------
  pet.lastSeenEpoch = 21000UL * 86400 + 3600;
  GauntletRules d = gauntletDaily(pet.today());
  uint8_t placed = 0;
  for (int16_t s = 1; s <= DEX_COUNT && placed < d.size; s++)
    if (gauntletSpeciesFits(d, s) && speciesShowable(s) && DEX_TBL[s].rarity != R_LEGENDARIO) party.slots[placed++] = mon(s, d.levelCap ? d.levelCap : 70);
  uint16_t mask = 0;
  for (uint8_t i = 0; i < placed; i++) mask |= (uint16_t)(2 << i);
  ck(placed == d.size, "(a team for the day's rules)");
  ck(gauntletBegin(d, true, mask) && gauntletNextFight(), "a daily run");
  fightOut(false);
  ck(!gaunt.active && gaunt.claimDay != pet.today(), "lost its first battle: the day's 2x is still unclaimed");
  ck(gauntletBegin(d, true, mask) && gauntletNextFight(), "the next daily run");
  bp0 = pet.bp;
  fightOut(true);
  ck(gaunt.doubled && gaunt.claimDay == pet.today() && pet.bp == bp0 + gauntletBp(0, true), "its first win claims the 2x");
  pet.lastSeenEpoch += 2 * 86400;       // two days later
  bp0 = pet.bp;
  gauntletNextFight(); fightOut(true);
  ck(pet.bp == bp0 + gauntletBp(1, true), "and it keeps the 2x for its whole life, days later");
  gauntletNextFight(); fightOut(false);
  ck(!gaunt.active && gaunt.bestToday == 2, "its two wins are the day's best");
  pet.lastSeenEpoch -= 2 * 86400;
  ck(gauntletBegin(d, true, mask) && gauntletNextFight(), "another run of the same day's challenge");
  bp0 = pet.bp;
  fightOut(true);
  ck(!gaunt.doubled && pet.bp == bp0 + gauntletBp(0, false), "pays 1x: the day's 2x is taken");
  tapRow(2); tapRow(2);
  ck(!gaunt.active && pet.zoneBestStreak == 2, "the best streak is the longest run");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
