// Safari encounters (v3.75): Kanto's SAFARI ZONE is met with no team, 30 Safari
// Balls, BAIT and ROCK, under FireRed's own catch and flee rules
// (docs/explore-specialty-build-plan.md § 1).
//
// The rules are pinned at the boundaries pokefirered sets them (the factors'
// floor and cap, the 2..6 counters, x2 and /4 on the flee chance, the reset
// when a rock wears off) and the ball against its arithmetic worked by hand.
// The battle is then driven through the sketch's own start and tap handlers,
// and the city fishing that moved out of the Safari areas is checked in the
// generated tables.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "items.h"
#include "wild.h"
#include "pp.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x5AFA;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
bool startWildBattle(uint8_t region, bool hard);
int btlCellIndexAt(int16_t x, int16_t y);
const char *btlMainOpt(uint8_t i, bool *live);
extern Pet pet;
extern Party party;
extern Inventory bag;
extern bool battleOpen, btlOver, btlWon, btlSafari, btlPetIn, btlWild, wildCaught;
extern uint8_t btlMenu, btlMsgCount, btlSquadN;
extern char btlMsg[8][40];
extern Combatant btlYou, btlFoe;
extern SafariState btlSafariSt;
extern uint16_t exploreAreaPick[];
extern int16_t wildDex;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
static void tapCell(int i) { int16_t x = 0, y = 0; cellPoint(i, x, y); onTap(x, y); }
static void drain() { for (int t = 0; t < 20 && btlMsgCount; t++) onTap(233, 233); }
static bool sawLine(const char *needle) {
  for (uint8_t i = 0; i < btlMsgCount; i++) if (strstr(btlMsg[i], needle)) return true;
  return false;
}

static bool areaHas(uint16_t area, int16_t dex) {
  for (uint32_t r = 0; r < 20000; r++)
    if (wildAreaPick(area, wildTimeBand(12), pet, r) == dex) return true;
  return false;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  pet.ageMinutes = 49 * MINUTES_PER_LEVEL;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  gRegionArt = 0xFFFF;

  // ---------------------------------------------------------------- the rules
  SafariState s;
  uint16_t pass4[4] = { 55186, 55186, 55186, 55186 };
  uint16_t fail1[4] = { 55187, 0, 0, 0 };
  safariBegin(s, 113);                    // CHANSEY: catch rate 30, FireRed flee rate 125
  ck(s.balls == SAFARI_BALLS && s.balls == 30, "an encounter starts with 30 Safari Balls");
  ck(s.catchFactor == 2, "Chansey's catch factor is 30 * 100 / 1275 = 2");
  ck(s.escapeFactor == 9, "Chansey's escape factor is 125 * 100 / 1275 = 9");
  ck(safariFleePct(s) == 45, "it flees 5 x 9 = 45% of turns untouched");
  safariBait(s, 0);
  ck(s.baitTurns == 2 && s.rockTurns == 0, "bait adds 2..6 turns (roll 0 = 2)");
  ck(s.catchFactor == 3, "bait halves the catch factor, floored at 3");
  ck(safariFleePct(s) == 10, "while eating it flees 5 x (9 / 4) = 10%");
  safariBait(s, 4);
  ck(s.baitTurns == 6, "bait counters stack to at most 6");
  ck(safariWatch(s) == SAFARI_EATING && s.baitTurns == 5, "a turn it stays is one off the bait");
  safariRock(s, 0);
  ck(s.baitTurns == 0 && s.rockTurns == 2, "a rock clears the bait and adds its own turns");
  ck(s.catchFactor == 6, "a rock doubles the catch factor");
  ck(safariFleePct(s) == 90, "while angry it flees 5 x (9 x 2) = 90%");
  ck(safariWatch(s) == SAFARI_ANGRY && s.rockTurns == 1, "a turn angry is one off the rock");
  ck(safariWatch(s) == SAFARI_WATCHING && s.rockTurns == 0, "the rock wears off");
  ck(s.catchFactor == 2, "and the catch factor returns to the species' own");

  safariBegin(s, 29);                     // NIDORAN F: 235, flee 50
  ck(s.catchFactor == 18 && s.escapeFactor == 3, "Nidoran F: factors 18 and 3");
  safariRock(s, 0);
  ck(s.catchFactor == SAFARI_FACTOR_MAX, "a rock caps the catch factor at 20");
  safariBegin(s, 29);
  safariBait(s, 0);
  ck(s.catchFactor == 9, "bait halves an 18 to 9 (above the floor)");
  safariBegin(s, 1);                      // BULBASAUR: no FireRed Safari flee rate
  ck(wildSafariFleeRate(1, SAFARI_FRLG) == 0 && s.escapeFactor == 2, "a species with no flee rate gets the floor of 2");
  ck(wildSafariFleeRate(147, SAFARI_FRLG) == 100 && wildSafariFleeRate(148, SAFARI_FRLG) == 125,
     "Dratini 100 and Dragonair 125, read from FireRed");
  s.escapeFactor = 20; s.rockTurns = 3;
  ck(safariFleePct(s) == 100, "the flee chance caps at 100%");

  // The ball, at factor 20: catch rate 255, odds (255 x 15 / 10) / 3 = 127,
  // 16711680 / 127 = 131588, sqrt sqrt = 19, 1048560 / 19 = 55187 a shake.
  safariBegin(s, 29);
  s.catchFactor = 20;
  uint16_t fail3[4] = { 0, 0, 55187, 0 };
  ck(safariBallShakes(s, pass4) == 4, "four rolls under 55187 catch it");
  ck(safariBallShakes(s, fail1) == 0, "a first roll at 55187 breaks free at once");
  ck(safariBallShakes(s, fail3) == 2, "the shakes stop at the first failed roll");
  s.catchFactor = 0;
  ck(safariBallShakes(s, pass4) == 0, "a catch factor of 0 never catches (and never divides by zero)");

  // ------------------------------------------------- Emerald (v3.76)
  safariBegin(s, 203, SAFARI_EMERALD);    // GIRAFARIG: catch rate 60
  ck(s.escapeFactor == 3 && safariFleePct(s) == 15, "Emerald starts every species at escape factor 3: 15%");
  ck(s.catchFactor == 4, "and the catch factor from the catch rate (60 -> 4)");
  ck(safariBait(s, 0) == SAFARI_CURIOUS && s.escapeFactor == 1, "a first Pokeblock takes 3 off: 3 -> 1 (never 0)");
  ck(safariFleePct(s) == 5, "so it flees 5% of turns");
  safariBait(s, 0);
  ck(s.escapeFactor == 1 && s.blocks == 2, "at 1 a block takes nothing more");
  safariBegin(s, 203, SAFARI_EMERALD);
  s.escapeFactor = 10;
  safariBait(s, 0); safariBait(s, 0); safariBait(s, 0); safariBait(s, 0);
  ck(s.escapeFactor == 10 - 3 - 2 - 1 - 1 && s.blocks == 3, "blocks take 3, 2, 1, then 1 each");
  safariBegin(s, 203, SAFARI_EMERALD);
  ck(safariRock(s, 0) == SAFARI_CLOSER && s.catchFactor == 8 && s.escapeFactor == 7, "GO NEAR: catch +4, escape +4");
  safariRock(s, 0); safariRock(s, 0);
  ck(s.catchFactor == 8 + 3 + 2 && s.escapeFactor == 15, "then catch +3, +2 (escape +4 each)");
  ck(safariRock(s, 0) == SAFARI_NO_CLOSER && s.catchFactor == 14 && s.escapeFactor == 19,
     "the fourth can't get closer, and still adds 1 (the game's table, counter at 3)");
  ck(safariRock(s, 0) == SAFARI_NO_CLOSER && s.catchFactor == 15 && s.escapeFactor == 20,
     "and so on, the escape factor capped at 20");
  ck(safariWatch(s) == SAFARI_WATCHING && s.escapeFactor == 20, "Emerald's foe only watches");

  // ------------------------------------------------ Platinum (v3.76)
  safariBegin(s, 194, SAFARI_PLATINUM);   // WOOPER: catch 255, Platinum flee rate 120
  ck(wildSafariFleeRate(194, SAFARI_PLATINUM) == 120, "Wooper's Platinum flee rate is 120");
  ck(s.catchStage == 6 && s.escapeStage == 6, "both stages start at 6 (x1)");
  ck(safariFlees(s, 120) && !safariFlees(s, 121), "it flees when a roll % 255 is at most 120");
  ck(safariFlees(s, 255) && !safariFlees(s, 255 + 121), "(the roll is taken % 255)");
  ck(safariBait(s, 1) == SAFARI_EATING && s.catchStage == 7 && s.escapeStage == 7,
     "BAIT: catch stage up, and escape stage up (roll % 10 != 0)");
  ck(safariFlees(s, 180) && !safariFlees(s, 181), "at escape stage 7 the rate is 120 x 1.5 = 180");
  safariBait(s, 0);
  ck(s.catchStage == 8 && s.escapeStage == 7, "a bait on a 1-in-10 roll leaves the escape stage alone");
  ck(safariRock(s, 3) == SAFARI_ANGRY && s.escapeStage == 6 && s.catchStage == 7,
     "MUD: escape stage down, and catch stage down (roll % 10 != 0)");
  safariRock(s, 0);
  ck(s.escapeStage == 5 && s.catchStage == 7, "a mud on a 1-in-10 roll leaves the catch stage alone");
  ck(safariFlees(s, 80) && !safariFlees(s, 81), "at escape stage 5 the rate is 120 / 1.5 = 80");
  s.escapeStage = 12;
  ck(safariFlees(s, 254), "at stage 12 (x4: 480) every roll flees");
  for (int i = 0; i < 20; i++) safariRock(s, 1);
  ck(s.escapeStage == 0 && s.catchStage == 0, "both stages floor at 0");
  for (int i = 0; i < 20; i++) safariBait(s, 1);
  ck(s.escapeStage == 12 && s.catchStage == 12, "and cap at 12");
  // the ball: Wooper's 255 at stage 6 is (255 x 15/10) / 3 = 127 -> 55187 a shake;
  // at stage 0, 255 x 10/40 = 63 -> 31 -> 1048560 / sqrt(sqrt(539086)) = 38835
  s.catchStage = 6;
  ck(safariBallShakes(s, pass4) == 4 && safariBallShakes(s, fail1) == 0, "stage 6 shakes at 55187, as Gen 3");
  s.catchStage = 0;
  uint16_t at38834[4] = { 38834, 38834, 38834, 38834 };
  uint16_t at38835[4] = { 38835, 0, 0, 0 };
  ck(safariBallShakes(s, at38834) == 4 && safariBallShakes(s, at38835) == 0, "stage 0 shakes under 38835");
  s.catchStage = 12;
  ck(safariBallShakes(s, fail1) == 4, "stage 12 (x4) catches outright");
  ck(safariWatch(s) == SAFARI_WATCHING, "Platinum's foe only watches");

  // ------------------------------------------------------------- the areas
  uint16_t safari = wildAreaByName(0, "SAFARI ZONE");
  uint16_t r1 = wildAreaByName(0, "ROUTE 1");
  ck(safari != WILD_NO_AREA && wildAreaMode(safari) == WILD_MODE_SAFARI, "SAFARI ZONE is a Safari area");
  ck(wildAreaMode(r1) == WILD_MODE_NORMAL, "ROUTE 1 is not");
  ck(wildAreaMode(wildAreaByName(2, "SAFARI ZONE")) == WILD_MODE_SAFARI &&
     wildAreaMode(wildAreaByName(2, "SAFARI EXTENSION")) == WILD_MODE_SAFARI &&
     wildAreaMode(wildAreaByName(3, "GREAT MARSH")) == WILD_MODE_SAFARI,
     "Hoenn's Safari Zone and Extension and Sinnoh's Great Marsh are Safari areas (v3.76)");
  ck(wildSafariRules(safari) == SAFARI_FRLG && wildSafariRules(wildAreaByName(2, "SAFARI ZONE")) == SAFARI_EMERALD &&
     wildSafariRules(wildAreaByName(3, "GREAT MARSH")) == SAFARI_PLATINUM,
     "each plays by its own game's rules");
  pet.badges = 0xFF;                       // every Kanto gate open
  ck(!areaHas(safari, 130), "Fuchsia's Gyarados is no longer met in the Safari Zone");
  ck(areaHas(safari, 147), "the Safari Zone keeps its own Dratini");
  ck(areaHas(wildAreaByName(0, "ROUTES 12-15"), 54), "Fuchsia's Psyduck is met on Routes 12-15");
  uint16_t marsh = wildAreaByName(3, "GREAT MARSH"), r213 = wildAreaByName(3, "ROUTE 213");
  pet.badgesX[3 - 1] = 0xFF;               // Sinnoh's gates too
  ck(!areaHas(marsh, 72), "Pastoria's Tentacool is no longer met in the Great Marsh");
  ck(areaHas(r213, 72), "Route 213 keeps it (Pastoria's water is the route's)");

  // ---------------------------------------------------------- the battle
  // a Route 1 fight builds a squad: the probe below can tell the two apart
  exploreAreaPick[0] = (uint16_t)(r1 + 1);
  ck(startWildBattle(0, false) && btlSquadN > 0 && !btlSafari, "an ordinary area builds a team");
  battleOpen = false;

  bag.clear();
  bag.add(IT_POKEBALL, 5);
  exploreAreaPick[0] = (uint16_t)(safari + 1);
  uint8_t energy0 = pet.energy;
  ck(startWildBattle(0, false), "the Safari Zone starts an encounter");
  ck(btlSafari && btlWild, "it is a Safari encounter");
  ck(btlSquadN == 0 && !btlPetIn, "no team is built");
  ck(btlSafariSt.balls == SAFARI_BALLS, "with 30 Safari Balls");
  ck(pet.energy == energy0, "and it costs the live pet no energy");
  bool live;
  ck(!strcmp(btlMainOpt(1, &live), "BAIT") && live, "the menu offers BAIT");
  ck(!strcmp(btlMainOpt(2, &live), "ROCK") && live, "and ROCK");
  ck(strstr(btlMainOpt(0, &live), "BALL x30") && live, "and BALL x30");
  render();                                // draws the trainer's side without a creature
  drain();

  // BAIT through the screen
  tapCell(1);
  ck(sawLine("BAIT"), "tapping BAIT throws bait");
  ck(btlSafariSt.baitTurns > 0 || btlOver, "the foe is eating (or fled)");
  drain();

  // throw balls until it is caught, flees or they run out
  uint8_t partyBefore = party.count();
  int throws = 0;
  for (int t = 0; t < 40 && battleOpen && !btlOver; t++) {
    uint8_t before = btlSafariSt.balls;
    tapCell(0);
    if (btlSafariSt.balls == before - 1) throws++;
    if (btlOver) break;
    drain();
  }
  ck(throws > 0, "tapping BALL throws a Safari Ball");
  // a catch may DROP Poke Balls (every catch grants its drops), so the check is
  // that a throw never takes one: equal unless something was caught
  ck(wildCaught ? bag.count(IT_POKEBALL) >= 5 : bag.count(IT_POKEBALL) == 5,
     "Safari Balls never come out of the bag");
  ck(pet.energy == energy0, "still no energy spent");
  ck(btlOver, "the encounter ends (caught, fled or out of balls)");
  if (wildCaught) ck(party.count() == partyBefore + 1 || pet.endedKind != CER_NONE,
                     "a catch goes where every catch goes");
  drain();
  drain();
  ck(!battleOpen && !btlSafari, "dismissing the last line closes it and clears the Safari state");

  // RUN always works
  exploreAreaPick[0] = (uint16_t)(safari + 1);
  ck(startWildBattle(0, false) && btlSafari, "another Safari encounter");
  drain();
  tapCell(3);
  ck(btlOver && !btlWon && !wildCaught, "RUN leaves at once");
  drain();
  ck(!battleOpen, "and closes the fight");

  // the screen runs the flee roll: at a factor of 20 it always flees
  ck(startWildBattle(0, false) && btlSafari, "a Safari encounter to flee from");
  drain();
  btlSafariSt.escapeFactor = 20;
  btlSafariSt.catchFactor = 0;             // so the ball cannot catch it first
  tapCell(0);
  ck(btlOver && !wildCaught && sawLine("fled"), "a certain flee ends the encounter after the throw");
  drain();
  ck(!battleOpen, "and closes it");

  // out of balls ends it
  ck(startWildBattle(0, false), "one more");
  drain();
  btlSafariSt.balls = 1;
  btlSafariSt.catchFactor = 0;             // cannot be caught
  btlSafariSt.escapeFactor = 0;            // and will not flee
  tapCell(0);
  ck(btlOver && sawLine("SAFARI BALLS"), "the last Safari Ball missed: the encounter is over");
  drain();
  bool deadBall;
  ck(startWildBattle(0, false), "and the next one starts with a fresh 30");
  btlMainOpt(0, &deadBall);
  ck(btlSafariSt.balls == 30 && deadBall, "BALL is usable again");
  btlSafariSt.balls = 0;
  btlMainOpt(0, &deadBall);
  ck(!deadBall, "with none left, BALL reads as unusable");
  battleOpen = false;

  // ---- Hoenn and Sinnoh through the screen (v3.76)
  pet.badgesX[2 - 1] = 0xFF;
  exploreAreaPick[2] = (uint16_t)(wildAreaByName(2, "SAFARI ZONE") + 1);
  ck(startWildBattle(2, false) && btlSafari && btlSafariSt.rules == SAFARI_EMERALD, "Hoenn's Safari Zone: Emerald's rules");
  ck(btlSquadN == 0, "no team in Hoenn's either");
  ck(!strcmp(btlMainOpt(1, &live), "POKEBLOCK") && !strcmp(btlMainOpt(2, &live), "GO NEAR"),
     "its menu reads POKEBLOCK and GO NEAR");
  drain();
  btlSafariSt.escapeFactor = 0;            // stays for the test
  tapCell(2);
  ck(sawLine("closer") && btlSafariSt.goNear == 1 && btlSafariSt.catchFactor > 0, "GO NEAR through the screen");
  drain();
  tapCell(1);
  ck(sawLine("POKEBLOCK") && btlSafariSt.blocks == 1, "POKEBLOCK through the screen");
  drain();
  tapCell(3); drain();
  ck(!battleOpen, "and RUN leaves");
  exploreAreaPick[3] = (uint16_t)(wildAreaByName(3, "GREAT MARSH") + 1);
  ck(startWildBattle(3, false) && btlSafari && btlSafariSt.rules == SAFARI_PLATINUM, "the Great Marsh: Platinum's rules");
  ck(!strcmp(btlMainOpt(1, &live), "BAIT") && !strcmp(btlMainOpt(2, &live), "MUD"), "its menu reads BAIT and MUD");
  drain();
  btlSafariSt.fleeRate = 0; btlSafariSt.escapeStage = 0;   // 0 x 0.25: only a roll of 0 flees
  uint8_t st0 = btlSafariSt.catchStage;
  tapCell(1);
  ck(btlSafariSt.catchStage == st0 + 1 || btlOver, "BAIT through the screen raises the catch stage");
  drain();
  if (battleOpen && !btlOver) { tapCell(2); ck(sawLine("MUD") || btlOver, "MUD through the screen"); drain(); }
  if (battleOpen) { tapCell(3); drain(); }
  ck(!battleOpen, "and it closes");

  printf(bad ? "\nsafari_test: %d FAILED\n" : "\nsafari_test: all passed\n", bad);
  return bad ? 1 : 0;
}
