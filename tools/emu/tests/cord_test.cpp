// The Linking Cord: trade evolution without a trade, as the owner specified it.
//
//   - one cord, one evolution, used up -- and NOT used up when it does nothing;
//   - sold at the Mart with the vitamins, never in the battle bag;
//   - KARRABLAST and SHELMET still need each other: the cord asks for the
//     partner on the party ring, and only the real pair spends it.
//
// Drives the REAL bag and party screens through their tap handlers, at
// positions asked of the firmware (uiBagRowCenterY, partySlotPos) -- the cord's
// rule living in Pet::cordUse() proves nothing if the bag routes around it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "items.h"
#include "dex.h"
#include <cstdio>
#include <cstdlib>

uint32_t g_seed = 0xC0BD;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void render();
void bagTap(int16_t x, int16_t y);
void partyTap(int16_t x, int16_t y);
int uiBagRowCenterY(int i);
void partySlotPos(int i, int *cx, int *cy);
extern Pet pet;
extern Party party;
extern Inventory bag;
extern bool bagOpen, partyOpen, cordPick;
extern uint8_t bagPage;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

static PartyMon banked(int16_t dex, uint32_t serial) {
  PartyMon m;
  m.dex = dex;
  m.level = 25;
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
  m.stateVersion = 1;
  m.fullness = m.joy = m.energy = 90;
  m.hygiene = 100;
  m.weight = 25;
  m.ageMinutes = 24 * MINUTES_PER_LEVEL;
  m.uid.mac[0] = 0xCC;
  m.uid.serial = serial;
  return m;
}
static void live(int16_t dex, uint32_t serial) {
  pet.switchTo(banked(dex, serial));
  pet.fullness = pet.joy = pet.energy = pet.hygiene = 100;
  pet.sleeping = false;
}
// Taps the cord's row in the bag. The test keeps the bag holding cords and
// nothing else, so the cord is row 0 of page 0 -- asserted, not assumed.
static void tapCord() {
  bagOpen = true;
  partyOpen = false;
  bagPage = 0;
  if (bag.keyAt(0) != IT_LINKCORD) { ck(false, "(the bag's first row is the cord)"); return; }
  bagTap(233, (int16_t)uiBagRowCenterY(0));
}
static void tapSlot(int i) {
  int x, y;
  partySlotPos(i, &x, &y);
  partyTap((int16_t)x, (int16_t)y);
}
static void dump(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n466 466\n255\n");
  const uint16_t *fb = gfx->buffer();
  for (int i = 0; i < 466 * 466; i++) {
    uint16_t c = fb[i];
    unsigned char px[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                            (unsigned char)((c & 31) << 3) };
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  // An emptied bag holding only cords, so the cord is row 0 of page 0.
  bag.clear();

  // --- 1. it is for sale, priced with the vitamins, and stays out of battle
  ck(itemForSale(IT_LINKCORD) && itemEntry(IT_LINKCORD).price == itemEntry(IT_PROTEIN).price,
     "the Mart sells it at the vitamins' price");
  ck(itemEntry(IT_LINKCORD).dropWeight == 0, "it never drops from a wild win");
  ck(itemUsableInField(IT_LINKCORD) && !itemUsableInBattle(IT_LINKCORD),
     "it is used from the bag, never in a fight");

  // --- 2. one cord, one evolution: a live Kadabra
  bag.add(IT_LINKCORD, 3);
  live(64, 301);
  ck(pet.evoInfo().status == Pet::EVO_NEEDS_TRADE, "(the live Kadabra is waiting for a trade)");
  tapCord();
  ck(bag.count(IT_LINKCORD) == 2, "the cord is used up: exactly one");
  ck(pet.tradeEvo && pet.canEvolveNow(), "and the Kadabra can evolve now");
  pet.evolve();
  ck(pet.speciesId == 65, "into Alakazam");

  // --- 3. it is NOT used up when it would do nothing
  tapCord();
  ck(bag.count(IT_LINKCORD) == 2, "on Alakazam (a final form) it is kept");
  live(25, 302);
  tapCord();
  ck(bag.count(IT_LINKCORD) == 2 && !pet.tradeEvo, "on a Pikachu it is kept");
  live(93, 303);
  pet.tradeEvo = true;   // already marked, by a real trade say
  tapCord();
  ck(bag.count(IT_LINKCORD) == 2, "on an already-marked Haunter it is kept");
  Pet frozen;
  frozen.reviveFrom(banked(64, 304));
  ck(frozen.cordUse() == Pet::CORD_NO_EFFECT, "a frozen companion is no target (it can never evolve)");

  // --- 4. KARRABLAST needs a SHELMET: the cord asks, and only the pair spends it
  party.releaseAt(0);
  party.releaseAt(1);
  party.replaceAt(0, banked(25, 305));    // a Pikachu
  party.replaceAt(1, banked(616, 306));   // a Shelmet
  live(588, 307);                          // the live Karrablast
  tapCord();
  ck(partyOpen && cordPick && !bagOpen, "on a Karrablast the party ring opens to pick a partner");
  render();
  if (getenv("CORD_SHOT")) dump(getenv("CORD_SHOT"));
  tapSlot(0);
  ck(!cordPick && bagOpen, "picking the wrong creature goes back to the bag");
  ck(bag.count(IT_LINKCORD) == 2 && !pet.tradeEvo && !(party.slots[0].flags & PMON_TRADE_EVO),
     "and neither is marked, and the cord is kept");
  tapCord();
  ck(cordPick, "(asking again)");
  partyTap(233, 20);   // the top band cancels
  ck(!cordPick && bagOpen && bag.count(IT_LINKCORD) == 2, "cancelling keeps the cord");
  tapCord();
  tapSlot(1);
  ck(bag.count(IT_LINKCORD) == 1, "linking with the Shelmet spends exactly one cord");
  ck(pet.tradeEvo && (party.slots[1].flags & PMON_TRADE_EVO), "and marks BOTH");
  ck(!(party.slots[0].flags & PMON_TRADE_EVO), "and nobody else");
  ck(pet.canEvolveNow() && pet.evoInfo().target == 589, "the Karrablast can become Escavalier");
  Party reread;
  reread.begin();
  ck(reread.slots[1].flags & PMON_TRADE_EVO, "the Shelmet's mark is saved");
  Pet sh;
  sh.switchTo(reread.slots[1]);
  sh.fullness = sh.joy = sh.energy = sh.hygiene = 100;
  ck(sh.canEvolveNow() && sh.evoInfo().target == 617, "and the Shelmet becomes Accelgor when raised");

  // --- 5. a failed link leaves a partner's EXISTING mark alone
  {
    PartyMon kb = banked(588, 308);
    Pet::markTraded(kb, 616);   // marked by a real trade for a Shelmet
    live(588, 309);
    ck(!pet.applyCordPair(kb), "(two Karrablast do not pair)");
    ck(kb.flags & PMON_TRADE_EVO, "and the partner keeps the mark its real trade gave it");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
