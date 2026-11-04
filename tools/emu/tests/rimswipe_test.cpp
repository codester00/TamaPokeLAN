// A swipe that starts on the rim and heads into the panel moves a tile; a drag
// ALONG the rim still pages (v4.00).
//
// Reported from a board: the Mart was hard to swipe past. A finger reaches past
// its item buttons from the right edge, and the whole ring outside RIM_R0 is
// the rim, so a right-to-left swipe begun there above or below the middle swept
// through the angle a scroll would and turned the Mart's page instead.
//
// Drives the REAL setup()/loop()/handleTouch(), one poll per pump, through the
// touch globals the SDL window pokes, like lantap_test.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdio>

uint32_t g_seed = 0x51A;
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
extern bool martOpen;
extern uint8_t martPage;
uint8_t martPages();
void uiTileGo(int i);
int uiTileIndex();
bool uiOnRim(int16_t x, int16_t y);

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}
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

static int martTile = -1;
static void onMart() {
  for (int i = 0; i < 16 && martTile < 0; i++) { uiTileGo(i); if (martOpen) martTile = i; }
  uiTileGo(martTile);
  martPage = 0;
  pump(2);
}

// A straight horizontal stroke, 20 px a poll.
static void stroke(int x0, int x1, int y) {
  int step = x1 > x0 ? 20 : -20;
  for (int x = x0; step > 0 ? x <= x1 : x >= x1; x += step) at(x, y);
  lift();
}

int main() {
  setup();
  pump(4);
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pump(2);

  onMart();
  ck(martOpen && martPages() > 1, "(the Mart is open and pages, so its rim scrolls)");

  // 1-2. THE REPORT: right to left from the right edge, below and above the
  //      middle. Both start on the rim.
  const int ys[2] = {340, 130};
  for (int k = 0; k < 2; k++) {
    onMart();
    int x0 = 233 + (int)sqrtf(215.0f * 215.0f - (float)((ys[k] - 233) * (ys[k] - 233)));
    ck(uiOnRim(x0, ys[k]), "(the stroke starts on the rim)");
    stroke(x0, x0 - 200, ys[k]);
    char m[96];
    snprintf(m, sizeof m, "a swipe left from the right rim at y %d leaves the Mart for the next tile", ys[k]);
    ck(!martOpen && uiTileIndex() == martTile + 1, m);
  }

  // 3. and the other way, from the left rim
  onMart();
  {
    int y = 340, x0 = 233 - (int)sqrtf(215.0f * 215.0f - 107.0f * 107.0f);
    stroke(x0, x0 + 200, y);
    ck(!martOpen && uiTileIndex() == martTile - 1, "a swipe right from the left rim goes to the tile before");
  }

  // 4. the mechanism must still ENGAGE: a drag along the rim pages, both ways
  onMart();
  for (int a = 40; a <= 110; a += 5) {
    float r = a * 3.14159265f / 180.0f;
    at(233 + (int)(212 * cosf(r)), 233 + (int)(212 * sinf(r)));
  }
  lift();
  ck(martOpen && martPage > 0, "a drag along the rim still pages the Mart");
  uint8_t after = martPage;
  for (int a = 110; a >= 40; a -= 5) {
    float r = a * 3.14159265f / 180.0f;
    at(233 + (int)(212 * cosf(r)), 233 + (int)(212 * sinf(r)));
  }
  lift();
  ck(martOpen && martPage < after, "and back along the rim pages back");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
