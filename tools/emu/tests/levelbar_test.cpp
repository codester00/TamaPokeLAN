// The card's level bar at level 100 (v3.72): full, reading MAX LEVEL. It used
// to count down "44 min to Lv.101" -- a level that never comes.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "i18n.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x1E7;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void uiNextLevelText(char *buf, size_t n, uint8_t *into);
extern Pet pet;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  char buf[32];
  uint8_t into = 0;

  pet.ageMinutes = (uint32_t)(50 - 1) * MINUTES_PER_LEVEL + 16;   // L50, 16 min in
  uiNextLevelText(buf, sizeof(buf), &into);
  printf("      L50: \"%s\" into=%u\n", buf, into);
  ck(into == 16 && strstr(buf, "51") && !strstr(buf, T(S_MAX_LEVEL)),
     "below 100 the bar counts to the next level");

  pet.ageMinutes = (uint32_t)(MAX_LEVEL - 1) * MINUTES_PER_LEVEL + 16;  // L100, 16 min in
  uiNextLevelText(buf, sizeof(buf), &into);
  printf("      L100: \"%s\" into=%u\n", buf, into);
  ck(pet.level() == MAX_LEVEL, "(the creature is level 100)");
  ck(!strcmp(buf, T(S_MAX_LEVEL)), "at 100 the bar reads MAX LEVEL");
  ck(!strstr(buf, "101"), "and never names level 101");
  ck(into == MINUTES_PER_LEVEL, "and is full");

  pet.ageMinutes += 30UL * 24 * 60;   // a month later, still 100
  uiNextLevelText(buf, sizeof(buf), &into);
  ck(!strcmp(buf, T(S_MAX_LEVEL)) && into == MINUTES_PER_LEVEL, "and stays so");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
