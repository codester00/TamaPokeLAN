// The weekday on the clock screen (v4.12). Only the time could be set before,
// so a board's weekday was wherever its first boot's seed date (a Thursday)
// had drifted to -- and the one-offs that keep a weekday (Lapras on Fridays)
// came on whatever day that happened to be. The screen shows the day; tapping
// its right half steps on, its left half back; SET moves the date to the
// nearest day that is that weekday (three days either way), keeping the time.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "wild.h"
#include <cstdio>
#include <cstring>
#include <string>
uint32_t g_seed = 0xDA7; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void openClock();
void clockTap(int16_t x, int16_t y);
void clockDayRect(int *x, int *y, int *w, int *h);
extern Pet pet;
extern bool clockOpen;
extern uint8_t clockWd;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static std::string drawn() { std::string log; Arduino_Canvas::textLog = &log; render(); Arduino_Canvas::textLog = nullptr; return log; }
static void tapDay(bool on) { int x, y, w, h; clockDayRect(&x, &y, &w, &h); clockTap((int16_t)(on ? x + w - 20 : x + 20), (int16_t)(y + h / 2)); }
static void tapSet() { clockTap(233, 364); }

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  const uint32_t FRI = 20763;                       // 2026-11-06, a Friday
  ck(Pet::weekdayOfDay(FRI) == 5 && Pet::weekdayOfDay(0) == 4, "(the weekday rule: 1970-01-01 a Thursday, 2026-11-06 a Friday)");
  pet.lastSeenEpoch = FRI * 86400UL + 9 * 3600 + 30 * 60;   // Friday 09:30
  openClock();
  ck(clockOpen && clockWd == 5, "the clock screen opens on the board's weekday");
  ck(drawn().find("DAY FRI") != std::string::npos, "and shows it");

  tapDay(true);
  ck(clockWd == 6 && drawn().find("DAY SAT") != std::string::npos, "the right half steps on");
  tapSet();
  ck(!clockOpen && pet.today() == FRI + 1, "SET moves the date to that day");
  ck((pet.lastSeenEpoch / 3600) % 24 == 9 && (pet.lastSeenEpoch / 60) % 60 == 30, "and keeps the time");
  ck(wildWeekday(pet) == 6, "the one-offs' weekday follows");

  openClock();
  tapDay(false); tapDay(false); tapDay(false);       // Sat -> Wed
  ck(clockWd == 3, "the left half steps back");
  tapSet();
  ck(pet.today() == FRI + 1 - 3, "to the NEAREST such day: three back rather than four on");
  openClock();
  for (int i = 0; i < 7; i++) tapDay(true);
  ck(clockWd == 3, "seven steps come round to the same day");
  tapSet();
  ck(pet.today() == FRI - 2, "and SET then leaves the date where it was");

  // the Friday one-off comes on the board's Friday
  pet.lastSeenEpoch = FRI * 86400UL + 12 * 3600;
  int fridayOnly = -1;
  for (uint8_t i = 0; i < wildFixedCount(); i++) {
    int days = 0;
    for (uint32_t d = 0; d < 7; d++) { pet.lastSeenEpoch = (FRI + d) * 86400UL + 12 * 3600; days += wildFixedToday(i, pet) ? 1 : 0; }
    pet.lastSeenEpoch = FRI * 86400UL + 12 * 3600;
    if (days == 1 && wildFixedToday(i, pet)) fridayOnly = i;
  }
  ck(fridayOnly >= 0, "a one-off that comes only on Fridays (Lapras) is there today");
  openClock();
  tapDay(true);                                     // make it Saturday
  tapSet();
  ck(fridayOnly >= 0 && !wildFixedToday((uint8_t)fridayOnly, pet), "and gone once the board is told it is Saturday");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
