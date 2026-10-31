// Which fights are indoors (v3.65): a gym leader in the gym, each Elite Four
// seat in its own room, the champion in his, a LAN fight in the Battle
// Frontier, and wild or one-off fights outdoors. Read off real battle starts
// and off the panel itself, not off btlIndoorScene() alone: a scene chosen and
// never drawn would pass a test that only asked the rule.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include "../../../backs_indoor.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x1d00;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void startBattle(int16_t dex, uint8_t lvl);
void startTrainerBattle(uint8_t idx, bool hard);
// v3.94: a League seat is fought inside a run; this puts one in place.
void leagueDbgAt(uint8_t region, bool hard, uint8_t seat); void leagueEnd(); extern uint8_t gymRegion;
bool startWildBattle(uint8_t region, bool hard);
int8_t btlIndoorScene();
int btlIndoorY();
extern Pet pet;
extern bool battleOpen;
extern int8_t btlSceneIndoor;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// The battle screen draws a 240x112 scene at 2x, from btlIndoorY(), centred.
// Row 2 is the stripe along the top of every room, clear of plates and sprites.
static int rowMatches(const BackScene &b) {
  const uint16_t *fb = gfx->buffer();
  int hits = 0;
  for (int sx = 120; sx < 346; sx += 2) {
    int bx = (sx - (233 - 240)) / 2;
    if (fb[(btlIndoorY() + 4) * 466 + sx] == b.pal[b.idx[2 * b.w + bx]]) hits++;
  }
  return hits;   // out of 113
}
static void closeFight() { for (int t = 0; t < 300 && battleOpen; t++) onTap(233, 233); }
static void ready() {
  if (pet.isEgg() || pet.speciesId != 150) pet.dbgHatchAs(150, false);
  pet.ageMinutes = 60UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  ready();
  for (uint8_t i = 0; i < TRAINER_COUNT; i++) pet.winBadge(0, i, false);

  // --- every ladder seat, through startTrainerBattle()
  static const int8_t want[TRAINER_COUNT] = {
    INDOOR_LEADER, INDOOR_LEADER, INDOOR_LEADER, INDOOR_LEADER,
    INDOOR_LEADER, INDOOR_LEADER, INDOOR_LEADER, INDOOR_LEADER,
    INDOOR_SIDNEY, INDOOR_PHOEBE, INDOOR_GLACIA, INDOOR_DRAKE, INDOOR_CHAMPION };
  bool seats = true;
  for (uint8_t i = 0; i < TRAINER_COUNT; i++) {
    ready();
    if (i >= TRAINER_COUNT - TRAINER_ELITE4 - 1) leagueDbgAt(gymRegion, i & 1, i); else leagueEnd();
    startTrainerBattle(i, i & 1);
    if (!battleOpen || btlSceneIndoor != want[i]) {
      printf("  seat %u: scene %d, want %d\n", i, btlSceneIndoor, want[i]);
      seats = false;
    }
    closeFight();
  }
  ck(seats, "gym leaders in the gym, each Elite Four seat its room, the champion his");

  // --- and the panel shows it
  ready();
  startTrainerBattle(0, false);
  render();
  int lead = rowMatches(INDOOR_BACKS[INDOOR_LEADER]);
  ck(lead == 113, "a gym leader's fight draws the gym on the panel");
  closeFight();
  ready();
  leagueDbgAt(gymRegion, false, TRAINER_COUNT - 1);
  startTrainerBattle(TRAINER_COUNT - 1, false);
  render();
  int champ = rowMatches(INDOOR_BACKS[INDOOR_CHAMPION]);
  ck(champ == 113 && rowMatches(INDOOR_BACKS[INDOOR_LEADER]) < 20,
     "the champion's fight draws his room, not the gym");
  closeFight();

  // --- outdoors: wild and one-off fights
  ready();
  ck(startWildBattle(0, false) && btlSceneIndoor < 0, "a wild fight stays outdoors");
  render();
  ck(rowMatches(INDOOR_BACKS[INDOOR_LEADER]) < 20, "and the panel shows the outdoors, not the gym");
  closeFight();
  ready();
  startBattle(10, 5);
  ck(battleOpen && btlSceneIndoor < 0, "a one-off fight stays outdoors");
  closeFight();

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
