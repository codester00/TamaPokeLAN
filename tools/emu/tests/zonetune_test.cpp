// The v4.09 tuning pass: the owner's economy rules pinned as rules (not as the
// numbers that happen to satisfy them today), the gauntlet's bosses, and the
// BACK button on every Battle Zone screen and the Battle Hall, plus the bigger
// status text, read off the panel.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "gauntlet.h"
#include "hall.h"
#include "zone.h"
#include "trainers.h"
#include "species.h"
#include <cstdio>
#include <cstring>
#include <string>
uint32_t g_seed = 0x409; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void uiTileGo(int i);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
void plazaBtnRect(uint8_t i, int *x, int *y, int *w, int *h);
void zoneRowRect(uint8_t i, int *x, int *y, int *w, int *h);
void gauntRuleRect(uint8_t i, int *x, int *y, int *w, int *h);
void backPillRect(uint8_t which, int *x, int *y, int *w, int *h);
int backZoneY(uint8_t which);
extern Pet pet;
extern Arduino_Canvas *gfx;
extern bool plazaOpen, zoneOpen, bhallOpen;
extern uint8_t zoneMode;
static const uint8_t BACK_ZONE_T = 3;   // the sketch's BACK_ZONE

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void toPlaza() { for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i); }
static void tapPlaza(uint8_t i) { render(); int x, y, w, h; plazaBtnRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapRow(uint8_t i) { render(); int x, y, w, h; zoneRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapBack() { render(); int x, y, w, h; backPillRect(BACK_ZONE_T, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static std::string drawn() {
  std::string log; Arduino_Canvas::textLog = &log; render(); Arduino_Canvas::textLog = nullptr; return log;
}
// The pixels of the BACK pill's middle row hold ink (its word), i.e. it is drawn.
static bool pillDrawn() {
  render();
  int x, y, w, h; backPillRect(BACK_ZONE_T, &x, &y, &w, &h);
  const uint16_t *fb = gfx->buffer();
  int ink = 0;
  for (int yy = y + 4; yy < y + h - 4; yy++)
    for (int xx = x + 8; xx < x + w - 8; xx++) if (fb[yy * 466 + xx] == 0x0000 || fb[yy * 466 + xx] == UI_INK) ink++;
  return ink > 20;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6, false);

  // --- 1. the owner's economy rules (2026-11-05) ------------------------------
  auto prize = [](ItemKey k) { for (uint8_t i = 0; i < ZONE_PRIZE_COUNT; i++) if (ZONE_PRIZES[i].item == k) return (int)ZONE_PRIZES[i].bp; return -1; };
  int run40 = 0; for (uint16_t r = 0; r < 40; r++) run40 += gauntletBp(r, false);
  int run10 = 0; for (uint16_t r = 0; r < 10; r++) run10 += gauntletBp(r, false);
  int ppmax = prize(IT_PPMAX), cap = prize(IT_BOTTLECAP), mb = prize(IT_MASTERBALL), ppup = prize(IT_PPUP);
  ck(ppmax <= run40 && cap <= run40 && ppmax > 2 * run10 + run10 / 2, "PP MAX / BOTTLE CAP: one 40-win run, or about 3-4 casual runs of 10");
  ck(ppmax >= 3 * run10 && ppmax <= 4 * run10 + run10 / 2 && cap == ppmax, "  (3-4 runs of ten, both the same)");
  ck(mb >= 6 * ppmax && mb <= 7 * ppmax, "the Master Ball is 6-7 PP MAXes");
  ck(3 * ppup > ppmax, "three PP UPs stay dearer than a PP MAX");
  bool flat = true;
  for (uint16_t r = 0; r < 60; r++) if (!gauntletIsBoss(r) && gauntletBp(r, false) != gauntletBp(0, false)) flat = false;
  ck(flat, "a gauntlet win pays the same every round, bosses aside");
  ck(gauntletBp(19, false) == gauntletBp(0, false) + GAUNTLET_BOSS_BP && gauntletBp(19, true) == 2 * gauntletBp(19, false),
     "a boss pays its bonus, doubled on the day's 2x");
  ck(ZONE_FEE[ZONE_GAUNTLET] < trainerPrize(0, 7, true, false) && ZONE_FEE[ZONE_TOURNAMENT] < trainerPrize(0, TRAINER_COUNT - 1, true, false),
     "an entry costs less than one hard rematch pays (a retry, never a wall)");
  ck(bhallPrize(0, 50) == (uint32_t)HALL_PAY * 50 * HALL_CLASS, "the Hall's rung pay");

  // --- 2. the bosses ---------------------------------------------------------------
  ck(!gauntletIsBoss(18) && gauntletIsBoss(19) && gauntletIsBoss(39) && !gauntletIsBoss(20), "a boss is the 20th, 40th, ... challenger");
  GauntletRules r; r.size = 3;
  ZoneFoe b, n;
  ck(gauntletChallenger(r, 19, 1234, 100, 19, b) && gauntletChallenger(r, 18, 1234, 100, 19, n), "both built");
  ck(b.iv == 31 && b.tr == 100 && b.smart && b.switching, "a boss: IV 31, full training, the smart AI, switching");
  ck(n.tr < 100 && b.level[0] == 100, "the challenger before it is not fully trained; the boss is at the team's level");
  // a boss is drawn from the GAUNTLET_BOSS_TOP strongest that fit
  int floorBst = 0;
  { int top[GAUNTLET_BOSS_TOP] = { 0 };
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      if (!gauntletSpeciesFits(r, d) || !speciesShowable(d) || DEX_TBL[d].rarity == R_LEGENDARIO) continue;
      int v = gauntletBst(d);
      for (int j = 0; j < GAUNTLET_BOSS_TOP; j++)
        if (v > top[j]) { for (int m = GAUNTLET_BOSS_TOP - 1; m > j; m--) top[m] = top[m - 1]; top[j] = v; break; }
    }
    floorBst = top[GAUNTLET_BOSS_TOP - 1]; }
  bool strong = true, weakSeen = false;
  for (uint32_t sd = 0; sd < 40; sd++) {
    ZoneFoe f; gauntletChallenger(r, 39, sd * 77 + 5, 100, 19, f);
    for (uint8_t k = 0; k < f.count; k++) if (gauntletBst(f.dex[k]) < floorBst) strong = false;
    ZoneFoe g; gauntletChallenger(r, 38, sd * 77 + 5, 100, 19, g);
    for (uint8_t k = 0; k < g.count; k++) if (gauntletBst(g.dex[k]) < floorBst) weakSeen = true;
  }
  ck(strong, "every boss creature is among the strongest that fit");
  ck(weakSeen, "  (and an ordinary challenger is not: the filter is the boss's)");
  ck(ZONE_SYM_SILVER_STREAK == GAUNTLET_BOSS_EVERY && zoneSymbols(0, 19) == 0 && zoneSymbols(0, 20) == ZSYM_SILVER &&
     zoneSymbols(0, 40) == (ZSYM_SILVER | ZSYM_GOLD), "silver for the first boss, gold for the second");

  // --- 3. BACK on every zone screen and the Hall --------------------------------------
  for (uint8_t rg = 0; rg < GYM_REGIONS; rg++) pet.winBadge(rg, TRAINER_COUNT - 1, false);
  pet.wallet = 100000;
  toPlaza();
  tapPlaza(3);
  ck(!strcmp(scr(), "zone") && zoneMode == 0, "the zone's landing page");
  ck(pillDrawn(), "  BACK is drawn");
  { int x, y, w, h, bx, by, bw, bh; zoneRowRect(2, &x, &y, &w, &h); backPillRect(BACK_ZONE_T, &bx, &by, &bw, &bh);
    ck(y + h < backZoneY(BACK_ZONE_T) && by + bh < 440, "  the last row ends above the BACK zone, the pill above the tile dots"); }
  tapRow(2); ck(zoneMode == 1 && pillDrawn(), "prizes: BACK drawn");
  tapBack(); ck(zoneMode == 0 && zoneOpen, "  and it goes back to the landing page");
  tapRow(1); ck(zoneMode == 2 && pillDrawn(), "gauntlet: BACK drawn");
  tapRow(1); ck(zoneMode == 3 && pillDrawn(), "custom rules: BACK drawn");
  { int x, y, w, h; gauntRuleRect(4, &x, &y, &w, &h); ck(y + h < backZoneY(BACK_ZONE_T), "  START ends above the BACK zone"); }
  tapBack(); ck(zoneMode == 2, "  rules -> gauntlet");
  tapBack(); ck(zoneMode == 0, "  gauntlet -> the landing page");
  tapRow(0); ck(zoneMode == 4 && pillDrawn(), "tournament: BACK drawn");
  tapBack(); ck(zoneMode == 0, "  tournament -> the landing page");
  tapBack(); ck(!zoneOpen && !strcmp(scr(), "plaza"), "the landing page -> the PLAZA");
  tapPlaza(2); ck(bhallOpen && pillDrawn(), "the Battle Hall: BACK drawn");
  tapBack(); ck(!bhallOpen && !strcmp(scr(), "plaza"), "  and it goes back to the PLAZA");

  // --- 4. the status text is the big size where it fits --------------------------------
  // A size-2 glyph is 12 px wide: the fee line drawn at size 2 covers twice the
  // width of the same line at size 1. Measure the ink of line 2 on the gauntlet page.
  tapPlaza(3); tapRow(1);
  render();
  const uint16_t *fb = gfx->buffer();
  int minX = 466, maxX = 0, rows = 0;
  for (int yy = 120; yy < 136; yy++) {
    bool any = false;
    for (int xx = 0; xx < 466; xx++) if (fb[yy * 466 + xx] == UI_INK_SOFT) { any = true; if (xx < minX) minX = xx; if (xx > maxX) maxX = xx; }
    if (any) rows++;
  }
  char fee[32]; snprintf(fee, sizeof(fee), "ENTRY $%lu", (unsigned long)ZONE_FEE[ZONE_GAUNTLET]);
  ck(rows >= 12 && maxX - minX >= (int)strlen(fee) * 12 - 14, "the fee line is drawn at size 2 (16 px tall, 12 px a glyph)");
  std::string t = drawn();
  ck(t.find(fee) != std::string::npos, "  and it reads the new fee");

  printf(bad ? "\n%d FAILED\n" : "\nALL PASS\n", bad);
  return bad ? 1 : 0;
}
