// The new prizes (v4.08, postgame-plan step 7). The prize counter's ONE list
// (items, the signature TMs not owned, five TMs a day the Mart is not
// offering); buying a TM through the screen; a signature move learnable only
// by its species, and only once the TM is owned, never by an opponent or a
// wild creature; PP MAX and the BOTTLE CAP through the bag, on the live pet
// and a party creature; the zone's symbols.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "items.h"
#include "pp.h"
#include "tms.h"
#include "zone.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t g_seed = 0x408; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
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
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
void partySlotPos(int i, int *x, int *y);
uint8_t learnableFor(int16_t dex, uint8_t lvl, uint8_t *out, uint8_t max);
uint8_t zonePrizePages();
extern Pet pet;
extern Party party;
extern Inventory bag;
extern bool plazaOpen, zoneOpen, bagOpen, partyOpen;
extern uint8_t zoneMode, zoneConfirm, zonePage, bagPage, vitPick, ppUpWho;
static const uint8_t PPUP_LIVE = 0xFE;
static const uint8_t ZONE_ROWS_T = 3;   // the sketch's ZONE_ROWS

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static const char *scr() { return SCREEN_NAME[uiCurrentScreen()]; }
static void toPlaza() { for (int i = 0; i < 16 && !plazaOpen; i++) uiTileGo(i); }
static void tapPlaza(uint8_t i) { int x, y, w, h; plazaBtnRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapZoneRow(uint8_t i) { render(); int x, y, w, h; zoneRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
static void tapYes() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); onTap(233, (a + b) / 2); }
static void tapBagItem(ItemKey k) {
  uint8_t d = bag.distinctCount(), at = 0;
  for (uint8_t i = 0; i < d; i++) if (bag.keyAt(i) == k) at = i;
  bagOpen = true; bagPage = at / 5; render();
  onTap(233, 112 + (at % 5) * 58 + 25);
}
static void tapBagRow(int s) { render(); onTap(233, 112 + s * 58 + 25); }   // BAG_ROW_Y
static void tapSlot(int i) { int x, y; partySlotPos(i, &x, &y); onTap((int16_t)x, (int16_t)y); }
static bool learnsNow(int16_t dex, uint8_t lvl, uint8_t mv) {
  uint8_t l[255]; uint8_t n = learnableFor(dex, lvl, l, sizeof(l));
  for (uint8_t i = 0; i < n; i++) if (l[i] == mv) return true;
  return false;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6, false);
  for (auto &m : party.slots) m = PartyMon();
  uint32_t day = pet.today();

  // --- 1. the data: ten signature moves, apart from the ordinary TMs ----------
  ck(SIG_TM_COUNT == 10 && MOVE_COUNT == 252, "ten signature moves appended, four indices left");
  bool apart = true;
  for (uint16_t i = 0; i < TM_COUNT; i++) if (moveIsSignature(TM_MOVES[i])) apart = false;
  ck(apart && TM_COUNT == 134, "TM_MOVES (the Mart, drops, the Game Corners) holds none of them");
  // the learners: exactly the SIGNATURE species, as TM rows
  int learners = 0, wrongLvl = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    for (uint8_t i = 0; i < learnCount(d); i++)
      if (moveIsSignature(learnMove(d, i))) { learners++; if (learnLevel(d, i)) wrongLvl++; }
  ck(learners == 7 + 27 && !wrongLvl, "34 learner rows (7 legendaries, 27 starters), all TM rows");
  bool roarDialga = false;
  for (uint8_t i = 0; i < learnCount(483); i++) if (learnMove(483, i) == MV_ROAR_OF_TIME) roarDialga = true;
  ck(roarDialga, "DIALGA learns ROAR OF TIME");

  // --- 2. a signature move: its species, the TM, never an opponent ---------------
  pet.dbgHatchAs(6, false);                         // CHARIZARD
  ck(!learnsNow(6, 50, MV_BLAST_BURN), "CHARIZARD without the TM: no BLAST BURN");
  pet.giveTm(MV_BLAST_BURN);
  ck(learnsNow(6, 50, MV_BLAST_BURN), "with it: BLAST BURN in the picker");
  ck(!learnsNow(25, 50, MV_BLAST_BURN) && !learnsNow(5, 50, MV_BLAST_BURN),
     "PIKACHU and CHARMELEON never, TM or not: its species only");
  ck(!learnsNow(6, 50, MV_HYDRO_CANNON), "and not another starter's ultimate");
  bool opp = false;
  for (uint8_t i = 0; i < learnCount(6); i++)
    if (learnMove(6, i) == MV_BLAST_BURN && moveUnlockLevel(6, i) <= 100) opp = true;
  ck(!opp, "the opponents' gate never opens for it, even at 100");
  {
    Pet foe;
    bool got = false;
    int16_t legends[] = { 6, 483, 487, 644, 890, 9, 3 };
    for (int16_t d : legends) {
      foe.dbgHatchAs(d, false);
      foe.ageMinutes = 100UL * 60;
      foe.relearnFromLevel();
      for (int s = 0; s < MOVE_SLOTS; s++) if (moveIsSignature(foe.moves[s])) got = true;
      for (int r = 0; r < 40; r++) {
        foe.rollWildMoves();
        for (int s = 0; s < MOVE_SLOTS; s++) if (moveIsSignature(foe.moves[s])) got = true;
      }
    }
    foe.dbgHatchAs(235, false);                     // SMEARGLE: the whole list
    for (int r = 0; r < 400; r++) {
      foe.rollWildMoves();
      for (int s = 0; s < MOVE_SLOTS; s++) if (moveIsSignature(foe.moves[s])) got = true;
    }
    ck(!got, "no trainer roster, wild creature or wild SMEARGLE at 100 ever holds one");
  }
  pet.tmOwned[MV_BLAST_BURN >> 3] &= (uint8_t)~(1 << (MV_BLAST_BURN & 7));

  // --- 3. the prize counter's list ------------------------------------------------
  uint8_t n = zonePrizeCount(pet, day);
  ck(n == ZONE_PRIZE_COUNT + SIG_TM_COUNT + ZONE_TM_PER_DAY, "items, ten signature TMs, five TMs a day");
  uint8_t mart[MART_TM_PER_DAY];
  uint8_t nm = martTmToday(pet, day, mart);
  bool listOk = true, sigOk = true, priceOk = true;
  std::vector<uint8_t> seen;
  for (uint8_t i = 0; i < n; i++) {
    ZonePrizeRow z;
    if (!zonePrizeAt(pet, day, i, &z)) { listOk = false; continue; }
    if (i < ZONE_PRIZE_COUNT) { if (z.kind != ZPR_ITEM) listOk = false; continue; }
    if (z.kind != ZPR_TM || pet.hasTm(z.what)) listOk = false;
    for (uint8_t s : seen) if (s == z.what) listOk = false;
    seen.push_back(z.what);
    bool sig = i < ZONE_PRIZE_COUNT + SIG_TM_COUNT;
    if (sig != moveIsSignature(z.what)) sigOk = false;
    if (!sig) {
      for (uint8_t j = 0; j < nm; j++) if (mart[j] == z.what) listOk = false;
      if (z.bp != tmPrice(z.what) / 250 || z.bp < 6 || z.bp > 20) priceOk = false;
    } else if (z.bp != zoneSigTmBp(z.what)) priceOk = false;
  }
  ZonePrizeRow past;
  ck(listOk && !zonePrizeAt(pet, day, n, &past), "every row resolves, distinct, unowned, none of the Mart's five");
  ck(sigOk, "signature TMs first, then the day's ordinary ones");
  ck(priceOk, "TM BP: Mart price / 250 (6-20), a signature 48 or 32");
  ck(nm == MART_TM_PER_DAY, "the Mart still has its five that day");
  // one day can miss an overlap by chance: 400 days cannot (~1 in 6 would clash)
  {
    int clash = 0, full = 0;
    for (uint32_t d = 1; d <= 400; d++) {
      uint8_t m5[MART_TM_PER_DAY], z5[ZONE_TM_PER_DAY];
      uint8_t a = martTmToday(pet, d, m5), b = zoneTmToday(pet, d, z5);
      if (b == ZONE_TM_PER_DAY) full++;
      for (uint8_t i = 0; i < a; i++) for (uint8_t j = 0; j < b; j++) if (m5[i] == z5[j]) clash++;
    }
    ck(!clash && full == 400, "400 days: never one of the Mart's five, always five");
  }
  {
    uint8_t a[ZONE_TM_PER_DAY], b[ZONE_TM_PER_DAY];
    uint8_t na = zoneTmToday(pet, day, a), nb = zoneTmToday(pet, day, b);
    bool same = na == nb;
    for (uint8_t i = 0; same && i < na; i++) same = a[i] == b[i];
    uint8_t c[ZONE_TM_PER_DAY]; uint8_t nc = zoneTmToday(pet, day + 1, c);
    bool differ = nc != na;
    for (uint8_t i = 0; !differ && i < na; i++) differ = a[i] != c[i];
    ck(same && differ, "the same all day, another the next");
  }

  // --- 4. buying a TM through the screen --------------------------------------------
  for (uint8_t r = 0; r < GYM_REGIONS; r++) pet.winBadge(r, TRAINER_COUNT - 1, false);
  toPlaza();
  tapPlaza(3);
  tapZoneRow(2);
  ck(!strcmp(scr(), "zone") && zoneMode == 1, "the prize counter is open");
  ck(zonePrizePages() == (n + ZONE_ROWS_T - 1) / ZONE_ROWS_T, "it pages over the whole list");
  uint8_t at = ZONE_PRIZE_COUNT;                     // the first signature TM
  ZonePrizeRow first; zonePrizeAt(pet, day, at, &first);
  pet.bp = 100;
  zonePage = at / ZONE_ROWS_T;
  tapZoneRow(at % ZONE_ROWS_T);
  ck(zoneConfirm == at, "a TM row asks first");
  tapYes();
  ck(pet.hasTm(first.what) && pet.bp == 100 - zoneSigTmBp(first.what), "YES: the TM is owned, its BP spent");
  ck(zonePrizeCount(pet, day) == n - 1, "and it leaves the list (TMs are reusable)");
  ZonePrizeRow now; zonePrizeAt(pet, day, at, &now);
  ck(now.what != first.what, "the next one takes its row");
  // an ordinary TM through the same path
  uint8_t ot = (uint8_t)(n - 1 - 1);                 // the last row of the shorter list
  ZonePrizeRow ord; zonePrizeAt(pet, day, ot, &ord);
  pet.bp = 50;
  zonePage = ot / ZONE_ROWS_T;
  tapZoneRow(ot % ZONE_ROWS_T); tapYes();
  ck(ord.kind == ZPR_TM && !moveIsSignature(ord.what) && pet.hasTm(ord.what) && pet.bp == 50 - ord.bp,
     "an ordinary TM: owned, its BP spent");
  pet.bp = 0;
  zonePage = 0;
  // PP MAX and the BOTTLE CAP are items at the counter
  bool ppmax = false, cap = false;
  for (uint8_t i = 0; i < ZONE_PRIZE_COUNT; i++) {
    if (ZONE_PRIZES[i].item == IT_PPMAX) ppmax = true;
    if (ZONE_PRIZES[i].item == IT_BOTTLECAP) cap = true;
  }
  ck(ppmax && cap && !itemForSale(IT_PPMAX) && !itemForSale(IT_BOTTLECAP) &&
     !itemEntry(IT_PPMAX).dropWeight && !itemEntry(IT_BOTTLECAP).dropWeight,
     "PP MAX and BOTTLE CAP: the counter only, never the Mart or a drop");
  zoneOpen = false; plazaOpen = false;
  uiTileGo(4);

  // --- 5. PP MAX through the bag -------------------------------------------------------
  pet.dbgHatchAs(6, false);
  pet.moves[0] = MV_FLAMETHROWER; pet.moves[1] = MV_SCRATCH; pet.moves[2] = 0; pet.moves[3] = 0;
  pet.ppUps = 0;
  ppUpAdd(pet.ppUps, pet.moves, 1);                 // slot 1 already has one
  bag.add(IT_PPMAX); bag.add(IT_PPMAX);
  tapBagItem(IT_PPMAX);
  ck(ppUpWho == PPUP_LIVE, "no party: PP MAX goes straight to the live pet's moves");
  tapBagRow(0);
  ck(ppUpsAt(pet.ppUps, 0) == PP_UPS_MAX && bag.count(IT_PPMAX) == 1, "a slot with none: all three at once, one spent");
  tapBagItem(IT_PPMAX); tapBagRow(1);
  ck(ppUpsAt(pet.ppUps, 1) == PP_UPS_MAX && bag.count(IT_PPMAX) == 0, "a slot with one: up to three, one spent");
  bag.add(IT_PPMAX);
  tapBagItem(IT_PPMAX); tapBagRow(0);
  ck(ppUpsAt(pet.ppUps, 0) == PP_UPS_MAX && bag.count(IT_PPMAX) == 1, "already full: refused, kept");
  tapBagItem(IT_PPUP);   // PP UP still one at a time (none in the bag: a no-op tap)
  bag.add(IT_PPUP);
  pet.moves[2] = MV_TACKLE;
  tapBagItem(IT_PPUP); tapBagRow(2);
  ck(ppUpsAt(pet.ppUps, 2) == 1, "PP UP is still one Up");

  // --- 6. the BOTTLE CAP ---------------------------------------------------------------
  pet.ivAtk = 10; pet.ivDef = 31; pet.ivSpe = 12; pet.ivHp = 9;
  uint8_t trA = pet.trAtk;
  bag.add(IT_BOTTLECAP); bag.add(IT_BOTTLECAP); bag.add(IT_BOTTLECAP);
  tapBagItem(IT_BOTTLECAP);
  ck(ppUpWho == PPUP_LIVE, "a BOTTLE CAP asks which IV");
  tapBagRow(0);
  ck(pet.ivAtk == 31 && pet.ivSpe == 12 && pet.ivHp == 9 && bag.count(IT_BOTTLECAP) == 2,
     "ATTACK's IV to 31, the others untouched, one spent");
  ck(pet.trAtk == trA, "training untouched");
  tapBagItem(IT_BOTTLECAP); tapBagRow(1);
  ck(pet.ivDef == 31 && bag.count(IT_BOTTLECAP) == 2, "an IV already 31: refused, kept");
  tapBagItem(IT_BOTTLECAP); tapBagRow(3);
  ck(pet.ivHp == 31 && bag.count(IT_BOTTLECAP) == 1, "the fourth row is HP");
  // on a party creature, through the ring, and it is saved
  PartyMon pm = pet.toPartyMon();
  pm.ivSpe = 5;
  party.slots[0] = pm; party.save();
  tapBagItem(IT_BOTTLECAP);
  ck(partyOpen && vitPick == IT_BOTTLECAP, "with a party: who first, on the ring");
  tapSlot(0);
  ck(ppUpWho == 0, "then which IV");
  tapBagRow(2);
  ck(party.slots[0].ivSpe == 31 && bag.count(IT_BOTTLECAP) == 0, "the party creature's SPEED IV to 31");
  party.slots[0] = PartyMon();
  party.begin();
  ck(party.slots[0].ivSpe == 31, "and saved");
  // PP MAX on the party creature too
  bag.add(IT_PPMAX);
  party.slots[0].reserved[1] = 0; party.save();
  tapBagItem(IT_PPMAX); tapSlot(0); tapBagRow(0);
  ck(ppUpsAt(party.slots[0].reserved[1], 0) == PP_UPS_MAX, "PP MAX on a party creature");
  for (auto &m : party.slots) m = PartyMon();
  party.save();

  // --- 7. the zone's symbols -------------------------------------------------------------
  ck(zoneSymbols(0, 0) == 0, "nothing won: no symbol");
  ck(zoneSymbols(1, 0) == ZSYM_TOUR, "the first tournament won");
  ck(zoneSymbols(0, ZONE_SYM_SILVER_STREAK - 1) == 0 && zoneSymbols(0, ZONE_SYM_SILVER_STREAK) == ZSYM_SILVER,
     "silver at its streak, not one before");
  ck(zoneSymbols(3, ZONE_SYM_GOLD_STREAK) == (ZSYM_TOUR | ZSYM_SILVER | ZSYM_GOLD), "gold keeps silver");

  printf(bad ? "\n%d FAILED\n" : "\nALL PASS\n", bad);
  return bad ? 1 : 0;
}
