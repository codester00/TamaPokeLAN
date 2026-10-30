// Selling at the Mart (v3.90): half the Mart's price, the games' rule, through
// the BUY | SELL switch on the Mart's title pill. Driven through the screen's
// own taps, not just itemSellPrice(), since the screen is the caller that
// matters (CLAUDE.md trap 1).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "items.h"
#include "inventory.h"
#include "i18n.h"
#include <cstdio>
#include <string>

uint32_t g_seed = 0x5E11;
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
void uiConfirmRects(int *a, int *b, int *c, int *d);
int martTabAt(int16_t x, int16_t y);
uint8_t martSellCount();
extern Pet pet;
extern Inventory bag;
extern bool martOpen, martSelling, martConfirmSell;
extern ItemKey martConfirmItem;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const int CX = 233;
static void tapSellTab() { onTap(CX + 70, 55); }
static void tapBuyTab() { onTap(CX - 70, 55); }
static void tapRow(int i) { onTap(CX, 210 + i * 60 + 25); }
static void tapYes() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); onTap(CX, (a + b) / 2); }
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
  if (pet.isEgg()) pet.dbgHatchAs(4, false);

  // 1. The price: half, and never at a profit.
  ck(itemSellPrice(IT_POKEBALL) == 100 && itemSellPrice(IT_FIRESTONE) == 1050 &&
     itemSellPrice(IT_LINKCORD) == 4900, "half the Mart's price");
  ck(itemSellPrice(IT_MASTERBALL) == 0, "what the Mart never sells it never buys (Master Ball)");
  bool loss = true;
  for (uint8_t k = 1; k < ITEM_COUNT; k++)
    if (itemForSale(k) && itemSellPrice(k) >= itemEntry(k).price) loss = false;
  ck(loss, "selling back always pays less than buying");

  // 2. The switch: the Mart opens on BUY; the title pill's right half is SELL.
  for (uint8_t k = 1; k < ITEM_COUNT; k++) while (bag.count(k)) bag.consume(k);
  bag.add(IT_FIRESTONE, 3);
  bag.add(IT_MASTERBALL, 1);
  pet.wallet = 1000;
  uiTileGo(4);
  ck(martOpen && !martSelling, "(the Mart opens, on BUY)");
  ck(martTabAt(CX - 70, 55) == 0 && martTabAt(CX + 70, 55) == 1 && martTabAt(CX, 150) == -1,
     "the title pill's halves are BUY and SELL");
  tapSellTab();
  ck(martSelling, "tapping SELL switches the list");
  ck(martSellCount() == 1, "the list holds what the bag has and the Mart buys (no Master Ball)");
  std::string s = printed();
  ck(s.find("FIRE STONE") != std::string::npos && s.find("x3") != std::string::npos &&
     s.find("$1050") != std::string::npos, "a row reads the item, how many, and what it pays");

  // 3. Selling one, through the confirm dialog.
  tapRow(0);
  ck(martConfirmItem == IT_FIRESTONE && martConfirmSell, "a row asks first");
  std::string q = printed();
  ck(q.find("Sell FIRE STONE?") != std::string::npos, "the dialog asks to SELL, with the item's name");
  tapYes();
  ck(bag.count(IT_FIRESTONE) == 2 && pet.wallet == 1000 + 1050, "YES takes one stone and pays $1,050");
  ck(!martConfirmItem && !martConfirmSell, "(the dialog closed)");

  // 4. Never past the wallet's cap: the money would be thrown away.
  pet.wallet = Pet::WALLET_CAP - 100;
  tapRow(0);
  ck(!martConfirmItem && bag.count(IT_FIRESTONE) == 2 && pet.wallet == Pet::WALLET_CAP - 100,
     "a sale that would overflow the wallet is refused, nothing taken");

  // 5. Back on BUY, buying works as before.
  pet.wallet = 1000;
  tapBuyTab();
  ck(!martSelling, "tapping BUY switches back");
  tapRow(0);                                   // POKE BALL, $200
  ck(martConfirmItem != ITEM_NONE && !martConfirmSell, "a BUY row asks to buy");
  uint8_t balls = bag.count(IT_POKEBALL);
  tapYes();
  ck(bag.count(IT_POKEBALL) == balls + 1 && pet.wallet == 800, "and buying still charges the Mart's price");

  // 6. Nothing to sell says so.
  for (uint8_t k = 1; k < ITEM_COUNT; k++) while (bag.count(k)) bag.consume(k);
  tapSellTab();
  std::string e = printed();
  ck(martSellCount() == 0 && e.find(T(S_MART_NOTHING)) != std::string::npos, "an empty bag reads NOTHING TO SELL");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
