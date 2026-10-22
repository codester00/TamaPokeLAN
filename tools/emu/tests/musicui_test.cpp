// Which music the firmware asks for (v3.56): each kind of fight its own theme,
// each kind of win its own jingle, and the halls' music following the screen
// however the hall is left. The sounds themselves are music_test's; this is the
// sketch choosing them (tools/emu/host_impl.cpp records the last request).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "audio.h"
#include "gamecorner.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x3051;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void startBattle(int16_t dex, uint8_t lvl);
void startTrainerBattle(uint8_t idx, bool hard);
bool startWildBattle(uint8_t region, bool hard);
int btlCellIndexAt(int16_t x, int16_t y);
void openHall(uint8_t h);
void hallBack();
void uiTileGo(int i);
uint8_t emuMusic();
extern Pet pet;
extern bool battleOpen, btlOver, btlWon;
extern uint8_t btlMsgCount, hallOpen;
extern Combatant btlYou;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// Tap the first move, dismissing messages, until the fight is decided (NOT
// closed: the win's jingle plays while its messages are still up).
static void playToEnd() {
  int16_t mx = 0, my = 0;
  for (int16_t py = 240; py < 466 && !mx; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == 0) { mx = px + 8; my = py + 8; break; }
  for (int t = 0; t < 600 && battleOpen && !btlOver; t++) {
    if (btlMsgCount) onTap(233, 233);
    else onTap(mx, my);
  }
}
static void closeFight() { for (int t = 0; t < 200 && battleOpen; t++) onTap(233, 233); }
static void strong() {
  if (pet.isEgg() || pet.speciesId != 150) pet.dbgHatchAs(150, false);   // MEWTWO
  pet.ageMinutes = 99UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  strong();

  // --- a one-off fight is a trainer; its win the trainer's jingle
  startBattle(10, 2);
  ck(battleOpen && emuMusic() == MUS_TRAINER, "a one-off fight plays the trainer battle");
  playToEnd();
  ck(btlWon && emuMusic() == MUS_WIN_TRAINER, "and a win there the trainer's victory");
  closeFight();

  // --- the ladder: a gym leader, then the champion's own theme
  for (uint8_t i = 0; i < TRAINER_COUNT; i++) pet.winBadge(0, i, false);
  startTrainerBattle(0, false);
  ck(battleOpen && emuMusic() == MUS_GYM, "a gym leader plays the gym leader battle");
  playToEnd();
  ck(btlWon && emuMusic() == MUS_WIN_GYM, "and beating one the gym leader's fanfare");
  closeFight();
  strong();
  startTrainerBattle(TRAINER_COUNT - 2, false);
  ck(battleOpen && emuMusic() == MUS_GYM, "the Elite Four fight to the gym leader battle, as in Red");
  closeFight();
  strong();
  startTrainerBattle(TRAINER_COUNT - 1, false);
  ck(battleOpen && emuMusic() == MUS_FINAL, "the champion has the final battle");
  closeFight();

  // --- a wild fight, and its win
  strong();
  bool started = startWildBattle(0, false);
  ck(started && battleOpen && emuMusic() == MUS_WILD, "a wild creature plays the wild battle");
  playToEnd();
  ck(btlWon && emuMusic() == MUS_WIN_WILD, "and beating it the wild victory");
  closeFight();

  // --- the halls: their music follows the screen, however it is left
  openHall(HALL_CELADON); render();
  ck(emuMusic() == MUS_GAMECORNER, "a Game Corner plays its own music");
  hallBack(); render();
  ck(!hallOpen && emuMusic() == MUS_NONE, "and backing out stops it");
  openHall(HALL_GOLDENROD); render();
  ck(emuMusic() == MUS_GAMECORNER, "(Goldenrod's too)");
  uiTileGo(2); render();                       // straight off along the tile axis
  ck(!hallOpen && emuMusic() == MUS_NONE, "leaving along the tile axis stops it as well");
  openHall(HALL_BEACH); render();
  ck(emuMusic() == MUS_SURF, "the beach house plays Surfing Pikachu's own theme");
  hallBack(); render();
  ck(!hallOpen && emuMusic() == MUS_NONE, "and stops when the ride is left");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
