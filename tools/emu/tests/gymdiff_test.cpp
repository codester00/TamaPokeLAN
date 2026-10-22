// The gym list remembers its difficulty across a restart (v3.50). It used to
// open on EASY every boot, and a player whose badges were all on HARD read the
// empty list as lost badges after an update.
//
// The pill is tapped through the sketch's own tap handler, then the boot path
// (setup()) is run again against the same NVS, as a restart would.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x6D1F;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
extern bool gymOpen, gymPick, gymHard;
extern Pet pet;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

int main() {
  setup();
  pet.dbgHatchAs(4, false);                  // past the starter screen
  ck(!gymHard, "a fresh save opens the gym list on EASY");

  gymOpen = true; gymPick = false;            // a ladder is open
  onTap(233, 60 + 20);                        // the difficulty pill
  ck(gymHard, "tapping the pill switches the list to HARD");

  gymHard = false;                            // RAM forgets, as a reboot does
  setup();
  ck(gymHard, "after a restart the list is still on HARD");
  ck(nvs().count("ghard") == 1, "the choice is stored under its own key");

  gymOpen = true; gymPick = false;
  onTap(233, 60 + 20);
  gymHard = true;
  setup();
  ck(!gymHard, "and switching back to EASY is remembered too");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
