// AI switching (v4.05, postgame-plan § 1 and step 3). The rule
// (aiSwitchTarget): a creature losing the matchup goes out for a teammate that
// does clearly better and does not take a big hit coming in; otherwise it
// stays. In a zone fight the opponent's creature is held in for its first
// AI_SWITCH_COOLDOWN turns, the switch is its turn, and only a challenger
// whose format turns switching on ever switches (the gauntlet's later rounds).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "zone.h"
#include "gauntlet.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0x5A1; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
bool zoneRunBegin(uint8_t kind, uint16_t mask);
bool zoneStartFight(const ZoneFoe &f);
void btlCellCenter(int i, int *x, int *y);
extern Pet pet;
extern bool battleOpen, btlZone;
extern uint8_t btlMenu, btlMsgCount, btlFoeAt;
extern Combatant btlYou, btlFoe;
extern char btlMsg[8][40];
extern ZoneRun zoneRun;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static Combatant mk(int16_t dex, uint16_t hp = 50) { Combatant c; c.dex = dex; c.level = 50; c.hp = c.maxHp = hp; return c; }
static void clearMsgs() { for (int i = 0; i < 60 && btlMsgCount && battleOpen; i++) battleTap(233, 300); }
static bool saidName(const char *n) {
  for (uint8_t i = 0; i < btlMsgCount; i++) if (strstr(btlMsg[i], n)) return true;
  return false;
}
// One turn: our GROWL; nobody faints, nothing charges.
static void turn() {
  btlYou.hp = btlYou.maxHp;
  btlFoe.charging = 0; btlFoe.recharge = false;
  btlMenu = 1;
  int x, y; btlCellCenter(0, &x, &y); battleTap(x, y);
}
static ZoneFoe foe(bool switching) {
  ZoneFoe f{};
  snprintf(f.name, sizeof(f.name), "%s", "ACE");
  f.count = 2; f.dex[0] = 6; f.dex[1] = 3;   // CHARIZARD, VENUSAUR
  f.level[0] = f.level[1] = 50; f.iv = 20; f.tr = 0;
  f.switching = switching ? 1 : 0;
  return f;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the rule -------------------------------------------------------------
  Combatant blastoise = mk(9);
  { Combatant t[2] = { mk(6), mk(3) };            // CHARIZARD out, VENUSAUR behind
    ck(aiSwitchTarget(t, 2, 0, blastoise) == 1, "a fire type facing water goes out for the grass type"); }
  { Combatant t[2] = { mk(3), mk(6) };
    ck(aiSwitchTarget(t, 2, 0, blastoise) == -1, "a creature winning its matchup stays"); }
  { Combatant t[2] = { mk(6), mk(76) };           // GOLEM: 4x weak to water
    ck(aiSwitchTarget(t, 2, 0, blastoise) == -1, "never into a creature that takes a big hit coming in"); }
  { Combatant t[2] = { mk(6), mk(3, 0) };         // VENUSAUR fainted
    ck(aiSwitchTarget(t, 2, 0, blastoise) == -1, "never to a fainted one"); }
  { Combatant t[3] = { mk(6), mk(19), mk(3) };    // RATTATA (+150) and VENUSAUR (+150 and more)
    ck(aiSwitchTarget(t, 3, 0, blastoise) == 2, "the best matchup of the ones that qualify"); }
  ck(battleMatchScore(3, 9) > 0 && battleMatchScore(6, 9) < 0, "(the matchup scores)");
  { Combatant t[2] = { mk(19), mk(3) };           // RATTATA, neutral against water
    ck(aiSwitchTarget(t, 2, 0, blastoise) == -1, "a neutral matchup stays, though a teammate would do better"); }
  { Combatant charizard = mk(6);
    Combatant t[2] = { mk(3), mk(81) };            // VENUSAUR out; MAGNEMITE beats fire/flying but takes 2x fire
    ck(battleMatchScore(81, 6) >= battleMatchScore(3, 6) + AI_SWITCH_MARGIN,
       "(MAGNEMITE clears the margin...)");
    ck(aiSwitchTarget(t, 2, 0, charizard) == -1, "...but takes a big hit coming in: no switch"); }

  // --- 2. who switches ---------------------------------------------------------
  ck(!gauntletSwitches(10) && gauntletSwitches(11), "the gauntlet switches from the twelfth challenger");
  { GauntletRules r; ZoneFoe a, b;
    gauntletChallenger(r, 10, 77, 50, 19, a); gauntletChallenger(r, 11, 77, 50, 19, b);
    ck(!a.switching && b.switching, "and its challengers carry it"); }

  // --- 3. in a zone fight ----------------------------------------------------------
  pet.dbgHatchAs(9, false);                      // BLASTOISE
  pet.ageMinutes = 49UL * MINUTES_PER_LEVEL;
  memset(pet.moves, 0, sizeof(pet.moves));
  pet.moves[0] = MV_GROWL;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  for (uint8_t r = 0; r < GYM_REGIONS; r++) pet.winBadge(r, TRAINER_COUNT - 1, false);
  squadMask = 0x01;
  pet.wallet = 100000;
  ck(zoneRunBegin(ZONE_GAUNTLET, squadMask), "(a zone run)");

  ck(zoneStartFight(foe(false)) && battleOpen && btlFoe.dex == 6, "(CHARIZARD leads, switching off)");
  bool stayed = true;
  for (int t = 0; t < 12; t++) { turn(); clearMsgs(); if (btlFoeAt != 0) stayed = false; }
  ck(stayed, "a challenger without switching never switches, however bad the matchup");
  battleOpen = false; btlZone = false;

  ck(zoneStartFight(foe(true)) && btlFoe.dex == 6, "(CHARIZARD leads, switching on)");
  bool held = true;
  for (int t = 0; t < AI_SWITCH_COOLDOWN; t++) { turn(); clearMsgs(); if (btlFoeAt != 0) held = false; }
  ck(held, "held in for its first turns");
  uint16_t youHp = btlYou.maxHp;
  turn();
  ck(btlFoeAt == 1 && btlFoe.dex == 3, "then it switches out to VENUSAUR");
  ck(saidName("VENUSAUR") && saidName("ACE") && !saidName("CHARIZARD used"), "and says so, and CHARIZARD does not attack");
  ck(btlYou.hp == youHp, "the switch is its turn: it does not attack too");
  clearMsgs();
  bool back = false;
  for (int t = 0; t < 12; t++) { turn(); clearMsgs(); if (btlFoeAt != 1) back = true; }
  ck(!back, "VENUSAUR wins the matchup: it stays");
  battleOpen = false; btlZone = false;

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
