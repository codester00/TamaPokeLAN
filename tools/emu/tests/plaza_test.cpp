// The PLAZA tile (v4.01): LAN BATTLE and LAN TRADE moved off the gym screens
// to it, and the Battle Hall and Battle Zone wait there. Driven through the
// firmware's own taps and swipes: each LAN screen opens OVER the PLAZA and
// leaving it comes back there, the two that are not built do nothing, and the
// Zone's row names its gate until every champion is beaten.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "i18n.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <string>

uint32_t g_seed = 0x91A;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void render();
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
void uiTileGo(int i);
int uiTileIndex();
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
void plazaBtnRect(uint8_t i, int *x, int *y, int *w, int *h);
bool zoneUnlocked();
extern Pet pet;
extern bool plazaOpen, lanOpen, tradeOpen, gymOpen, gymPick;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void tapBtn(uint8_t i) {
  int x, y, w, h;
  plazaBtnRect(i, &x, &y, &w, &h);
  onTap(x + w / 2, y + h / 2);
}
static void toPlaza() {
  for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i);
  lanOpen = tradeOpen = false;
}
static std::string printed() {
  std::string log;
  Arduino_Canvas::textLog = &log;
  render();
  Arduino_Canvas::textLog = nullptr;
  return log;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  toPlaza();
  ck(!strcmp(scr(), "plaza"), "the PLAZA is a tile");

  tapBtn(0);
  ck(!strcmp(scr(), "lan"), "LAN BATTLE opens the LAN screen");
  onSwipeV(1);
  ck(!strcmp(scr(), "plaza"), "and leaving it comes back to the PLAZA");

  tapBtn(1);
  ck(!strcmp(scr(), "trade"), "LAN TRADE opens trading");
  onSwipeV(1);
  ck(!strcmp(scr(), "plaza"), "and leaving it comes back to the PLAZA");

  tapBtn(2);
  ck(!strcmp(scr(), "plaza"), "the Battle Hall is not built: a tap stays");
  tapBtn(3);
  ck(!strcmp(scr(), "plaza"), "nor the Battle Zone");

  onSwipeV(1);
  ck(!strcmp(scr(), "main"), "down from the PLAZA goes home");

  // The gym screens hold only ladders now: no LAN on the chooser.
  for (int i = 0; i < 16 && !gymOpen; i++) uiTileGo(i);
  ck(gymOpen && gymPick, "(the gym chooser)");
  std::string g = printed();
  ck(g.find(T(S_LAN)) == std::string::npos, "the gym chooser no longer offers LAN");

  // The Zone's gate: every region's champion, either difficulty.
  toPlaza();
  ck(!zoneUnlocked(), "a new player has not opened the Zone");
  ck(printed().find(T(S_PLZ_NEEDS_CHAMPS)) != std::string::npos, "and its row names the gate");
  for (uint8_t r = 0; r < GYM_REGIONS; r++) pet.winBadge(r, TRAINER_COUNT - 1, r % 2 == 0);
  ck(zoneUnlocked(), "every champion, on either difficulty, opens it");
  ck(printed().find(T(S_PLZ_NEEDS_CHAMPS)) == std::string::npos, "and the gate line goes");
  pet.badgesX[GYM_REGIONS - 2] = 0;   // one region's champion not beaten after all
  pet.badgesHardX[GYM_REGIONS - 2] = 0;
  ck(!zoneUnlocked(), "one champion missing keeps it shut");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
