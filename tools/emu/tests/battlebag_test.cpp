// The battle bag pages, and a faint drops with its own line (v3.71).
//
// The bag: v3.70 and earlier drew the first four usable items and had no way to
// reach the rest -- a player with four kinds of ball never saw a potion. The
// bag is paged along the rim now, so this fills it with eight usable items
// (balls sort first) and uses every potion on page two through the sketch's
// own rim and tap handlers.
//
// The faint: the foe's sprite dropped out of frame the moment the moves
// resolved, while "X used Y" was still on screen. It now waits for the line
// that says it fainted.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "items.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0xBA6;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y); void onRim(int dir);
uint8_t uiCurrentScreen(); extern const char *const SCREEN_NAME[];
uint8_t uiRimPages(); uint8_t uiRimPage();
void startBattle(int16_t dex, uint8_t lvl);
int btlCellIndexAt(int16_t x, int16_t y);
extern uint32_t btlFaintUntil[2];
extern Pet pet;
extern Inventory bag;
extern bool battleOpen, btlOver;
extern uint8_t btlMenu, btlMsgCount, btlBagPage;
extern char btlMsg[8][40];
extern Combatant btlYou, btlFoe;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
static void tapCell(int i) { int16_t x = 0, y = 0; cellPoint(i, x, y); onTap(x, y); }
static void drain() { for (int t = 0; t < 20 && btlMsgCount; t++) onTap(233, 233); }

static void freshCreature(uint8_t level) {
  pet.ageMinutes = (uint32_t)(level - 1) * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  freshCreature(100);

  // ---- the bag reaches every item
  bag.clear();
  const ItemKey ITEMS[] = { IT_POKEBALL, IT_GREATBALL, IT_ULTRABALL, IT_MASTERBALL,
                            IT_POTION, IT_SUPERPOTION, IT_HYPERPOTION, IT_FULLHEAL };
  for (ItemKey k : ITEMS) bag.add(k, 3);
  startBattle(129, 2);                          // a MAGIKARP: SPLASH never hurts
  ck(battleOpen, "a fight is open");
  tapCell(1);                                   // BAG
  ck(btlMenu == 3, "BAG opens the bag");
  render();
  ck(uiRimPages() == 2, "eight usable items are two pages on the rim");
  onRim(1);
  ck(uiRimPage() == 1 && btlBagPage == 1, "the rim turns to page two");
  for (int i = 0; i < 4; i++) {
    ItemKey k = ITEMS[4 + i];
    uint8_t before = bag.count(k);
    btlYou.hp = btlYou.maxHp / 2;                // room for a potion
    btlYou.ailment = i == 3 ? AIL_POISON : AIL_NONE;   // something to FULL HEAL
    if (btlMenu != 3) { tapCell(1); onRim(1); }
    render();
    tapCell(i);
    ck(bag.count(k) == before - 1, "an item on page two is used from its cell");
    drain();
  }
  onTap(10, 10);                                // back out of the bag
  drain();
  btlMenu = 0;

  // ---- the foe drops with its faint line, not before
  btlYou.hp = btlYou.maxHp;
  tapCell(0);                                   // FIGHT
  int slot = 0;
  for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i]) { slot = i; break; }
  tapCell(slot);
  ck(btlFoe.fainted(), "(the level 100 creature knocks the MAGIKARP out)");
  ck(btlMsgCount >= 2, "(the turn has more than one line)");
  ck(!strstr(btlMsg[0], "faint") && !strstr(btlMsg[0], "FAINT"),
     "(the first line is the attack, not the faint)");
  render();
  ck(btlFaintUntil[1] == 0, "the foe still stands while the attack's line is read");
  bool reached = false;
  for (int t = 0; t < 8 && btlMsgCount; t++) {
    bool isFaint = strstr(btlMsg[0], btlFoe.name) &&
                   (strstr(btlMsg[0], "faint") || strstr(btlMsg[0], "FAINT"));
    if (isFaint) { reached = true; break; }
    ck(btlFaintUntil[1] == 0, "no drop before the faint line");
    onTap(233, 233);
  }
  ck(reached, "the faint line comes on screen");
  ck(btlFaintUntil[1] != 0, "the drop starts with the faint line");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
