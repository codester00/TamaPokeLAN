// Bill's PC's screens (v3.62) and the Pokedex's OWNED, GO TO # and the PC's
// missing-only filter (v3.63), all through the sketch's own tap handler.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "pcstore.h"
#include "pcfs_host.h"
#include "trade.h"
#include <cstdio>
uint32_t g_seed = 63; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
void partySlotPos(int i, int *cx, int *cy);
bool monSheetBtn(int16_t x, int16_t y, bool left);
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
int rpickRowAt(int16_t x, int16_t y, uint8_t mode);
void pcCellPos(uint8_t i, int *x, int *y);
int16_t pcListAt(uint16_t k);
uint16_t pcListCount();
void jumpKeyRect(uint8_t k, int *x, int *y);
bool dexJump(int16_t n, bool forPc);
uint8_t ownedCount(int16_t dex);
uint16_t ownedIn(int16_t lo, int16_t hi);
bool tradeRoomFor(const PartyMon &in);
uint8_t uiCurrentScreen();
extern Pet pet;
extern Trade trade;
extern bool partyOpen, partyPick, pcOpen, pcPick, pcSeventh, pcMissingOnly, jumpOpen, galleryOpen, galleryPick;
extern uint8_t pcRegion, pcPage, galleryRegion, galleryPage;
extern int16_t pcSpecies, pcJumpMark, galleryJumpMark;
extern uint16_t boxDetail, boxSel;
extern uint8_t boxSwapFrom;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint32_t serial = 500;
static PartyMon mon(int16_t dex) {
  PartyMon m; m.dex = dex; m.level = 25; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
  m.stateVersion = 1; m.uid.mac[0] = 0xEE; m.uid.serial = serial++; m.uid.salt = 9;
  return m;
}
static void tapSheetBtn(bool left) {
  for (int y = 300; y < 440; y += 4)
    for (int x = 40; x < 430; x += 4)
      if (monSheetBtn(x, y, left)) { onTap(x + 6, y + 6); return; }
}
static void tapYes() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); onTap(233, (a + b) / 2); }
static void tapKey(uint8_t k) { int x, y; jumpKeyRect(k, &x, &y); onTap(x + 30, y + 30); render(); }
static void tapRegionRow(uint8_t region) {
  for (int y = 90; y < 400; y += 2)
    if (rpickRowAt(233, y, 3) == region) { onTap(233, y + 4); render(); return; }
}
static bool tapCellOf(int16_t dex) {
  for (uint8_t i = 0; i < 16; i++) {
    if (pcListAt((uint16_t)(pcPage * 16 + i)) != dex) continue;
    int x, y; pcCellPos(i, &x, &y); onTap(x + 40, y + 40); render(); return true;
  }
  return false;
}

int main() {
  pcfsHostReset(PCFS_HOST_BLANK);
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(4, false);
  for (auto &m : party.slots) m = PartyMon();
  party.slots[0] = mon(16); party.slots[1] = mon(19);
  party.save();
  PartyMon r1 = mon(19), r2 = mon(19), abra = mon(63);
  pcPut(r1); pcPut(r2); pcPut(abra);

  // --- the hub opens Bill's PC on its region chooser, and a row opens the grid
  partyOpen = true; render();
  onTap(233, 233); render();
  ck(pcOpen && pcPick, "the party hub opens Bill's PC on its chooser");
  tapRegionRow(0);
  ck(pcOpen && !pcPick && pcRegion == 0 && pcSpecies == 0, "a region row opens that region's grid");

  // --- a held species opens its ring; the sheet sends one to the party
  pcPage = (19 - 1) / 16;               // Rattata is on the second page
  render();
  ck(tapCellOf(19) && pcSpecies == 19, "a held species opens its ring of six");
  int sx, sy; partySlotPos(1, &sx, &sy); onTap(sx, sy); render();
  ck(boxDetail != 0, "a creature in the ring opens its sheet");
  tapSheetBtn(true); render();
  ck(pcCount(19) == 1 && ownedCount(19) == 3, "TO PARTY moves it out of the PC into a free party slot");
  bool inParty = false;
  for (auto &m : party.slots) inParty |= (!m.empty() && m.uid == r2.uid);
  ck(inParty, "and it is in the party");

  // --- release from the ring, behind the confirmation
  partySlotPos(0, &sx, &sy); onTap(sx, sy); render();
  tapSheetBtn(false); render();
  tapYes(); render();
  ck(pcCount(19) == 0 && !pcFind(r1.uid, nullptr, nullptr), "RELEASE lets it go after YES");
  ck(pcSpecies == 0, "and an emptied ring closes itself");

  // --- deposit: an armed party slot and the hub
  onSwipeV(+1); render(); onSwipeV(+1); render();
  ck(!pcOpen && partyOpen, "down backs out of the grid and the chooser to the party");
  boxSwapFrom = 1;                       // slot 0, the Pidgey
  onTap(233, 233); render();
  ck(pcCount(16) == 1 && party.slots[0].empty(), "the hub with a slot armed deposits it in the PC");

  // --- a seventh of a species: one of the six makes room
  for (int i = 0; i < 6; i++) pcPut(mon(41));
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = mon(100 + i);
  party.save();
  PartyMon seventh = mon(41);
  pet.setEnded(seventh, CER_RELEASE);
  partyPick = true; partyOpen = true; render();
  onTap(233, 233); render();
  ck(pcOpen && pcSeventh && pcSpecies == 41, "a seventh Zubat's hub opens the Zubat ring to choose");
  PartyMon goner; pcGet(41, 2, goner);
  partySlotPos(2, &sx, &sy); onTap(sx, sy); render();
  ck(pcFind(seventh.uid, nullptr, nullptr) && !pcFind(goner.uid, nullptr, nullptr) && pcCount(41) == 6,
     "the tapped one goes and the newcomer takes its slot");
  ck(!partyPick && !pcOpen && pet.endedKind == CER_NONE, "and the newcomer is settled");

  // --- a trade with nowhere to put the incoming one is refused
  PartyMon zub = mon(41);
  trade.mine = PartyMon();
  int16_t d; uint8_t s;
  pcFind(abra.uid, &d, &s);
  trade.mine = abra;
  ck(!tradeRoomFor(zub), "giving a PC Abra for a seventh Zubat with a full party: no room");
  trade.mine = party.slots[0];
  ck(tradeRoomFor(zub), "giving a party creature: it takes that slot");
  trade.mine = PartyMon();

  // --- GO TO # on the PC grid, and the missing-only filter
  partyOpen = true; pcOpen = false; render();
  onTap(233, 233); render(); tapRegionRow(0);
  onTap(233, 84 + 4 * 80 + 8); render();
  ck(jumpOpen, "the PC grid's footer opens GO TO #");
  tapKey(0); tapKey(4); tapKey(9); tapKey(9); tapKey(9);   // 1 5 < < < ...
  tapKey(0); tapKey(4); tapKey(10); tapKey(11);            // 1 5 0 GO
  ck(!jumpOpen && pcRegion == 0 && pcPage == (150 - 1) / 16 && pcJumpMark == 150, "#150 lands on Mewtwo's page, outlined");
  pcMissingOnly = false;
  onTap(233, 84 + 4 * 80 + 30); render();
  ck(pcMissingOnly, "the filter switches to missing only");
  bool none = true;
  for (uint16_t k = 0; k < pcListCount(); k++) none = none && !ownedCount(pcListAt(k));
  ck(none && pcListAt(0) != 0 && pcListCount() < 151, "and lists only species not owned");
  ck(dexJump(41, true) && !pcMissingOnly && pcJumpMark == 41, "GO TO # turns the filter off to show what it asked for");
  ck(!dexJump(1026, true) && !dexJump(0, true), "a number past the Pokedex goes nowhere");

  // --- the Pokedex: GO TO # and OWNED
  pcOpen = false; partyOpen = false;
  galleryOpen = true; galleryPick = false; galleryRegion = 0; render();
  onTap(233, 84 + 4 * 80 + 8); render();
  ck(jumpOpen, "the Pokedex footer opens GO TO # too");
  tapKey(1); tapKey(4); tapKey(1); tapKey(11);   // 2 5 2 GO
  ck(!jumpOpen && galleryRegion == 2 && galleryPage == 0 && galleryJumpMark == 252, "#252 lands on Hoenn's first page");
  ck(ownedIn(1, 151) >= 4 && ownedCount(4) == 1, "OWNED counts the PC, the party and the live pet");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
