// A simulated playthrough of the real firmware (v3.86 audit, v3.92 economy). Not a test.
//
//   bash tools/emu/playsim.sh <profile> <seed> <days> > run.jsonl
//
// A bot plays a fresh save the way a person would, through the sketch's own
// entry points -- the real loop() runs between its actions with the clock
// moving, so ageing, sleep, saves, evolutions offered, farewells, eggs and the
// party handover are the firmware's own:
//   - care at set session times (feed, clean, play, a training game, petting),
//     to bed at night with the screen off, up in the morning;
//   - fights through battleTap(), the screen's handler: FIGHT and a move the
//     hard AI would pick, a ball from the battle bag for anything not yet
//     registered (a Master Ball for a legendary), BALL in the Safari;
//   - gym leaders (startTrainerBattle) when the team is ready, Explore
//     (startWildBattle) otherwise, in the open area with the most species not
//     yet registered;
//   - the Mart (spendWallet + the bag, as its confirm does): balls, the stone
//     or Linking Cord an evolution needs; then, from what is left over a
//     reserve, X items for the gyms, vitamins and PP Ups (v3.90-v3.92);
//   - SELLING (martSellOne, the Mart's own) of what it will not use: spare
//     potions, spare stones;
//   - vitamins and PP Ups through the bag screen's taps (onTap), X items and
//     potions through the battle bag, as a player would;
//   - evolutions as offered, stones from the bag, the cord, Applin's choice of
//     a branch not registered, and banked creatures swapped in to evolve;
//   - the farewell when offered, the next egg from the region with the most
//     left to register.
// Each event is one JSON line on stdout; tools/emu/playsim_report.py reads them.
//
// Profiles (v3.92 rerun; the board counts steps only while it is powered and
// carried, and a child leaves it home for school):
//   child   school days 3 sessions, 8 fights,   300 steps; weekends 4, 15, 3,000
//   walker  weekdays    3 sessions, 10 fights, 1,500 steps; weekends 4, 15, 6,000
//   adult   every day   2 sessions, 6 fights,  4,000 steps (an upper bound)
// 2026-01-01, the first day, is a Thursday.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "wild.h"
#include "wild_areas.h"
#include "battle.h"
#include "trainers.h"
#include "pcstore.h"
#include "pp.h"
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <set>
#include <map>
#include <string>

uint32_t g_seed = 1;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { printf("{\"ev\":\"restart\"}\n"); exit(2); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

// ---- the sketch ------------------------------------------------------------------
uint32_t rtcEpoch(); void rtcSetEpoch(uint32_t e);
void setup(); void loop();
void battleTap(int16_t x, int16_t y);
bool startWildBattle(uint8_t region, bool hard);
void startTrainerBattle(uint8_t idx, bool hard);
int btlCellIndexAt(int16_t x, int16_t y);
void focusSwap(uint8_t slot);
bool boxViewSwap(uint8_t partyIdx, uint16_t idx);
uint16_t boxViewIndexOf(int16_t dex, uint8_t slot);
void stoneFromBag(uint8_t k);
void shedinjaCheck();
bool martSellOne(uint8_t k);
void onTap(int16_t x, int16_t y);
void render();
void partySlotPos(int i, int *cx, int *cy);
extern bool bagOpen;
extern uint8_t bagPage, vitPick, ppUpWho, btlBagPage;
extern uint8_t btlTrainer;
extern bool btlLink;
extern Pet pet; extern Party party; extern Inventory bag;
extern bool battleOpen, btlOver, btlWon, btlLost, blackoutOpen, btlWild, btlSafari, screenOff;
extern bool partyPick, partyOpen, gymHard, exploreOpen;
extern uint8_t btlMsgCount, btlMenu, gymRegion, exploreRegion, squadOrderN;
extern uint8_t squadOrder[];
extern uint16_t squadMask, exploreAreaPick[];
extern uint32_t btlWinUntil;
extern int16_t wildDex;
extern uint8_t wildLvl;
extern int8_t wildLegend, wildFixed;
extern bool wildCaught;
uint8_t ownedCount(int16_t dex);
extern Combatant btlYou, btlFoe;

// ---- the profile ------------------------------------------------------------------
struct DayPlan { uint8_t sessions; uint16_t fights; uint32_t steps; };
struct Profile { const char *name; DayPlan weekday, weekend; bool swapIn; };
static const Profile PROFILES[] = {
  { "child",  { 3, 8, 300 },   { 4, 15, 3000 }, true },
  { "walker", { 3, 10, 1500 }, { 4, 15, 6000 }, true },
  { "adult",  { 2, 6, 4000 },  { 2, 6, 4000 },  false },
};
static Profile P;

// ---- the clock --------------------------------------------------------------------
static const uint32_t START_EPOCH = 1767225600UL;   // 2026-01-01 00:00 (a Thursday)
static uint32_t nowEpoch() { return rtcEpoch(); }
static int day() { return (int)((nowEpoch() - START_EPOCH) / 86400); }
static int hourOf() { return (int)((nowEpoch() / 3600) % 24); }
static bool weekend() { int wd = (day() + 4) % 7; return wd == 0 || wd == 6; }   // 0 = Sunday
static const DayPlan &plan() { return weekend() ? P.weekend : P.weekday; }
static uint8_t sessionHour(uint8_t i) {
  static const uint8_t two[] = { 8, 19 }, three[] = { 7, 16, 19 }, four[] = { 9, 12, 16, 19 };
  uint8_t n = plan().sessions;
  return n == 2 ? two[i] : n == 3 ? three[i] : four[i];
}
// Lets the game run for `ms`, in slices so loop() sees every minute's tick.
static void run(uint64_t ms, uint32_t slice = 60000) {
  while (ms) {
    uint32_t s = ms > slice ? slice : (uint32_t)ms;
    emuAdvanceMs(s);
    loop();
    ms -= s;
  }
}

// ---- bookkeeping ------------------------------------------------------------------
static int nRaised = 0;
static int nFights = 0, nWild = 0, nCatch = 0, nCatchFail = 0, nGym = 0, nGymWin = 0, nBlack = 0;
static int nFarewell = 0, nEvolve = 0, nStonesFound = 0, nStonesBought = 0, nCords = 0, nSwaps = 0;
static uint64_t moneyIn = 0, moneyOut = 0;
// where the money came from and went (v3.92 rerun)
static uint64_t inSteps = 0, inPrize = 0, inSold = 0;
static uint64_t outBalls = 0, outStones = 0, outCords = 0, outX = 0, outVit = 0, outPpUp = 0, outHeal = 0;
static int nSold = 0, nVit = 0, nVitMax = 0, nXUsed = 0, nPpUp = 0, nHealUsed = 0, nUiMiss = 0;
static uint64_t *spendBucket = &outBalls;   // what buy() counts the money against
static std::set<int16_t> target;
static void ev(const char *fmt, ...) {
  char b[8192];                                 // the target list is ~4 KB
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  printf("{\"day\":%d,\"h\":%d,%s}\n", day(), hourOf(), b);
}
static uint16_t registeredNow() { return pet.registeredCount(); }
// A species newly registered since the last look, with the cause.
static std::vector<bool> seen, acquired;
// A species is still wanted until the player has owned one by any means --
// caught, hatched, evolved or won. (The Pokedex REGISTERS a species only when
// it is raised; a catch makes it OWNED. The goal is every species acquired.)
static bool wanted(int16_t d) { return d > 0 && d <= DEX_COUNT && speciesShowable(d) && !acquired[d]; }
static int targetLeft() {
  return (int)std::count_if(target.begin(), target.end(), [](int16_t t) { return !acquired[t]; });
}
static void noteAcquired(int16_t d, const char *how) {
  if (d <= 0 || d > DEX_COUNT || acquired[d]) return;
  acquired[d] = true;
  ev("\"ev\":\"got\",\"dex\":%d,\"name\":\"%s\",\"how\":\"%s\",\"left\":%d", d, DEX_TBL[d].name, how, targetLeft());
}
static void noteRegistered(const char *how) {
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (!seen[d] && pet.isRegistered(d)) {
      seen[d] = true;
      // an egg can hatch while the bot waits after an evolution: the dex knows
      // which a species is (an evolution-only one never hatches)
      if (!strcmp(how, "evolve") || !strcmp(how, "hatch")) how = DEX_TBL[d].rarity == R_EVO ? "evolve" : "hatch";
      noteAcquired(d, how);
      ev("\"ev\":\"reg\",\"dex\":%d,\"name\":\"%s\",\"how\":\"%s\",\"total\":%u,\"target_left\":%d",
         d, DEX_TBL[d].name, how, registeredNow(),
         targetLeft());
    }
}

// ---- the battle screen ------------------------------------------------------------
static int16_t cellX[8], cellY[8];
static bool cellsKnown = false;
static void findCells() {
  if (cellsKnown) return;
  for (int i = 0; i < 4; i++) cellX[i] = cellY[i] = -1;
  for (int16_t y = 200; y < 466; y += 3)
    for (int16_t x = 0; x < 466; x += 3) {
      int c = btlCellIndexAt(x, y);
      if (c >= 0 && c < 4 && cellX[c] < 0) { cellX[c] = x + 10; cellY[c] = y + 10; }
    }
  cellsKnown = cellX[0] >= 0 && cellX[3] >= 0;
}
// A person's tap takes a moment: the screen's lunges, faints and HP bars run
// on the clock, so each tap lets 400 ms of game time pass first.
static void run(uint64_t ms, uint32_t slice);
static void tapAt(int16_t x, int16_t y) { run(400, 100); battleTap(x, y); }
static void tapCell(int i) { tapAt(cellX[i], cellY[i]); }
// Where a ball sits in the battle bag: the bag's usable items, in its order.
static int bagIndexOf(ItemKey want) {
  uint8_t n = 0, d = bag.distinctCount();
  for (uint8_t i = 0; i < d; i++) {
    ItemKey k = bag.keyAt(i);
    if (!itemUsableInBattle(k)) continue;
    if (k == want) return n;
    n++;
  }
  return -1;
}
static ItemKey bestBall(bool legend) {
  if (legend && bag.count(IT_MASTERBALL)) return IT_MASTERBALL;
  if (bag.count(IT_ULTRABALL)) return IT_ULTRABALL;
  if (bag.count(IT_GREATBALL)) return IT_GREATBALL;
  if (bag.count(IT_POKEBALL)) return IT_POKEBALL;
  return ITEM_NONE;
}
// Plays a fight out through the screen. Returns +1 won (or caught), -1 lost, 0 fled/other.
// The lead's stronger side, by its best attacking move.
static uint8_t bestCategory(const Combatant &c) {
  int phys = 0, spec = 0;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    uint8_t mv = c.moves[i];
    if (!mv || mv >= MOVE_COUNT) continue;
    int p = MOVE_TBL[mv].power;
    if (MOVE_TBL[mv].cat == MC_PHYS) phys = std::max(phys, p); else if (MOVE_TBL[mv].cat == MC_SPEC) spec = std::max(spec, p);
  }
  return spec > phys ? MC_SPEC : MC_PHYS;
}
// BAG, then the item's page and cell, through the battle screen's taps.
static bool useBattleItem(ItemKey k) {
  int idx = bagIndexOf(k);
  if (idx < 0) return false;
  uint8_t before = bag.count(k);
  tapCell(1);
  if (btlMenu != 3) return false;
  btlBagPage = (uint8_t)(idx / 4);
  tapCell(idx % 4);
  // taken = the bag went down, or the screen answered with its lines (a catch
  // can drop the same ball straight back into the bag)
  bool took = bag.count(k) < before || (btlMenu == 0 && btlMsgCount > 0);
  if (!took) {
    nUiMiss++;
    ev("\"ev\":\"ui_miss\",\"what\":\"battle_bag\",\"item\":%u,\"idx\":%d,\"menu\":%u,\"msgs\":%u,\"wild\":%d,\"safari\":%d",
       (unsigned)k, idx, btlMenu, btlMsgCount, btlWild, btlSafari);
  }
  if (!took) { if (btlMenu == 3) { tapAt(5, 5); btlMenu = 0; } return false; }
  return true;
}
static int playFight(bool wantCatch) {
  findCells();
  int throws = 0;
  bool xUsedThisFight = false;
  int healsUsed = 0;
  static int traced = 0;
  bool trace = getenv("PLAYSIM_TRACE") && traced++ < 1;
  for (int guard = 0; guard < 4000 && battleOpen; guard++) {
    if (trace && guard < 40) fprintf(stderr, "g%d menu=%u msgs=%u win=%u over=%d you=%u foe=%u/%u\n", guard, btlMenu, btlMsgCount, btlWinUntil, btlOver, btlYou.hp, btlFoe.hp, btlFoe.maxHp);
    if (btlWinUntil || btlMsgCount) { tapAt(233, 233); continue; }
    if (btlSafari) { tapCell(0); throws++; continue; }        // BALL
    if (btlMenu != 0) { tapAt(5, 5); btlMenu = 0; continue; }
    ItemKey ball = wantCatch && btlWild ? bestBall(wildLegend >= 0) : ITEM_NONE;
    // weaken first: throw once the foe is under 60%, or at once if a hit might KO it
    // a much stronger lead would knock it out with one hit: a person throws at once
    bool weak = btlFoe.hp * 100 < btlFoe.maxHp * 60 || btlYou.level >= btlFoe.level + 8;
    if (ball != ITEM_NONE && (weak || throws > 0) && useBattleItem(ball)) { throws++; continue; }
    // a trainer fight: an X item on the lead's first turn, a potion when low
    if (!btlWild && !btlLink) {
      if (!xUsedThisFight && btlYou.hp == btlYou.maxHp) {
        xUsedThisFight = true;
        ItemKey x = bestCategory(btlYou) == MC_SPEC ? IT_XSPATK : IT_XATTACK;
        if (bag.count(x) && useBattleItem(x)) { nXUsed++; continue; }
      }
      if (btlYou.hp * 100 < btlYou.maxHp * 30 && btlFoe.hp * 100 > btlFoe.maxHp * 25 && healsUsed < 3) {
        ItemKey h = bag.count(IT_HYPERPOTION) ? IT_HYPERPOTION : bag.count(IT_SUPERPOTION) ? IT_SUPERPOTION
                  : bag.count(IT_POTION) ? IT_POTION : ITEM_NONE;
        if (h != ITEM_NONE && useBattleItem(h)) { healsUsed++; nHealUsed++; continue; }
      }
    }
    tapCell(0);                         // FIGHT
    if (trace && guard < 40) fprintf(stderr, "  after FIGHT menu=%u\n", btlMenu);
    if (btlMenu != 1) continue;
    uint8_t mv = aiChooseMove(btlYou, btlFoe, true);   // a move number, not a slot
    uint8_t slot = 0;
    for (uint8_t i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i] == mv) slot = i;
    tapCell(slot);
    if (trace && guard < 40) fprintf(stderr, "  slot %u (move %u pp?) -> menu=%u msgs=%u\n", slot, btlYou.moves[slot], btlMenu, btlMsgCount);
    if (btlMenu == 1) { for (int i = 0; i < 4 && btlMenu == 1; i++) tapCell(i); }
  }
  if (battleOpen) {
    ev("\"ev\":\"stuck_fight\",\"menu\":%u,\"msgs\":%u,\"over\":%d,\"safari\":%d,\"cells\":\"%d,%d %d,%d %d,%d %d,%d\",\"you\":%u,\"foe\":%u,\"mv\":\"%u %u %u %u\"",
       btlMenu, btlMsgCount, btlOver, btlSafari, cellX[0], cellY[0], cellX[1], cellY[1], cellX[2], cellY[2], cellX[3], cellY[3],
       btlYou.hp, btlFoe.hp, btlYou.moves[0], btlYou.moves[1], btlYou.moves[2], btlYou.moves[3]);
    battleOpen = false;
  }
  bool lost = blackoutOpen;
  if (blackoutOpen) { blackoutOpen = false; nBlack++; }
  run(1000, 1000);
  return lost ? -1 : (btlWon ? 1 : 0);
}

// ---- the team ---------------------------------------------------------------------
// The six highest-level creatures into the party (from the PC), then the squad:
// the live pet only if it is among the best and has energy to spend.
static void arrangeTeam() {
  if (pet.isEgg()) return;
  for (int pass = 0; pass < 6; pass++) {
    int weakSlot = -1, weakLvl = 999;
    for (int i = 0; i < PARTY_SLOTS; i++) {
      int l = party.slots[i].empty() ? -1 : party.slots[i].level;
      if (l < weakLvl) { weakLvl = l; weakSlot = i; }
    }
    int16_t bestDex = 0; uint8_t bestSlot = 0; int bestLvl = weakLvl;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      for (uint8_t s = 0; s < pcCount(d); s++) {
        PartyMon m;
        if (pcGet(d, s, m) && !m.empty() && m.level > bestLvl + 2) { bestLvl = m.level; bestDex = d; bestSlot = s; }
      }
    if (!bestDex || weakSlot < 0) break;
    if (!boxViewSwap((uint8_t)weakSlot, boxViewIndexOf(bestDex, bestSlot))) break;
  }
  // squad: party by level, the pet in if it is strong and rested
  int order[PARTY_SLOTS + 1]; int n = 0;
  for (int i = 0; i < PARTY_SLOTS; i++) if (!party.slots[i].empty()) order[n++] = i + 1;
  std::sort(order, order + n, [](int a, int b) { return party.slots[a - 1].level > party.slots[b - 1].level; });
  bool petIn = pet.energy >= 40 && (n < 3 || pet.level() >= party.slots[order[std::min(n, 6) - 1] - 1].level);
  squadMask = 0; squadOrderN = 0;
  if (petIn) { squadMask |= 1; squadOrder[squadOrderN++] = 0; }
  for (int i = 0; i < n && squadOrderN < 6; i++) { squadMask |= (uint16_t)(1 << order[i]); squadOrder[squadOrderN++] = (uint8_t)order[i]; }
  if (!squadMask) { squadMask = 1; squadOrder[squadOrderN++] = 0; }
}
static int teamLevel(int k) {          // the k-th best level on the team (1-based)
  std::vector<int> l;
  if (!pet.isEgg()) l.push_back(pet.level());
  for (int i = 0; i < PARTY_SLOTS; i++) if (!party.slots[i].empty()) l.push_back(party.slots[i].level);
  std::sort(l.rbegin(), l.rend());
  if (l.empty()) return 0;
  return l[std::min((int)l.size(), k) - 1];
}

// ---- shopping -----------------------------------------------------------------------
static bool buy(ItemKey k, uint8_t n = 1) {
  uint32_t price = itemEntry(k).price * n;
  if (!itemForSale(k) || pet.wallet < price) return false;
  if (!pet.spendWallet(price)) return false;
  bag.add(k, n);
  moneyOut += price;
  *spendBucket += price;
  return true;
}
static void stockBalls() {
  spendBucket = &outBalls;
  if (bag.count(IT_ULTRABALL) < 10 && pet.wallet > 20000) buy(IT_ULTRABALL, 5);
  if (bag.count(IT_GREATBALL) < 10 && pet.wallet > 6000) buy(IT_GREATBALL, 5);
  if (bag.count(IT_POKEBALL) < 10) buy(IT_POKEBALL, 5);
  spendBucket = &outHeal;
  if (bag.count(IT_SUPERPOTION) < 3 && pet.wallet > 5000) buy(IT_SUPERPOTION, 3);
  spendBucket = &outBalls;
}
// What a player keeps: balls, 5 of each potion, 2 of each stone, every item it
// uses (vitamins, X items, PP Up, cords). The rest goes back to the Mart.
static void sellSpares() {
  for (uint8_t i = 0; i < bag.distinctCount();) {
    ItemKey k = bag.keyAt(i);
    const ItemEntry &e = itemEntry(k);
    uint8_t keep = e.category == IC_HEAL || e.category == IC_CURE ? 5 : e.category == IC_STONE ? 2 : 255;
    uint8_t had = bag.distinctCount();
    while (bag.count(k) > keep) {
      uint32_t w0 = pet.wallet;
      if (!martSellOne(k)) break;
      inSold += pet.wallet - w0; moneyIn += pet.wallet - w0; nSold++;
    }
    if (bag.distinctCount() == had) i++;   // the row is still there: next one
  }
}
// The bag screen, tapped as a person would (BAG_ROW_Y rows of 5 a page).
static void tapBagItem(ItemKey k) {
  uint8_t d = bag.distinctCount(), at = 0;
  for (uint8_t i = 0; i < d; i++) if (bag.keyAt(i) == k) at = i;
  bagOpen = true; bagPage = at / 5; render();
  onTap(233, 112 + (at % 5) * 58 + 25);
}
static void closeBag() { bagOpen = false; partyOpen = false; vitPick = 0; ppUpWho = 0xFF; render(); }
// A vitamin on `who` (-1 the live pet, else a party slot), through the ring.
static bool vitaminOn(ItemKey k, int who) {
  uint8_t before = bag.count(k);
  tapBagItem(k);
  if (party.count()) {
    if (who < 0) onTap(233, 233);
    else { int x, y; partySlotPos(who, &x, &y); onTap((int16_t)x, (int16_t)y); }
  }
  bool ok = bag.count(k) < before;
  closeBag();
  return ok;
}
static bool ppUpOnSlot(int who, int slot) {
  uint8_t before = bag.count(IT_PPUP);
  tapBagItem(IT_PPUP);
  if (party.count()) {
    if (who < 0) onTap(233, 233);
    else { int x, y; partySlotPos(who, &x, &y); onTap((int16_t)x, (int16_t)y); }
  }
  render(); onTap(233, 112 + slot * 58 + 25);
  bool ok = bag.count(IT_PPUP) < before;
  closeBag();
  return ok;
}
// Spending what is left over a reserve (balls, stones and cords come first, in
// session()): X items before a gym, then vitamins on the team, then PP Ups on
// the live pet's best move.
static const uint32_t RESERVE = 30000;
static void spendSurplus() {
  if (pet.isEgg()) return;
  spendBucket = &outX;
  if (pet.wallet > RESERVE + 8000) {
    if (bag.count(IT_XATTACK) < 3) buy(IT_XATTACK, 3 - bag.count(IT_XATTACK));
    if (bag.count(IT_XSPATK) < 3) buy(IT_XSPATK, 3 - bag.count(IT_XSPATK));
  }
  // vitamins: the team member with the most headroom on a stat, a few a session
  spendBucket = &outVit;
  for (int n = 0; n < 4 && pet.wallet > RESERVE + itemEntry(IT_PROTEIN).price; n++) {
    int bestWho = -2, bestGap = 0; ItemKey bestK = IT_PROTEIN;
    auto look = [&](int who, uint8_t a, uint8_t d, uint8_t s, uint8_t ia, uint8_t id, uint8_t is) {
      int g[3] = { Pet::trMaxFor(ia) - a, Pet::trMaxFor(id) - d, Pet::trMaxFor(is) - s };
      static const ItemKey K[3] = { IT_PROTEIN, IT_IRON, IT_CARBOS };
      for (int i = 0; i < 3; i++) if (g[i] > bestGap) { bestGap = g[i]; bestWho = who; bestK = K[i]; }
    };
    look(-1, pet.trAtk, pet.trDef, pet.trSpe, pet.ivAtk, pet.ivDef, pet.ivSpe);
    for (int i = 0; i < PARTY_SLOTS; i++) {
      const PartyMon &m = party.slots[i];
      if (!m.empty()) look(i, m.trAtk, m.trDef, m.trSpe, m.ivAtk, m.ivDef, m.ivSpe);
    }
    if (bestWho == -2) { nVitMax++; break; }   // the whole team is trained to its ceiling
    if (!bag.count(bestK) && !buy(bestK)) break;
    if (vitaminOn(bestK, bestWho)) nVit++; else { nUiMiss++; break; }
  }
  // PP Up: only with real surplus, on the live pet's strongest move
  spendBucket = &outPpUp;
  if (pet.wallet > RESERVE + 100000) {
    int slot = -1, pow = 0;
    for (int i = 0; i < MOVE_SLOTS; i++) {
      uint8_t mv = pet.moves[i];
      if (!mv || mv >= MOVE_COUNT || ppUpsAt(pet.ppUps, i) >= PP_UPS_MAX) continue;
      if (MOVE_TBL[mv].power > pow) { pow = MOVE_TBL[mv].power; slot = i; }
    }
    if (slot >= 0 && (bag.count(IT_PPUP) || buy(IT_PPUP))) {
      if (ppUpOnSlot(-1, slot)) nPpUp++; else nUiMiss++;
    }
  }
  spendBucket = &outBalls;
}

// ---- evolution ----------------------------------------------------------------------
static bool chainHasUnregistered(int16_t d, int depth = 0) {
  if (depth > 3) return false;
  if (wanted(d)) return true;
  // every route out of d
  if (DEX_TBL[d].evolvesTo && chainHasUnregistered(DEX_TBL[d].evolvesTo, depth + 1)) return true;
  for (int i = 0; i < ITEM_EVO_COUNT; i++) if (ITEM_EVOS[i].from == d && chainHasUnregistered(ITEM_EVOS[i].to, depth + 1)) return true;
  for (int i = 0; i < TRADE_EVO_COUNT; i++) if (TRADE_EVOS[i].from == d && chainHasUnregistered(TRADE_EVOS[i].to, depth + 1)) return true;
  for (int i = 0; i < BRANCH_EVO_COUNT; i++) if (BRANCH_EVOS[i].from == d && chainHasUnregistered(BRANCH_EVOS[i].to, depth + 1)) return true;
  for (int i = 0; i < GENDER_EVO_COUNT; i++) if (GENDER_EVOS[i].from == d && chainHasUnregistered(GENDER_EVOS[i].to, depth + 1)) return true;
  if (d == DEX_EEVEE) for (int i = 0; i < EEVEE_EVO_COUNT; i++) if (wanted(EEVEE_EVOS[i])) return true;
  return false;
}
static bool aheadUnregistered(int16_t d) {    // something past d, not d itself
  bool was = pet.isRegistered(d);
  if (!was) return chainHasUnregistered(d);
  return chainHasUnregistered(d);
}
// Does whatever the live pet's evolution needs right now. True if it evolved.
static bool tendEvolution(const char *why) {
  if (pet.isEgg() || pet.ceremony != CER_NONE) return false;
  int16_t before = pet.speciesId;
  Pet::EvoInfo e = pet.evoInfo();
  if (e.status == Pet::EVO_NEEDS_ITEM) {
    for (uint8_t s = 0; s < EVO_STONE_COUNT; s++) {
      int16_t t = pet.stoneTarget(s);
      if (!t || !chainHasUnregistered(t)) continue;
      ItemKey k = (ItemKey)(IT_FIRESTONE + s);
      if (!bag.count(k)) {
        spendBucket = &outStones;
        bool got = buy(k);
        spendBucket = &outBalls;
        if (!got) continue;
        nStonesBought++; ev("\"ev\":\"buy_stone\",\"stone\":%u", s);
      }
      stoneFromBag(k);
      break;
    }
  } else if (e.status == Pet::EVO_NEEDS_TRADE && e.target && chainHasUnregistered(e.target)) {
    if (pet.wallet >= itemEntry(IT_LINKCORD).price && pet.cordUse() == Pet::CORD_SOLO) {
      if (pet.spendWallet(itemEntry(IT_LINKCORD).price) && pet.applyCord()) {
        moneyOut += itemEntry(IT_LINKCORD).price; outCords += itemEntry(IT_LINKCORD).price; nCords++; ev("\"ev\":\"cord\",\"dex\":%d", pet.speciesId);
      }
    }
  }
  if (pet.wantEvolveButton() || (pet.canEvolveNow() && e.status == Pet::EVO_READY)) {
    int16_t ch[4];
    uint8_t n = pet.evoChoices(ch);
    if (n > 1) {
      int16_t pick = ch[0];
      for (uint8_t i = 0; i < n; i++) if (wanted(ch[i])) pick = ch[i];
      pet.evolveChoice(pick);
    } else pet.evolve();
    shedinjaCheck();
    if (ownedCount(292)) noteAcquired(292, "shedinja");
  }
  if (pet.speciesId != before) {
    nEvolve++;
    ev("\"ev\":\"evolve\",\"from\":%d,\"to\":%d,\"lvl\":%u,\"why\":\"%s\"", before, pet.speciesId, pet.level(), why);
    noteRegistered("evolve");
    return true;
  }
  return false;
}
// A banked creature whose next form is not registered, evolved by swapping it in.
static void swapInEvolutions(int budget) {
  if (pet.isEgg() || pet.ceremony != CER_NONE) return;
  for (int i = 0; i < PARTY_SLOTS && budget > 0; i++) {
    const PartyMon m = party.slots[i];   // a copy: the swap rewrites the slot
    if (m.empty()) continue;
    // only creatures with something unregistered AFTER them
    bool after = false;
    if (DEX_TBL[m.dex].evolvesTo && chainHasUnregistered(DEX_TBL[m.dex].evolvesTo)) after = true;
    for (int k = 0; k < ITEM_EVO_COUNT && !after; k++) if (ITEM_EVOS[k].from == m.dex && chainHasUnregistered(ITEM_EVOS[k].to)) after = true;
    for (int k = 0; k < TRADE_EVO_COUNT && !after; k++) if (TRADE_EVOS[k].from == m.dex && chainHasUnregistered(TRADE_EVOS[k].to)) after = true;
    for (int k = 0; k < BRANCH_EVO_COUNT && !after; k++) if (BRANCH_EVOS[k].from == m.dex && chainHasUnregistered(BRANCH_EVOS[k].to)) after = true;
    if (!after) continue;
    int16_t was = pet.speciesId;
    focusSwap((uint8_t)i);
    if (pet.speciesId != m.dex) continue;   // refused
    nSwaps++;
    pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;   // the player cares for it first
    tendEvolution("swapped in");
    budget--;
    // put the old pet back
    for (int j = 0; j < PARTY_SLOTS; j++)
      if (!party.slots[j].empty() && party.slots[j].dex == was) { focusSwap((uint8_t)j); break; }
  }
}

// Something still wanted past d by a level: the one route a creature reaches by
// being raised as the live pet, with nothing to buy or trade.
static bool levelAheadWanted(int16_t d) {
  int16_t t = DEX_TBL[d].evolvesTo;
  if (!t || DEX_TBL[d].evolveLevel == 0) return false;
  for (int k = 0; k < ITEM_EVO_COUNT; k++) if (ITEM_EVOS[k].from == d) return false;
  for (int k = 0; k < TRADE_EVO_COUNT; k++) if (TRADE_EVOS[k].from == d) return false;
  return chainHasUnregistered(t);
}
// When the live pet has nothing left to give the collection, a collector raises
// a stored creature that only needs levels instead: the one closest to its
// evolution, from the party or the PC, kept as the live pet (it ages, evolves and
// says its farewell like any other).
static void raiseStored() {
  if (pet.isEgg() || pet.ceremony != CER_NONE || pet.wantFarewellButton()) return;
  if (levelAheadWanted(pet.speciesId)) return;
  int bestGap = 999, bestSlot = -1; int16_t bestDex = 0; uint8_t bestPc = 0;
  for (int i = 0; i < PARTY_SLOTS; i++) {
    const PartyMon &m = party.slots[i];
    if (m.empty() || !levelAheadWanted(m.dex)) continue;
    int gap = DEX_TBL[m.dex].evolveLevel - m.level;
    if (gap < bestGap) { bestGap = gap; bestSlot = i; bestDex = 0; }
  }
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (!pcCount(d) || !levelAheadWanted(d)) continue;
    for (uint8_t k = 0; k < pcCount(d); k++) {
      PartyMon m;
      if (!pcGet(d, k, m) || m.empty()) continue;
      int gap = DEX_TBL[d].evolveLevel - m.level;
      if (gap < bestGap) { bestGap = gap; bestSlot = -1; bestDex = d; bestPc = k; }
    }
  }
  if (bestGap == 999) return;
  if (bestDex) {                                 // out of the PC, into the party's weakest slot
    int weak = 0, weakLvl = 999;
    for (int i = 0; i < PARTY_SLOTS; i++) {
      int l = party.slots[i].empty() ? -1 : party.slots[i].level;
      if (l < weakLvl) { weakLvl = l; weak = i; }
    }
    if (!boxViewSwap((uint8_t)weak, boxViewIndexOf(bestDex, bestPc))) return;
    bestSlot = weak;
  }
  int16_t want = party.slots[bestSlot].dex;
  focusSwap((uint8_t)bestSlot);
  if (pet.speciesId == want) { nRaised++; ev("\"ev\":\"raise\",\"dex\":%d,\"lvl\":%u", want, pet.level()); }
}

// ---- learning moves -------------------------------------------------------------------
static void tendLearning() {
  for (int g = 0; g < 8 && pet.hasLearnOffer(); g++) {
    uint8_t mv = pet.learnOffer();
    int weak = 0, weakPow = 999;
    for (int i = 0; i < MOVE_SLOTS; i++) {
      int p = pet.moves[i] ? MOVE_TBL[pet.moves[i]].power : -1;
      if (p < weakPow) { weakPow = p; weak = i; }
    }
    if ((int)MOVE_TBL[mv].power > weakPow) pet.acceptLearn((uint8_t)weak); else pet.declineLearn();
  }
}

// ---- where to explore ---------------------------------------------------------------------
static int unregisteredIn(uint16_t a, uint8_t when) {
  const WildArea &A = WILD_AREAS[a];
  std::set<int16_t> s;
  for (uint16_t i = 0; i < A.count; i++) {
    const WildSlot &w = WILD_SLOTS[A.first + i];
    if (w.dex > 0 && (w.when & when) && wanted(w.dex) && wildGateOpen(A.region, w.gate, pet))
      s.insert(w.dex);
  }
  for (uint8_t i = 0; i < wildLegendCount(); i++)
    if (wildLegendHome(i) == a && wildLegendLive(i, pet) && wanted(wildLegendDex(i))) s.insert(wildLegendDex(i));
  return (int)s.size();
}
// What a player weighs is the chance the NEXT encounter is new, not how many new
// species an area lists: a zone listing forty at a fraction of a percent each
// is worse than a forest whose every second encounter is new. Each area's
// encounter mix is sampled once per gate state (and per day where it has a
// weather), through the firmware's own pick, and kept.
struct Mix { uint64_t key = ~0ull; std::vector<std::pair<int16_t, uint16_t>> n; };
static std::vector<Mix> mixCache;
static const Mix &areaMix(uint16_t a, uint8_t when) {
  if (mixCache.empty()) mixCache.resize((size_t)WILD_AREA_COUNT * 8);
  uint64_t key = WILD_AREAS[a].wxMask ? (uint64_t)day() : 0;   // the ladders are sequential: a count each
  for (uint8_t r = 0; r < GYM_REGIONS; r++)
    key = key * 14 + (uint64_t)__builtin_popcount(pet.badgeMask(r, false) | pet.badgeMask(r, true));
  Mix &m = mixCache[(size_t)a * 8 + when];
  if (m.key == key) return m;
  m.key = key; m.n.clear();
  std::map<int16_t, uint16_t> c;
  for (uint32_t i = 0; i < 256; i++) {
    int16_t d = wildAreaPick(a, when, pet, i * 39 + 7);
    if (d > 0) c[d]++;
  }
  for (auto &kv : c) m.n.push_back(kv);
  return m;
}
static int wantedShare(uint16_t a, uint8_t when) {     // per 256 encounters
  int n = 0;
  for (auto &kv : areaMix(a, when).n) if (wanted(kv.first)) n += kv.second;
  return n;
}
static uint16_t pickArea(uint8_t *region) {
  int hr = hourOf();
  uint8_t when = wildTimeBand(hr);
  int best = -1; uint16_t bestA = WILD_NO_AREA;
  for (uint8_t r = 0; r < GYM_REGIONS; r++)
    for (uint8_t k = 0; k < wildAreaCount(r); k++) {
      uint16_t a = wildAreaFirst(r) + k;
      if (!wildAreaUnlocked(a, pet) || wildAreaState(a, when, pet) != WILD_AREA_LIVE) continue;
      // a legendary waiting at home is a certain new species
      int score = unregisteredIn(a, when) && (wildAreaLegendDue(a, pet) || wildAreaFixedDue(a, pet)) ? 100000 : 0;
      // a met but uncaught legendary or one-off turns up WILD_LEGEND_PCT of the time,
      // in place of the ordinary pick: count it at that chance, per 256 encounters
      int share = wantedShare(a, when), extra = 0;
      for (uint8_t i = 0; i < wildLegendCount(); i++)
        if (wanted(wildLegendDex(i)) && wildLegendLive(i, pet) &&
            (wildLegendHome(i) == a || (wildLegendRoams(i) && WILD_AREAS[wildLegendHome(i)].region == r)))
          extra += 256 * WILD_LEGEND_PCT / 100;
      for (uint8_t i = 0; i < wildFixedCount(); i++)
        if (wildFixedArea(i) == a && wanted(wildFixedDex(i)) && wildFixedLive(i, pet))
          extra += 256 * WILD_LEGEND_PCT / 100;
      score += (extra + share * (100 - (extra ? WILD_LEGEND_PCT : 0)) / 100) * 100;
      // a light pull towards areas at the team's level, for training and stones
      score -= abs((int)wildAreaLvMax(a) - teamLevel(3));
      if (score > best) { best = score; bestA = a; *region = r; }
    }
  return bestA;
}

// ---- the gyms ------------------------------------------------------------------------
static int retryDay[GYM_REGIONS] = {}, lossRun[GYM_REGIONS] = {};
static bool gymNext(bool hard, uint8_t *region, uint8_t *idx) {
  int bestGap = 999;
  bool found = false;
  for (uint8_t r = 0; r < GYM_REGIONS; r++) {
    if (day() < retryDay[r]) continue;          // backing off after a loss there
    uint16_t mask = pet.badgeMask(r, hard);
    for (uint8_t i = 0; i < TRAINER_COUNT; i++) {
      if (mask & (1u << i)) continue;
      const Trainer &t = TRAINER_SETS[r].list[i];
      int need = 0; for (uint8_t m = 0; m < t.count; m++) if (t.team[m].level > need) need = t.team[m].level;
      int gap = need - teamLevel(std::min<int>(t.count, 3));
      if (gap < bestGap) { bestGap = gap; *region = r; *idx = i; found = true; }
      break;                                   // the ladder is sequential
    }
  }
  return found && bestGap <= 0;
}

// ---- one session --------------------------------------------------------------------------
static void careFor() {
  if (pet.isEgg()) { pet.eggTap(); pet.eggTap(); pet.eggTap(); run(60000); noteRegistered("hatch"); return; }
  if (pet.sleeping) pet.toggleLight();
  for (int i = 0; i < 4 && pet.fullness < 85; i++) { pet.feed(); run(20000, 20000); }
  if (pet.hygiene < 80 || pet.poops) pet.clean();
  pet.playJoyGame(12);                        // a decent round of a joy game
  for (int i = 0; i < 3; i++) pet.caress();
  // one training game on the weakest trainable stat
  uint8_t mask = pet.trAtk <= pet.trDef && pet.trAtk <= pet.trSpe ? 1 : pet.trDef <= pet.trSpe ? 2 : 4;
  pet.trainSplit(mask, 14);
  if (pet.fullness < 85) pet.feed();
  tendLearning();
}
static void session(int s) {
  screenOff = false; pet.setScreenOff(false);
  careFor();
  const DayPlan &D = plan();
  {                                             // the walk since the last session
    uint32_t w0 = pet.wallet;
    pet.addSteps(D.steps / D.sessions);
    moneyIn += pet.wallet - w0; inSteps += pet.wallet - w0;   // what landed (the cap)
  }
  sellSpares();
  stockBalls();
  tendEvolution("offered");
  if (P.swapIn) swapInEvolutions(2);
  raiseStored();                                // every profile: it is just choosing who to raise
  spendSurplus();
  int fights = D.fights / D.sessions;
  int gymFights = 0;                            // at most half the session's fights
  for (int f = 0; f < fights; f++) {
    if (pet.isEgg() || pet.ceremony != CER_NONE) break;
    arrangeTeam();
    uint8_t gr, gi;
    // easy first, then hard once every easy ladder is done
    bool allEasy = true;
    for (uint8_t r = 0; r < GYM_REGIONS; r++) if (pet.badgeMask(r, false) != (uint16_t)((1u << TRAINER_COUNT) - 1)) allEasy = false;
    bool hardNow = allEasy;
    if (gymFights < (fights + 1) / 2 && gymNext(hardNow, &gr, &gi)) {
      gymFights++;
      gymRegion = gr; gymHard = hardNow;
      uint16_t before = pet.badgeMask(gr, hardNow);
      uint32_t w0 = pet.wallet;
      startTrainerBattle(gi, hardNow);
      if (!battleOpen) { ev("\"ev\":\"gym_refused\",\"r\":%u,\"i\":%u", gr, gi); continue; }
      nFights++; nGym++;
      int res = playFight(false);
      bool won = pet.badgeMask(gr, hardNow) != before;
      if (pet.wallet > w0) { moneyIn += pet.wallet - w0; inPrize += pet.wallet - w0; }
      ev("\"ev\":\"gym\",\"r\":%u,\"i\":%u,\"hard\":%d,\"won\":%d,\"team\":%d,\"res\":%d",
         gr, gi, hardNow ? 1 : 0, won ? 1 : 0, teamLevel(3), res);
      // a loss: back off, longer each time in a row (1, 2, 4 ... 32 days), as a
      // player stops throwing the same team at a leader it keeps losing to
      if (won) { nGymWin++; lossRun[gr] = 0; }
      else { retryDay[gr] = day() + (1 << std::min<int>(lossRun[gr], 5)); lossRun[gr]++; }
      continue;
    }
    uint8_t r = 0;
    uint16_t a = pickArea(&r);
    if (a == WILD_NO_AREA) break;
    exploreRegion = r;
    exploreAreaPick[r] = (uint16_t)(a + 1);
    exploreOpen = true;
    uint8_t stones0[EVO_STONE_COUNT];
    for (uint8_t i = 0; i < EVO_STONE_COUNT; i++) stones0[i] = bag.count((ItemKey)(IT_FIRESTONE + i));
    if (!startWildBattle(r, false)) { exploreOpen = false; continue; }
    nFights++; nWild++;
    int16_t dex = wildDex;
    bool want = wanted(dex);
    bool safari = wildAreaMode(a) == WILD_MODE_SAFARI;
    int8_t legend = wildLegend, fixedOne = wildFixed;
    int res = playFight(want);
    exploreOpen = false;
    if (want) {
      if (wildCaught) {
        nCatch++;
        noteAcquired(dex, safari ? "safari" : legend >= 0 ? "legendary" : fixedOne >= 0 ? "one-off" : "catch");
      } else {
        nCatchFail++;
        ev("\"ev\":\"catch_fail\",\"dex\":%d,\"lvl\":%u,\"won\":%d,\"foe_hp\":%u,\"you_lvl\":%d,\"balls\":%u,\"safari\":%d,\"legend\":%d",
           dex, wildLvl, btlWon ? 1 : 0, btlFoe.hp, teamLevel(1), bag.count(IT_POKEBALL) + bag.count(IT_GREATBALL) + bag.count(IT_ULTRABALL), safari, legend);
      }
    }
    for (uint8_t i = 0; i < EVO_STONE_COUNT; i++)
      if (bag.count((ItemKey)(IT_FIRESTONE + i)) > stones0[i]) {
        nStonesFound++;
        ev("\"ev\":\"stone_found\",\"stone\":%u,\"area\":\"%s\"", i, wildAreaName(a));
      }
    (void)res;
  }
  tendEvolution("offered");
  // the farewell, when offered: the next egg from the region with the most left
  if (pet.wantFarewellButton()) {
    int bestR = pet.region, bestN = -1;
    for (uint8_t r = 0; r < GYM_REGIONS; r++) {
      int n = 0;
      for (int16_t d = REGIONS[r].lo; d <= REGIONS[r].hi; d++)
        if (wanted(d) && DEX_TBL[d].rarity != R_EVO) n++;
      if (n > bestN) { bestN = n; bestR = r; }
    }
    pet.setRegion((uint8_t)bestR);
    int16_t was = pet.speciesId;
    pet.startFarewell();
    run(120000, 10000);                           // the ceremony, then the egg
    if (partyPick) { pet.clearEnded(); partyPick = false; partyOpen = false; }
    nFarewell++;
    ev("\"ev\":\"farewell\",\"dex\":%d,\"egg_region\":%d", was, bestR);
  }
}

int main(int argc, char **argv) {
  const char *prof = argc > 1 ? argv[1] : "walker";
  uint32_t seedArg = argc > 2 ? (uint32_t)atoi(argv[2]) : 1;
  g_seed = seedArg;
  int days = argc > 3 ? atoi(argv[3]) : 365;
  for (const Profile &p : PROFILES) if (!strcmp(p.name, prof)) P = p;
  if (!P.name) { fprintf(stderr, "unknown profile\n"); return 1; }
  rtcSetEpoch(START_EPOCH + 7 * 3600);
  // Nobody looks at the panel; PLAYSIM_DRAW=1 draws anyway, to show it changes nothing
  Arduino_Canvas::skipDraw = !getenv("PLAYSIM_DRAW");
  emuVirtualClock(true);                        // and the clock is ours alone: same seed, same run
  setup();
  gRegionArt = 0xFFFF;
  seen.assign(DEX_COUNT + 1, false);
  acquired.assign(DEX_COUNT + 1, false);
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (!speciesShowable(d)) continue;
    bool eggOnlyLegend = DEX_TBL[d].rarity == R_LEGENDARIO && !wildIsExploreLegend(d);
    if (d == 589 || d == 617 || d == 733 || eggOnlyLegend) continue;
    target.insert(d);
  }
  ev("\"ev\":\"start\",\"profile\":\"%s\",\"seed\":%u,\"target\":%u", P.name, seedArg, (unsigned)target.size());
  {                                             // the goal, by number, so a report can name what never came
    std::string l;
    for (int16_t d : target) { char b[8]; snprintf(b, sizeof(b), "%s%d", l.empty() ? "" : ",", d); l += b; }
    ev("\"ev\":\"target\",\"dex\":[%s]", l.c_str());
  }
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  run(5 * 60000);
  noteRegistered("start");
  int lastDay = -1;
  while (day() < days) {
    int h = hourOf();
    for (uint8_t s = 0; s < plan().sessions; s++)
      if (h == sessionHour(s)) session(s);
    if (h == 22) {                               // bedtime: lights out, screen off
      if (!pet.isEgg() && !pet.sleeping) pet.toggleLight();
      screenOff = true; pet.setScreenOff(true);
    }
    // the next hour
    int64_t toNext = 3600 - (int64_t)(nowEpoch() % 3600);
    run((uint64_t)toNext * 1000, 600000);
    if (day() != lastDay) {
      lastDay = day();
      if (lastDay % 7 == 0) {
        int left = targetLeft();
        int badgesE = 0, badgesH = 0;
        for (uint8_t r = 0; r < GYM_REGIONS; r++) {
          badgesE += __builtin_popcount(pet.badgeMask(r, false));
          badgesH += __builtin_popcount(pet.badgeMask(r, true));
        }
        ev("\"ev\":\"week\",\"reg\":%u,\"left\":%d,\"badges_easy\":%d,\"badges_hard\":%d,\"fights\":%d,\"wild\":%d,"
           "\"catch\":%d,\"miss\":%d,\"gym\":%d,\"gymwin\":%d,\"blackouts\":%d,\"farewells\":%d,\"evolves\":%d,"
           "\"stones_found\":%d,\"stones_bought\":%d,\"cords\":%d,\"swaps\":%d,\"wallet\":%u,\"in\":%llu,\"out\":%llu,"
           "\"pet\":%d,\"lvl\":%u,\"raised\":%d,"
           "\"in_steps\":%llu,\"in_prize\":%llu,\"in_sold\":%llu,\"out_balls\":%llu,\"out_stones\":%llu,"
           "\"out_cords\":%llu,\"out_x\":%llu,\"out_vit\":%llu,\"out_ppup\":%llu,\"out_heal\":%llu,"
           "\"sold\":%d,\"vit\":%d,\"vit_maxed\":%d,\"x_used\":%d,\"ppup\":%d,\"heal_used\":%d,\"ui_miss\":%d",
           registeredNow(), left, badgesE, badgesH, nFights, nWild, nCatch, nCatchFail, nGym, nGymWin, nBlack,
           nFarewell, nEvolve, nStonesFound, nStonesBought, nCords, nSwaps, pet.wallet,
           (unsigned long long)moneyIn, (unsigned long long)moneyOut, pet.speciesId, pet.level(), nRaised,
           (unsigned long long)inSteps, (unsigned long long)inPrize, (unsigned long long)inSold,
           (unsigned long long)outBalls, (unsigned long long)outStones, (unsigned long long)outCords,
           (unsigned long long)outX, (unsigned long long)outVit, (unsigned long long)outPpUp, (unsigned long long)outHeal,
           nSold, nVit, nVitMax, nXUsed, nPpUp, nHealUsed, nUiMiss);
        fflush(stdout);
        if (left == 0) { ev("\"ev\":\"done\""); break; }
      }
    }
  }
  ev("\"ev\":\"end\",\"reg\":%u", registeredNow());
  return 0;
}
