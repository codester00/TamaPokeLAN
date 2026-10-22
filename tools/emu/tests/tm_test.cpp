// TMs as items (v3.51). The rule, both of its callers, the save, the Mart's
// daily five, the drop, and buying one through the Mart screen itself.
//
// CLAUDE.md trap 1 is why it is shaped this way: the gate is asked by the
// player's picker (moveUnlockLevelFor) and NOT by the opponents' builder
// (moveUnlockLevel), and each is tested from its own caller.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "tms.h"
#include "items.h"
#include <cstdio>
#include <cstring>
#include <vector>

uint32_t g_seed = 0x7A51;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
uint8_t learnableFor(int16_t dex, uint8_t lvl, uint8_t *out, uint8_t max);
void wildGrantDrops();
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
extern Pet pet;
extern bool martOpen, btlHard;
extern uint8_t martPage, btlMsgCount, martConfirmTm;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint8_t mv(const char *n) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, n)) return (uint8_t)i;
  return 0;
}
static bool isTm(uint8_t m) {
  for (uint16_t i = 0; i < TM_COUNT; i++) if (TM_MOVES[i] == m) return true;
  return false;
}
static bool offers(int16_t dex, uint8_t lvl, uint8_t m) {
  uint8_t l[192];
  uint8_t n = learnableFor(dex, lvl, l, sizeof(l));
  for (uint8_t i = 0; i < n; i++) if (l[i] == m) return true;
  return false;
}

int main() {
  setup();
  pet.dbgHatchAs(25, false);                         // PIKACHU
  pet.ageMinutes = 10 * MINUTES_PER_LEVEL;
  memset(pet.tmOwned, 0, sizeof(pet.tmOwned));
  uint8_t surf = mv("SURF"), tbolt = mv("THUNDERBOLT"), qa = mv("QUICK ATTACK");

  // --- 1. the player's gate, through the picker's own list
  ck(isTm(surf), "SURF is a TM");
  ck(!offers(25, 10, surf) && !offers(25, 90, surf), "no SURF for Pikachu without the TM, at any level");
  ck(offers(25, 15, qa), "level-up moves keep their real gates");
  pet.giveTm(surf);
  ck(offers(25, 5, surf), "with the SURF TM a level 5 Pikachu can learn it");
  ck(!offers(1, 50, surf), "but only a species that can learn it");
  ck(pet.giveTm(surf) == false, "owning a TM twice is not a second TM");
  ck(pet.hasTm(surf), "and it is still owned after being taught (reusable)");

  // --- 2. opponents keep the old rule
  {
    bool atForty = false, before = false;
    for (uint8_t i = 0; i < learnCount(25); i++) {
      if (learnLevel(25, i)) continue;
      if (learnMove(25, i) == tbolt) continue;
      uint8_t at = moveUnlockLevel(25, i);
      if (at == 40) atForty = true;
      if (at < 40) before = true;
    }
    ck(atForty && !before, "an opponent's TMs still open at level 40, owned or not");
  }

  // --- 3. it saves, through the checkpoint and through the legacy key alone
  {
    uint8_t blast = mv("FIRE BLAST");
    pet.giveTm(blast);
    pet.coins = 1234; pet.surfHi = 77;
    pet.renameTrainer("ASH");                        // commits a save
    Pet a; a.begin();
    ck(a.hasTm(surf) && a.hasTm(blast) && a.tmOwnedCount() == 2, "TMs survive a reload");
    ck(a.coins == 1234 && a.surfHi == 77, "and so do the coins and the surfing score");
    std::vector<uint8_t> ply[2] = { nvs()["plyA"], nvs()["plyB"] };
    nvs().erase("plyA"); nvs().erase("plyB");        // the legacy key alone
    Pet b; b.begin();
    ck(b.hasTm(surf) && b.hasTm(blast), "the legacy key alone carries them too");
    nvs()["plyA"] = ply[0]; nvs()["plyB"] = ply[1];
    nvs().erase("tms"); nvs().erase("coins"); nvs().erase("surfhi");   // the checkpoint alone
    Pet c; c.begin();
    ck(c.hasTm(surf) && c.hasTm(blast) && c.coins == 1234, "and the checkpoint alone does");
  }

  // --- 4. the Mart's five a day
  {
    Pet p; memset(p.tmOwned, 0, sizeof(p.tmOwned));
    uint8_t a[MART_TM_PER_DAY], b[MART_TM_PER_DAY];
    uint8_t na = martTmToday(p, 20000, a), nb = martTmToday(p, 20000, b);
    ck(na == MART_TM_PER_DAY && na == nb && !memcmp(a, b, na), "five TMs, the same all day");
    bool distinct = true;
    for (int i = 0; i < na; i++) for (int j = i + 1; j < na; j++) if (a[i] == a[j]) distinct = false;
    ck(distinct, "and all different");
    int changed = 0;
    for (uint32_t d = 20001; d < 20031; d++) { martTmToday(p, d, b); if (memcmp(a, b, 5)) changed++; }
    ck(changed >= 25, "a different set most days");
    martTmToday(p, 20000, a);
    p.giveTm(a[2]);
    nb = martTmToday(p, 20000, b);
    bool gone = true;
    for (int i = 0; i < nb; i++) if (b[i] == a[2]) gone = false;
    ck(nb == 4 && gone, "a bought TM leaves the list, nothing takes its place that day");
    for (uint16_t i = 0; i < TM_COUNT; i++) p.giveTm(TM_MOVES[i]);
    ck(martTmToday(p, 20000, b) == 0, "a player who owns them all is offered none");
    ck(tmRandomUnowned(p, 12345) == 0, "and none can drop");
  }

  // --- 5. prices sit in the games' range
  {
    bool ok = true;
    for (uint16_t i = 0; i < TM_COUNT; i++) { uint16_t pr = tmPrice(TM_MOVES[i]); if (pr < 1500 || pr > 5000) ok = false; }
    ck(ok, "every TM costs $1,500-$5,000");
  }

  // --- 6. the drop: now and then, never one already owned
  {
    memset(pet.tmOwned, 0, sizeof(pet.tmOwned));
    btlHard = false;
    uint16_t before = 0;
    int drops = 0, doubles = 0;
    for (int i = 0; i < 2000; i++) {
      btlMsgCount = 0;
      wildGrantDrops();
      uint16_t now = pet.tmOwnedCount();
      if (now > before) drops++;
      if (now > before + 1) doubles++;
      before = now;
    }
    ck(doubles == 0, "never two TMs from one win");
    printf("     %d TM drops in 2000 easy wins\n", drops);
    ck(drops > 50 && drops < 160, "about one win in twenty drops a TM");
  }

  // --- 7. buying one through the Mart screen
  {
    memset(pet.tmOwned, 0, sizeof(pet.tmOwned));
    pet.wallet = 999999;
    martOpen = true;
    uint8_t today[MART_TM_PER_DAY];
    uint8_t k = martTmToday(pet, pet.today(), today);
    uint8_t items = martSellableCount();
    uint8_t row = items;                             // the first TM row
    martPage = row / 3;
    render();
    onTap(233, 210 + (row % 3) * 60 + 25);
    ck(martConfirmTm == today[0], "tapping a TM row asks to buy that TM");
    int t1, b1, t2, b2;
    uiConfirmRects(&t1, &b1, &t2, &b2);
    uint32_t w0 = pet.wallet;
    onTap(233, (t1 + b1) / 2);
    ck(pet.hasTm(today[0]), "YES buys it");
    ck(pet.wallet == w0 - tmPrice(today[0]), "for its price");
    uint8_t after[MART_TM_PER_DAY];
    ck(martTmToday(pet, pet.today(), after) == k - 1, "and it is gone from today's list");
    martOpen = false;
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
