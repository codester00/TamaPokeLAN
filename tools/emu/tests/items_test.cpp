// The v3.91 items, through the screens that use them.
//
// Vitamins: +10 (was +5, worse than the free training games), and usable on a
// party creature without swapping it in -- the bag opens the party ring and the
// tap picks who; the hub is the creature being raised. One rule for both
// (vitaminInto()), never past the IV-bound ceiling.
// X items: +2 stages in battle (Gen 7 onward), from the battle bag, costing the
// turn as every item does.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "items.h"
#include "i18n.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x17E5;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void startBattle(int16_t dex, uint8_t lvl);
int btlCellIndexAt(int16_t x, int16_t y);
void partySlotPos(int i, int *x, int *y);
extern Pet pet;
extern Party party;
extern Inventory bag;
extern bool battleOpen, bagOpen, partyOpen;
extern uint8_t btlMenu, btlMsgCount, vitPick;
extern char btlMsg[8][40];
extern Combatant btlYou;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const int CX = 233, CY = 233;
// the bag's rows: find the row of k by its position in the bag
static void tapBagItem(ItemKey k) {
  uint8_t d = bag.distinctCount(), at = 0;
  for (uint8_t i = 0; i < d; i++) if (bag.keyAt(i) == k) at = i;
  bagOpen = true;
  render();
  extern uint8_t bagPage;
  bagPage = at / 5;                       // BAG_PER_PAGE 5, BAG_ROW_Y(i) 112 + 58 i
  render();
  onTap(CX, 112 + (at % 5) * 58 + 25);
}
static void tapSlot(int i) { int x, y; partySlotPos(i, &x, &y); onTap((int16_t)x, (int16_t)y); }
static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
static void tapCell(int i) { int16_t x = 0, y = 0; cellPoint(i, x, y); onTap(x, y); }

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  pet.ivAtk = 31; pet.ivDef = 31; pet.ivSpe = 31;
  pet.trAtk = pet.trDef = pet.trSpe = 0;

  // 1. With no party, a vitamin goes straight to the creature being raised: +10.
  for (auto &m : party.slots) m = PartyMon();
  bag.clear();
  bag.add(IT_PROTEIN, 5);
  tapBagItem(IT_PROTEIN);
  ck(pet.trAtk == 10 && bag.count(IT_PROTEIN) == 4 && !vitPick, "no party: PROTEIN gives the live pet +10 ATK");

  // 2. With a party, the ring asks who; a slot gets it, the live pet does not.
  PartyMon m; m.dex = 16; m.level = 30; m.ivAtk = 31; m.ivDef = 31; m.ivSpe = 31; m.trAtk = 40;
  party.slots[0] = m;
  tapBagItem(IT_PROTEIN);
  ck(vitPick == IT_PROTEIN && partyOpen && !bagOpen, "with a party, the bag opens the ring to choose");
  tapSlot(0);
  ck(party.slots[0].trAtk == 50 && pet.trAtk == 10 && bag.count(IT_PROTEIN) == 3,
     "a party slot gets +10, not the live pet");
  ck(bagOpen && !vitPick, "(back to the bag)");

  // 3. The hub is the live pet.
  tapBagItem(IT_PROTEIN);
  onTap(CX, CY);
  ck(pet.trAtk == 20 && party.slots[0].trAtk == 50 && bag.count(IT_PROTEIN) == 2, "the hub gives it to the live pet");

  // 4. Never past the ceiling: a stat at its limit keeps the item.
  party.slots[0].ivAtk = 0;                       // limit 70
  party.slots[0].trAtk = 65;
  tapBagItem(IT_PROTEIN);
  tapSlot(0);
  ck(party.slots[0].trAtk == 70 && bag.count(IT_PROTEIN) == 1, "the last few points stop at the IV's ceiling");
  tapBagItem(IT_PROTEIN);
  tapSlot(0);
  ck(party.slots[0].trAtk == 70 && bag.count(IT_PROTEIN) == 1, "at the ceiling the vitamin is refused and kept");

  // 5. Cancelling keeps it.
  tapBagItem(IT_PROTEIN);
  onTap(CX, 20);                                  // the strip at the top
  ck(bag.count(IT_PROTEIN) == 1 && !vitPick && bagOpen, "cancelling keeps the vitamin");
  bagOpen = false; partyOpen = false;

  // 6. X items: +2 stages, from the battle bag, costing the turn.
  bag.clear();
  bag.add(IT_XATTACK, 2);
  bag.add(IT_XSPEED, 1);
  pet.ageMinutes = 49UL * MINUTES_PER_LEVEL;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  startBattle(129, 2);                            // a MAGIKARP: SPLASH never hurts
  ck(battleOpen && btlYou.stage[SI_ATK] == 0, "(a fight, no stages yet)");
  tapCell(1);                                     // BAG
  render();
  tapCell(0);                                     // the first usable item: X ATTACK
  ck(btlYou.stage[SI_ATK] == 2 && bag.count(IT_XATTACK) == 1, "X ATTACK raises ATTACK two stages");
  bool said = false;
  for (int i = 0; i < btlMsgCount; i++) if (strstr(btlMsg[i], "rose sharply")) said = true;
  ck(said, "and says it rose sharply");
  for (int t = 0; t < 20 && btlMsgCount; t++) onTap(CX, CY);
  btlYou.stage[SI_ATK] = 5;
  tapCell(1); render(); tapCell(0);
  ck(btlYou.stage[SI_ATK] == 6 && bag.count(IT_XATTACK) == 0, "near the top it stops at +6");
  for (int t = 0; t < 20 && btlMsgCount; t++) onTap(CX, CY);
  btlYou.stage[SI_SPE] = 6;
  tapCell(1); render(); tapCell(0);              // now X SPEED is the first
  ck(btlYou.stage[SI_SPE] == 6 && bag.count(IT_XSPEED) == 1, "at +6 an X item is refused and kept");
  ck(itemUsableInBattle(IT_XDEFENSE) && !itemUsableInField(IT_XDEFENSE), "X items are for battle only");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
