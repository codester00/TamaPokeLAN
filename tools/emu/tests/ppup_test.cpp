// PP UP (v3.92): a fifth more of a move's base PP, three a move slot, stored ON
// THE CREATURE (owner) so it travels with it -- banking, switching, the save,
// trades and LAN. Checked through the bag and the party ring, then each place
// the creature goes.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "items.h"
#include "pp.h"
#include "trade.h"
#include "link.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x99AA;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void partySlotPos(int i, int *x, int *y);
extern Pet pet;
extern Party party;
extern Inventory bag;
extern bool bagOpen, partyOpen;
extern uint8_t bagPage, vitPick, ppUpWho;
static const uint8_t PPUP_LIVE = 0xFE;   // the sketch's value

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const int CX = 233, CY = 233;
static void tapBagItem(ItemKey k) {
  uint8_t d = bag.distinctCount(), at = 0;
  for (uint8_t i = 0; i < d; i++) if (bag.keyAt(i) == k) at = i;
  bagOpen = true; bagPage = at / 5; render();
  onTap(CX, 112 + (at % 5) * 58 + 25);
}
static void tapRow(int s) { render(); onTap(CX, 112 + s * 58 + 25); }   // BAG_ROW_Y
static void tapSlot(int i) { int x, y; partySlotPos(i, &x, &y); onTap((int16_t)x, (int16_t)y); }

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  for (auto &m : party.slots) m = PartyMon();
  pet.moves[0] = MV_TACKLE; pet.moves[1] = MV_SURF; pet.moves[2] = 0; pet.moves[3] = 0;
  pet.ppUps = 0;

  // 1. The rule: a fifth of the base, rounded down, three a move.
  ck(ppMaxFor(MV_TACKLE, 0) == 35 && ppMaxFor(MV_TACKLE, 1) == 42 && ppMaxFor(MV_TACKLE, 3) == 56,
     "TACKLE 35 -> 42 ... 56 with three");
  ck(ppMaxFor(MV_SURF, 2) == 21 && ppMaxFor(MV_SURF, 9) == 24, "SURF 15 -> 21 with two, never past three");

  // 2. No party: the bag goes straight to the live pet's moves.
  bag.clear();
  bag.add(IT_PPUP, 5);
  tapBagItem(IT_PPUP);
  ck(ppUpWho == PPUP_LIVE, "no party: PP UP asks which of the live pet's moves");
  tapRow(0);
  ck(ppUpsAt(pet.ppUps, 0) == 1 && bag.count(IT_PPUP) == 4, "a tap on TACKLE gives it one PP Up");
  for (int n = 0; n < 2; n++) { tapBagItem(IT_PPUP); tapRow(0); }
  ck(ppUpsAt(pet.ppUps, 0) == 3 && bag.count(IT_PPUP) == 2, "three on one move");
  tapBagItem(IT_PPUP); tapRow(0);
  ck(ppUpsAt(pet.ppUps, 0) == 3 && bag.count(IT_PPUP) == 2, "a fourth is refused and kept");
  tapBagItem(IT_PPUP); tapRow(2);
  ck(bag.count(IT_PPUP) == 2 && pet.ppUps == 3, "an empty slot is refused and kept");

  // 3. The fight starts with the bigger PP, and the book counts from it.
  Combatant c;
  combatantFromPet(c, pet);
  ck(c.pp[0] == 56 && c.pp[1] == 15, "a fight starts TACKLE at 56, SURF at its 15");
  c.pp[0] = 10;                                   // 46 used: more than the base 35
  ppRecord(c.uid, c.moves, c.pp, c.ppUps);
  ck(ppLeftSlot(pet.uid, 0, MV_TACKLE, 3) == 10, "the book counts what is left from 56");
  ppFlush(); ppLoad();
  ck(ppLeftSlot(pet.uid, 0, MV_TACKLE, 3) == 10, "and keeps it across a reload (46 used is not clamped to 35)");
  ppRefillAll();

  // 4. It travels with the creature: banking, the save, a switch.
  PartyMon m = pet.toPartyMon();
  ck(ppUpsAt(m.reserved[1], 0) == 3, "banked, the record carries it");
  pet.saveNow();
  Pet q; q.begin();
  ck(q.ppUps == pet.ppUps, "the save carries it (pet checkpoint v7)");
  Combatant cp;
  combatantFromParty(cp, m);
  ck(cp.ppUps == pet.ppUps && cp.pp[0] == 56, "a party creature fights with its PP Ups");

  // 5. With a party, the ring picks whose move.
  PartyMon pm; pm.dex = 16; pm.level = 30; pm.moves[0] = MV_TACKLE; pm.moves[1] = MV_SURF;
  pm.uid = pet.mintUid();
  party.slots[0] = pm;
  tapBagItem(IT_PPUP);
  ck(vitPick == IT_PPUP && partyOpen, "with a party, the ring asks whose");
  tapSlot(0);
  ck(ppUpWho == 0 && bagOpen, "then which of its moves");
  tapRow(1);
  ck(ppUpsAt(party.slots[0].reserved[1], 1) == 1 && bag.count(IT_PPUP) == 1, "the party creature's SURF gets it");
  tapBagItem(IT_PPUP); onTap(CX, CY);             // the hub: the live pet
  tapRow(1);
  ck(ppUpsAt(pet.ppUps, 1) == 1 && bag.count(IT_PPUP) == 0, "the hub is the live pet");

  // 6. Trades and LAN keep it.
  ck(tradeMonValid(party.slots[0]), "a trade accepts a creature with PP Ups");
  PartyMon bad2 = party.slots[0]; bad2.reserved[2] = 1;
  ck(!tradeMonValid(bad2), "the still-reserved byte is still refused");
  Combatant lc; combatantFromParty(lc, party.slots[0]);
  LinkMon lm; linkMonFrom(lm, lc);
  Combatant back; linkMonTo(back, lm); battleFullPp(back);
  ck(back.ppUps == lc.ppUps && back.pp[1] == 18, "LAN: the other board counts SURF from 18");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
