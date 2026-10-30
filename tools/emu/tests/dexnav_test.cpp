// The Pokedex's text and its BACK (v3.88). The grid's footer printed a buffer
// nothing had filled since v3.63 -- garbage glyphs on a board, nothing at all in
// the emulator, whose memory happened to be blank -- so this reads what the
// screen actually PRINTED. And every Pokedex screen now has BACK in the band
// under the grid, one level up, where the header used to close the whole
// Pokedex and the footer meant GO TO # instead.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "i18n.h"
#include <cstdio>
#include <cstring>
#include <string>

uint32_t g_seed = 0xDE7;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void loop();
void render();
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
extern Pet pet;
extern bool galleryOpen, galleryPick, galleryDirty, jumpOpen;
extern uint8_t galleryPage, galleryRegion;
extern int16_t galleryDetail;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const int CX = 233;
static const int BACK_Y = 84 + 4 * 80 + 20;   // in the band under the grid
static const int HEAD_Y = 50;                 // above the grid

static void grid(uint8_t page) {
  galleryOpen = true; galleryPick = false; galleryDetail = 0;
  galleryRegion = 0; galleryPage = page; galleryDirty = true; jumpOpen = false;
}
// What one frame of the current screen printed.
static std::string printed() {
  std::string log;
  Arduino_Canvas::textLog = &log;
  galleryDirty = true;
  render();                                   // one frame, whatever the dimmer says
  Arduino_Canvas::textLog = nullptr;
  return log;
}
static bool allPrintable(const std::string &s) {
  for (unsigned char c : s) if (c != '\n' && (c < 0x20 || c > 0x7E)) return false;
  return true;
}

int main() {
  emuVirtualClock(true);
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  emuAdvanceMs(60000); loop();

  // 1. The grid prints its page and GO TO #, and nothing that is not text.
  grid(2);
  std::string g = printed();
  char want[32];
  snprintf(want, sizeof(want), "3/");
  printf("      printed: \"%s\"\n", g.c_str());
  ck(g.find(want) != std::string::npos && g.find(T(S_GOTO)) != std::string::npos,
     "the grid shows its page number with GO TO #");
  ck(g.find(T(S_BTN_BACK)) != std::string::npos, "the grid shows BACK");
  ck(allPrintable(g), "every character the grid prints is printable");

  // 2. The header opens GO TO #; the pad's BACK returns to the same grid.
  grid(2);
  onTap(CX, HEAD_Y);
  ck(jumpOpen && galleryOpen, "tapping the header opens GO TO # (it used to close the Pokedex)");
  std::string p = printed();
  ck(p.find(T(S_BTN_BACK)) != std::string::npos && allPrintable(p), "the pad shows BACK");
  onTap(CX, 84 + 4 * 74 + 20 + 34);            // under the pad
  ck(!jumpOpen && galleryOpen && !galleryPick && galleryPage == 2, "the pad's BACK returns to the grid");

  // 3. A creature's page: BACK returns to the grid, not further.
  grid(0);
  onTap(73 + 40, 84 + 40);                     // the first cell
  ck(galleryDetail != 0, "(a cell opened its page)");
  std::string d = printed();
  ck(d.find(T(S_BTN_BACK)) != std::string::npos && allPrintable(d), "a creature's page shows BACK");
  onTap(CX, BACK_Y);
  ck(galleryOpen && galleryDetail == 0 && !galleryPick, "BACK on a creature's page returns to the grid");

  // 4. The grid's BACK goes up one level, to the region chooser -- not home.
  onTap(CX, BACK_Y);
  ck(galleryOpen && galleryPick, "BACK on the grid returns to the region chooser");

  // 5. The swipe down asks the same answer.
  grid(1);
  onSwipeV(1);
  ck(galleryOpen && galleryPick, "a swipe down on the grid also returns to the chooser");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
