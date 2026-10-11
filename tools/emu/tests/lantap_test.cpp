// The LAN BATTLE button on the gym chooser, as a finger presses it.
//
// Reported from a board: tapping LAN BATTLE often went straight back to the pet
// screen, and only a very deliberate tap worked. Two causes, both covered here:
//
//   1. One stray CST9217 reading ~90 px lower just before lift-off became the
//      gesture's END point, so the tap resolved as a swipe down -- which on the
//      chooser means "back to the pet".
//   2. Any tap below y=380 meant "back", and the 44 px button ended at 380, so
//      landing 1 px low left the screen.
//
// Drives the REAL setup()/loop()/handleTouch(), one poll per pump, through the
// same touch globals the SDL window pokes. Every position is asked of the
// firmware (lanBtnHit, rpickRowAt) rather than retyped here -- a test that
// keeps its own copy of a layout passes for the wrong reason (CLAUDE.md § 3).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <chrono>
#include <thread>
#include <cstdio>

uint32_t g_seed = 0x1A7;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void loop();
extern Pet pet;
extern bool gymOpen, gymPick, lanOpen;
extern uint8_t rpickPage;
bool lanBtnHit(int16_t x, int16_t y);
int rpickRowAt(int16_t x, int16_t y, uint8_t mode);
uint8_t rpickModeNow();
void uiTileGo(int i);

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

// handleTouch() polls at most every 20 ms of real time, so each pump is one poll.
static void pump(int n) {
  for (int i = 0; i < n; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    loop();
  }
}
static void at(int x, int y) {
  g_touchX = x; g_touchY = y; g_touchDown = true;
  emuFireInterrupt();
  pump(1);
}
static void lift() {
  g_touchDown = false;
  emuFireInterrupt();
  pump(3);
}

// A known state for every case: the gym chooser, page 0, nothing else open.
// Goes through the firmware's own tile navigation, which shuts every other
// screen -- setting the gym flags by hand left case 6's swipe destination open
// underneath, and case 7 then tested that screen instead (CLAUDE.md § 3: a
// state a test leaves behind is the next test's input). The gym tile's index
// is found by asking, not retyped.
static void onChooser() {
  lanOpen = false;
  for (int i = 0; i < 16; i++) {
    uiTileGo(i);
    if (gymOpen && gymPick) break;
  }
  pump(2);
}
static bool chooserUp() { return gymOpen && gymPick && !lanOpen && rpickModeNow() != 0xFF; }

int main() {
  setup();
  pump(4);
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pump(2);

  // Where the button is, asked of the firmware.
  int top = -1, bot = -1;
  for (int y = 250; y < 466; y++) if (lanBtnHit(233, y)) { if (top < 0) top = y; bot = y; }
  int cy = (top + bot) / 2;
  printf("      LAN button spans y %d..%d (%d px)\n", top, bot, bot - top);
  ck(top > 0 && bot - top >= 90, "the LAN button is finger-sized (9 mm)");

  onChooser();
  ck(chooserUp(), "(the gym chooser is the screen under test)");

  // 1. a clean tap
  at(233, cy); at(233, cy); lift();
  ck(lanOpen, "a clean tap opens LAN BATTLE");

  // 2. THE REPORT: a held tap whose last reading jumps 90 px down
  onChooser();
  at(233, cy - 30); at(233, cy - 30); at(233, cy - 29); at(233, cy + 60); lift();
  ck(lanOpen, "one stray reading 90 px low before lift-off is still a tap");
  ck(gymOpen || lanOpen, "and it did not leave for the pet screen");

  // 3. a sloppy tap that drifts 60 px -- under the jump limit, so the reading
  //    is genuine, but a finger on a button that wanders is still pressing it
  onChooser();
  at(233, cy - 30); at(233, cy - 10); at(233, cy + 30); lift();
  ck(lanOpen, "a tap that drifts 60 px on the button still presses it");

  // 4. landing just below the button: used to mean BACK, now does nothing
  onChooser();
  at(233, bot + 6); at(233, bot + 6); lift();
  ck(chooserUp(), "a tap just below the button does not leave the chooser");

  // 5. a deliberate swipe down from the button still goes back
  onChooser();
  at(233, top + 10); at(233, top + 40); at(233, top + 70); at(233, top + 100);
  at(233, top + 130); lift();
  ck(!gymOpen && !lanOpen, "a real swipe down that starts on the button still goes back");

  // 6. a fast flick from a region row -- big jumps between polls, as when the
  //    panel is busy drawing -- is still a swipe, not a tap on the row
  onChooser();
  int row = -1, ry = 0;
  for (int y = 100; y < 330 && row < 0; y++) if (rpickRowAt(233, y, rpickModeNow()) >= 0) { row = 0; ry = y + 20; }
  at(150, ry); at(250, ry); at(350, ry); lift();
  ck(!gymOpen, "a fast flick from a row is still a swipe along the tile axis");

  // 7. the stray mechanism must ENGAGE, not just be absent: a stray that
  //    comes back mid-gesture is ignored, and the tap lands
  onChooser();
  at(233, cy); at(233, cy + 120); at(233, cy + 1); at(233, cy + 1); lift();
  ck(lanOpen, "a stray that comes back mid-gesture is ignored");

  // 8. what ONLY the outlier filter catches: a finger creeping down a few px
  //    per poll is "moving consistently", so the on-control rule alone would
  //    let the 90 px stray complete a swipe. Held and never confirmed, it is
  //    dropped instead, and the tap lands.
  onChooser();
  at(233, cy - 30); at(233, cy - 25); at(233, cy - 20); at(233, cy - 15);
  at(233, cy + 75); lift();
  ck(lanOpen, "a slow creep then a stray is still a tap, not a swipe down");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
