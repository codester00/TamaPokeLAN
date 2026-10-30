// v3.60: the hidden CHEATS, the trainer's cheats mark and the creature mark.
// Each cheat goes through cheatApply(), the one place a cheat acts; the marks
// must be permanent across a reload, and the creature mark must survive
// banking, swapping back, evolving, a trade and the LAN wire.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "link.h"
#include "trade.h"
#include "inventory.h"
#include "trainers.h"
#include "wild.h"
#include "codes.h"
#include "cheats.h"
#include <cstdio>
uint32_t g_seed = 60; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void codeApply(uint8_t id);
void startSimon();
void simonBerryPos(uint8_t i, int *x, int *y);
void mgPress(int16_t x, int16_t y, uint32_t now);
void onRim(int dir);
void onTap(int16_t x, int16_t y);
void render();
bool cheatApply(uint8_t id);
void startTrainerBattle(uint8_t idx, bool hard);
bool startWildBattle(uint8_t region, bool hard);
extern Pet pet;
extern bool codesOpen, codesCheats, battleOpen, cheatLegendNext;
extern uint16_t squadMask;
extern Combatant btlSquad[], btlFoe;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(4, false);
  pet.ageMinutes = 20UL * MINUTES_PER_LEVEL;

  // Entering the code and looking around the menu marks nothing: only a cheat
  // that actually runs does (owner, 2026-10-24).
  startSimon();
  for (int i = 0; i < CODE_LEN; i++) {
    int x, y; simonBerryPos(CODES[CODE_CHEATS].seq[i], &x, &y); mgPress(x, y, millis());
  }
  ck(codesOpen && codesCheats, "the CHEATS code, tapped in Memory, opens the cheat list");
  for (int i = 0; i < 5; i++) { onRim(1); render(); }
  for (int i = 0; i < 5; i++) { onRim(-1); render(); }
  onTap(233, 30);                       // the title, not a row
  ck(codesOpen && pet.cheatsUsed == 0 && pet.rulesUsed == 0 && !pet.cheated,
     "opening, paging and missing every row marks nothing");
  codesOpen = false;
  codeApply(CODE_SETTINGS);
  for (int i = 0; i < 5; i++) { onRim(1); render(); }
  ck(pet.rulesUsed == 0 && pet.cheatsUsed == 0, "nor does opening the hidden SETTINGS");
  codesOpen = false;
  codeApply(CODE_CHEATS);
  ck(codesOpen && codesCheats, "CHEATS opens the cheat list");
  ck(!cheatApply(CHEAT_HATCH) && pet.cheatsUsed == 0, "a cheat that does nothing (no egg) marks nothing");
  cheatApply(CHEAT_COINS);
  ck(pet.cheatsUsed == (1ul << CHEAT_COINS), "the first cheat used sets the mark, and only its bit");

  // --- the grants
  uint16_t c0 = pet.coins; cheatApply(CHEAT_COINS);
  ck(pet.coins == c0 + 1000, "+1,000 coins");
  uint8_t mb = bag.count(IT_MASTERBALL); cheatApply(CHEAT_MASTERBALL);
  ck(bag.count(IT_MASTERBALL) == mb + 1, "+1 Master Ball");
  uint32_t w0 = pet.wallet; cheatApply(CHEAT_MONEY);
  ck(pet.wallet == w0 + 100000, "+$100,000");
  ck(!pet.cheated, "money, coins and balls mark the trainer only");
  cheatApply(CHEAT_ALL_TMS);
  ck(pet.tmOwnedCount() == TM_COUNT, "every TM is owned");
  cheatApply(CHEAT_ALL_BADGES);
  bool all = true;
  for (uint8_t rg = 0; rg < GYM_REGIONS; rg++)
    for (uint8_t i = 0; i < TRAINER_COUNT; i++)
      all = all && pet.hasBadge(rg, i, false) && pet.hasBadge(rg, i, true);
  ck(all, "every badge, both difficulties");

  // --- the toggles, and none of them in a LAN battle
  squadMask = 1;
  startTrainerBattle(0, false);
  ck(btlSquad[0].ppOn, "PP counts in a fight");
  battleOpen = false;
  cheatApply(CHEAT_UNLIMITED_PP);
  startTrainerBattle(0, false);
  ck(!btlSquad[0].ppOn, "unlimited PP: the player's side spends none");
  battleOpen = false;
  settingsSuspended = true;
  startTrainerBattle(0, false);
  ck(btlSquad[0].ppOn, "and never while a LAN battle is on");
  battleOpen = false;
  settingsSuspended = false;
  uint16_t last = wildAreaFirst(1) - 1;   // Kanto's last area, behind a late gate
  Pet blank;
  ck(!wildAreaUnlocked(last, blank), "a fresh trainer has the last area shut");
  cheatApply(CHEAT_ALL_AREAS);
  ck(wildAreaUnlocked(last, blank), "all areas open opens it");
  cheatApply(CHEAT_ALL_AREAS);
  ck(!wildAreaUnlocked(last, blank), "and the toggle turns back off");

  // --- the legendary
  cheatApply(CHEAT_LEGEND);
  ck(cheatLegendNext, "the legendary waits for the next encounter");
  bool met = startWildBattle(0, false);
  ck(met && wildIsExploreLegend(btlFoe.dex) && !cheatLegendNext, "and the next encounter is one");
  battleOpen = false;

  // --- the creature mark
  cheatApply(CHEAT_MAX_TRAINING);
  ck(pet.trAtk == pet.trMaxAtk() && pet.cheated, "max training marks the creature");
  PartyMon m = pet.toPartyMon();
  ck(m.flags & PMON_CHEATED, "banked, it keeps the mark");
  Pet q; q.switchTo(m);
  ck(q.cheated, "raised again, it still has it");
  q.ageMinutes = 40UL * MINUTES_PER_LEVEL;
  int16_t before = q.speciesId; q.evolve();
  ck(q.speciesId != before && q.cheated, "evolving keeps it");
  PartyMon t = q.toPartyMon();
  Pet::markTraded(t, 1);
  ck((t.flags & PMON_CHEATED) && tradeMonValid(t), "a trade carries it and accepts it");
  Combatant cb; combatantFromParty(cb, t);
  LinkMon lm; linkMonFrom(lm, cb);
  Combatant back; linkMonTo(back, lm);
  ck(back.cheated, "and so does the LAN wire");
  // 34 until v3.92 added ppUps (LINK_PROTO 10); the build fingerprint in the
  // hello carries this size, so two builds that disagree refuse to pair
  ck(sizeof(LinkMon) == 36, "LinkMon is the size both boards expect");

  // --- the egg cheats
  pet.dbgHatchAs(1, false);
  pet.newEgg();
  ck(pet.isEgg() && !pet.cheated, "a new egg starts unmarked");
  cheatApply(CHEAT_SHINY_EGG);
  cheatApply(CHEAT_HATCH);
  ck(!pet.isEgg() && pet.shiny && pet.cheated, "a shiny egg hatched now is shiny and marked");
  pet.newEgg(); pet.eggTap(); pet.eggTap(); pet.eggTap();
  ck(!pet.isEgg() && !pet.cheated, "the next egg, hatched by hand, is not");
  ck(!cheatApply(CHEAT_HATCH), "hatch with no egg does nothing");

  // --- the day
  uint8_t s0 = pet.daySalt; cheatApply(CHEAT_REROLL_DAY);
  ck(pet.daySalt == (uint8_t)(s0 + 1), "the day re-rolls");

  // --- the mark is permanent and every cheat used is remembered
  uint32_t used = pet.cheatsUsed;
  ck(used == ((1ul << CHEAT_COUNT) - 1), "every cheat used is recorded");
  pet.flushSave();
  { Pet r; r.begin(); ck(r.cheatsUsed == used && r.tmIsCheated(TM_MOVES[0]), "the mark survives a reload"); }
  {
    Preferences pr; pr.begin("tamapoke", false);
    pr.remove("plyA"); pr.remove("plyB"); pr.end();
    Pet r; r.begin();
    ck(r.cheatsUsed == used && r.tmIsCheated(TM_MOVES[0]), "and through the legacy keys alone");
  }

  // --- a move taught from a cheat's TM marks the creature
  pet.dbgHatchAs(4, false);
  pet.ageMinutes = 0;
  ck(!pet.cheated, "a fresh creature");
  uint8_t mv = 0;
  for (uint16_t i = 0; i < TM_COUNT && !mv; i++)
    if (moveNeedsCheatedTm(pet, 4, pet.level(), TM_MOVES[i])) mv = TM_MOVES[i];
  ck(mv != 0, "a TM Charmander could only learn from the cheat exists");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
