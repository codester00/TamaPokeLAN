// The Game Corners' rules -- see gamecorner.h.
#include "gamecorner.h"
#include <Arduino.h>
#include "settings.h"
#include <string.h>
#include "moves.h"
#include "wild.h"

// ---------------------------------------------------------------- the reels
// Celadon: pokered's SlotMachineWheel1-3 (data/events/slot_machine_wheels.asm).
// The tables are 18 long but the wheel is 15: the last three repeat the first
// three so a window of three can wrap (the offset wraps at 30 bytes, 15 stops).
// Symbols: 7, BAR, CHERRY, FISH, BIRD, MOUSE (the art's order).
enum : uint8_t { RS_7, RS_BAR, RS_CHERRY, RS_FISH, RS_BIRD, RS_MOUSE, RS_COUNT };
static const uint8_t RED_W1[15] = { RS_7, RS_MOUSE, RS_FISH, RS_BAR, RS_CHERRY, RS_7, RS_FISH, RS_BIRD, RS_BAR,
                                    RS_CHERRY, RS_7, RS_MOUSE, RS_BIRD, RS_BAR, RS_CHERRY };
static const uint8_t RED_W2[15] = { RS_7, RS_FISH, RS_CHERRY, RS_BIRD, RS_MOUSE, RS_BAR, RS_CHERRY, RS_FISH, RS_BIRD,
                                    RS_CHERRY, RS_BAR, RS_FISH, RS_BIRD, RS_CHERRY, RS_MOUSE };
static const uint8_t RED_W3[15] = { RS_7, RS_BIRD, RS_FISH, RS_CHERRY, RS_MOUSE, RS_BIRD, RS_FISH, RS_CHERRY, RS_MOUSE,
                                    RS_BIRD, RS_FISH, RS_CHERRY, RS_MOUSE, RS_BIRD, RS_BAR };
// Red/Blue: 7s 300, BARs 100, three cherries 8, the creatures 15.
static const uint16_t RED_PAY[RS_COUNT] = { 300, 100, 8, 15, 15, 15 };

// Goldenrod: pokecrystal's Reel1Tilemap-Reel3Tilemap (engine/games/
// slot_machine.asm), REEL_SIZE 15. Symbols in SLOTS_* order: 7, POKE BALL,
// CHERRY, PIKACHU, SQUIRTLE, STARYU.
enum : uint8_t { CS_7, CS_BALL, CS_CHERRY, CS_PIKA, CS_SQUIRTLE, CS_STARYU, CS_COUNT };
static const uint8_t GS_W1[15] = { CS_7, CS_CHERRY, CS_STARYU, CS_PIKA, CS_SQUIRTLE, CS_7, CS_CHERRY, CS_STARYU,
                                   CS_PIKA, CS_SQUIRTLE, CS_BALL, CS_CHERRY, CS_STARYU, CS_PIKA, CS_SQUIRTLE };
static const uint8_t GS_W2[15] = { CS_7, CS_PIKA, CS_CHERRY, CS_SQUIRTLE, CS_STARYU, CS_BALL, CS_PIKA, CS_CHERRY,
                                   CS_SQUIRTLE, CS_STARYU, CS_BALL, CS_PIKA, CS_CHERRY, CS_SQUIRTLE, CS_STARYU };
static const uint8_t GS_W3[15] = { CS_7, CS_PIKA, CS_CHERRY, CS_SQUIRTLE, CS_STARYU, CS_PIKA, CS_CHERRY, CS_SQUIRTLE,
                                   CS_STARYU, CS_PIKA, CS_BALL, CS_CHERRY, CS_SQUIRTLE, CS_STARYU, CS_PIKA };
// Crystal's .PayoutTable.
static const uint16_t GS_PAY[CS_COUNT] = { 300, 50, 6, 8, 10, 15 };

// The lines, as rows from the middle per reel (-1 is the stop before the middle
// one), in the bit order of SlotResult::lines.
static const int8_t LINE[5][3] = {
  { 0, 0, 0 },      // middle
  { -1, -1, -1 },   // top
  { 1, 1, 1 },      // bottom
  { -1, 0, 1 },     // diagonal
  { 1, 0, -1 },     // the other diagonal
};
// Which line pays when two do: Red takes the FIRST match of diagonals, then the
// outer rows, then the middle (CheckForMatches); Crystal checks the same lines
// the other way round and keeps the LAST (Slots_CheckMatchedAllThreeReels).
static const uint8_t RED_ORDER[5] = { 3, 4, 2, 1, 0 };
static const uint8_t GS_ORDER[5]  = { 0, 2, 1, 4, 3 };

static const SlotMachine MACHINES[HALL_CORNERS] = {
  { { RED_W1, RED_W2, RED_W3 }, 15, RS_COUNT, RED_PAY, RED_ORDER },
  { { GS_W1, GS_W2, GS_W3 }, 15, CS_COUNT, GS_PAY, GS_ORDER },
};

const SlotMachine &slotMachine(uint8_t hall) {
  return MACHINES[hall < HALL_CORNERS ? hall : 0];
}

uint8_t slotSymbol(const SlotMachine &m, uint8_t reel, uint8_t stop, int8_t row) {
  int p = ((int)stop + row) % m.len;
  if (p < 0) p += m.len;
  return m.reel[reel % SLOT_REELS][p];
}

static uint8_t clampBet(uint8_t bet) {
  return bet < 1 ? 1 : bet > SLOT_MAX_BET ? SLOT_MAX_BET : bet;
}
static bool linePlayed(uint8_t line, uint8_t bet) {
  return line == 0 || (line <= 2 && bet >= 2) || bet >= 3;
}

// The symbol of the line that pays, or 0xFF.
static uint8_t paidSymbol(const SlotMachine &m, const uint8_t stop[SLOT_REELS], uint8_t bet, uint8_t *line) {
  bet = clampBet(bet);
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t l = m.order[i];
    if (!linePlayed(l, bet)) continue;
    uint8_t a = slotSymbol(m, 0, stop[0], LINE[l][0]);
    if (a != slotSymbol(m, 1, stop[1], LINE[l][1]) || a != slotSymbol(m, 2, stop[2], LINE[l][2])) continue;
    if (line) *line = l;
    return a;
  }
  return 0xFF;
}

uint16_t slotPayout(const SlotMachine &m, const uint8_t stop[SLOT_REELS], uint8_t bet,
                    uint8_t *lines) {
  uint8_t l = 0;
  uint8_t sym = paidSymbol(m, stop, bet, &l);
  if (lines) *lines = sym == 0xFF ? 0 : (uint8_t)(1 << l);
  return sym == 0xFF ? 0 : m.pay[sym];
}

// ---- the lucky machines
// Red: walking into the Game Corner picks one machine (GameCornerSelectLuckySlot
// Machine: a random byte, 8 if under 7, over 8 -> 1..31). Its hidden events
// list 36 machines, three of them out of order, and only the first 31 can be
// the lucky one.
#define RED_MACHINES 36
static bool redBroken(uint8_t i) { return i == 5 || i == 8 || i == 27; }
static uint8_t redLuckyIdx = 0;
static uint8_t redFlags = 0;   // wSlotMachineFlags: kept between sittings, as in Red

void slotVisit(uint8_t hall) {
  if (hall != HALL_CELADON) return;
  uint8_t a = (uint8_t)random(256);
  if (a < 7) a = 8;
  redLuckyIdx = (uint8_t)(a >> 3);
}

SlotSession slotSit(uint8_t hall) {
  SlotSession s;
  s.hall = hall < HALL_CORNERS ? hall : HALL_CELADON;
  if (s.hall == HALL_CELADON) {
    if (!redLuckyIdx) slotVisit(HALL_CELADON);
    uint8_t m;
    do { m = (uint8_t)random(RED_MACHINES); } while (redBroken(m));
    s.lucky = (uint8_t)(m + 1) == redLuckyIdx;
    s.flags = redFlags;
  } else {
    // Goldenrod's 18 machines: the eighth is always lucky, any other 1 in 6
    // (GoldenrodGameCornerSlotsMachineScript: random 6)
    uint8_t m = (uint8_t)random(18);
    s.lucky = m == 7 || random(6) == 0;
    s.keepRare = (random(256) & 0x2A) == 0;   // Slots init: 12.5%
  }
  return s;
}

// ---- Red: SlotMachine_SetFlags, the wheel stops and CheckForMatches
static void redSetFlags(SlotSession &s) {
  if (s.flags & SLOT_RED_CAN_7) return;
  if (s.allow) { s.flags |= SLOT_RED_CAN_WIN; return; }
  uint8_t b = (uint8_t)random(256);
  if (b == 0) { s.allow = 60; return; }                 // 1/256: sixty small wins
  if (b > (s.lucky ? 250 : 253)) { s.flags |= SLOT_RED_CAN_7; return; }
  if (b > 210) { s.flags |= SLOT_RED_CAN_WIN; return; }  // ~1 in 6
  s.flags = 0;
}

static SlotResult redSpin(SlotSession &s, uint8_t bet) {
  const SlotMachine &m = MACHINES[HALL_CELADON];
  redSetFlags(s);
  SlotResult r;
  for (int i = 0; i < SLOT_REELS; i++) r.stop[i] = (uint8_t)random(m.len);
  auto adv = [&](int i) { r.stop[i] = (uint8_t)((r.stop[i] + 1) % m.len); };
  bool seven = s.flags & SLOT_RED_CAN_7;
  // wheel 1 slips up to four stops: past a centred cherry normally; in 7/BAR
  // mode always all four (Red's bug: it meant to stop on a 7)
  for (int c = 0; c < 4; c++) {
    if (!seven && slotSymbol(m, 0, r.stop[0], 0) != RS_CHERRY) break;
    adv(0);
  }
  // wheel 2 slips until it pairs with wheel 1 (in 7/BAR mode: until the paired,
  // or else the lower, symbol is a 7 or BAR)
  static const int8_t PAIR[5][2] = { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 1, 1 } };
  for (int c = 0; c < 4; c++) {
    int8_t w2 = -1;
    bool paired = false;
    for (auto &p : PAIR)
      if (slotSymbol(m, 0, r.stop[0], p[0]) == slotSymbol(m, 1, r.stop[1], p[1])) { paired = true; w2 = p[1]; break; }
    uint8_t t = slotSymbol(m, 1, r.stop[1], w2);
    if (seven ? t <= RS_BAR : paired) break;
    adv(1);
  }
  // wheel 3 stops at once, then rolls on while a match is not allowed -- or,
  // when a win is allowed, until one lines up or the re-roll budget runs out
  uint8_t sym = 0xFF;
  for (int guard = 0; guard < 2000; guard++) {
    sym = paidSymbol(m, r.stop, bet, nullptr);
    if (sym != 0xFF) {
      if (!(s.flags & (SLOT_RED_CAN_WIN | SLOT_RED_CAN_7))) { adv(2); continue; }
      if (!(s.flags & SLOT_RED_CAN_7) && sym <= RS_BAR) { adv(2); continue; }
      break;
    }
    if (!(s.flags & (SLOT_RED_CAN_WIN | SLOT_RED_CAN_7))) break;
    if (--s.reroll == 0) break;
    adv(2);
  }
  r.win = slotPayout(m, r.stop, bet, &r.lines);
  if (r.win) {
    if (sym == RS_BAR) s.flags = 0;                       // SlotReward100Func
    else if (sym == RS_7) {                               // SlotReward300Func
      if (random(256) >= 0x80) s.flags = 0;
      s.allow = 0;
    } else if (s.allow) s.allow--;                        // the small prizes
  }
  redFlags = s.flags;
  return r;
}

// ---- Crystal: Slots_InitBias, the ReelAction stops and the reel-3 modes
static void gsInitBias(SlotSession &s) {
  if (s.bias == CS_7) return;                 // a 7 bias stays until 7s pay
  static const uint8_t NORMAL[7][2] = { { 1, CS_7 }, { 3, CS_BALL }, { 10, CS_STARYU }, { 20, CS_SQUIRTLE },
                                        { 40, CS_PIKA }, { 48, CS_CHERRY }, { 255, SLOT_NO_BIAS } };
  static const uint8_t LUCKY[7][2]  = { { 2, CS_7 }, { 3, CS_BALL }, { 8, CS_STARYU }, { 16, CS_SQUIRTLE },
                                        { 30, CS_PIKA }, { 80, CS_CHERRY }, { 255, SLOT_NO_BIAS } };
  const uint8_t (*t)[2] = s.lucky ? LUCKY : NORMAL;
  uint8_t c = (uint8_t)random(256);
  for (int i = 0; i < 7; i++) if (t[i][0] >= c) { s.bias = t[i][1]; return; }
}

// Slots_CheckMatchedFirstTwoReels: the pair the bet's lines show (the last
// checked wins, as the game overwrites it), and whether any of them is 7s.
static uint8_t gsPair(const SlotMachine &m, const uint8_t stop[SLOT_REELS], uint8_t bet, bool *sevens) {
  uint8_t got = 0xFF;
  if (sevens) *sevens = false;
  for (int i = 4; i >= 0; i--) {             // checked in reverse of GS_ORDER: last wins
    uint8_t l = GS_ORDER[i];
    if (!linePlayed(l, bet)) continue;
    uint8_t a = slotSymbol(m, 0, stop[0], LINE[l][0]);
    if (a != slotSymbol(m, 1, stop[1], LINE[l][1])) continue;
    got = a;
    if (a == CS_7 && sevens) *sevens = true;
  }
  return got;
}

static SlotResult gsSpin(SlotSession &s, uint8_t bet) {
  const SlotMachine &m = MACHINES[HALL_GOLDENROD];
  gsInitBias(s);
  SlotResult r;
  for (int i = 0; i < SLOT_REELS; i++) r.stop[i] = (uint8_t)random(m.len);
  auto adv = [&](int i) { r.stop[i] = (uint8_t)((r.stop[i] + 1) % m.len); };
  auto shows = [&](int reel, uint8_t sym) {
    for (int8_t row = -1; row <= 1; row++) if (slotSymbol(m, (uint8_t)reel, r.stop[reel], row) == sym) return true;
    return false;
  };
  // reel 1: with a bias, slip up to four until the symbol shows anywhere on it
  if (s.bias != SLOT_NO_BIAS)
    for (int c = 0; c < 4 && !shows(0, s.bias); c++) adv(0);
  // reel 2: at 2+ coins, a 7 on reel 1 and a 7 bias or none, 80/256 spin on
  // until 7s pair -- otherwise slip up to four toward the bias
  bool sevens = false;
  if (bet >= 2 && (s.bias == CS_7 || s.bias == SLOT_NO_BIAS) && shows(0, CS_7) && random(256) < 80) {
    for (int guard = 0; guard < 2 * m.len; guard++) {
      gsPair(m, r.stop, bet, &sevens);
      if (sevens) break;
      adv(1);
    }
  } else {
    for (int c = 4;;) {
      uint8_t p = gsPair(m, r.stop, bet, nullptr);
      if (p != 0xFF && p == s.bias) break;
      if (s.bias == SLOT_NO_BIAS || c == 0) break;
      c--;
      adv(1);
    }
  }
  bool paired = gsPair(m, r.stop, bet, &sevens) != 0xFF;
  // reel 3: with 7s paired, Slots_StopReel3 may hand over to the slow reel,
  // Golem or Chansey -- which land 7s with a 7 bias and nothing otherwise
  bool theatre = false;
  if (paired && sevens) theatre = (uint8_t)random(256) < (s.bias == CS_7 ? 180 : 160);
  if (theatre) {
    for (int guard = 0; guard < 4 * m.len; guard++) {
      uint8_t got = paidSymbol(m, r.stop, bet, nullptr);
      if (s.bias == CS_7 ? got == CS_7 : got == 0xFF) break;
      adv(2);
    }
  } else {
    // ReelAction_StopReel3: stop on the bias lined up; slip away from any
    // other line-up; with a bias, slip up to four looking for it
    for (int c = 4, guard = 0; guard < 4 * m.len; guard++) {
      uint8_t got = paidSymbol(m, r.stop, bet, nullptr);
      if (got != 0xFF) {
        if (got == s.bias) break;
        if (c) c--;
        adv(2);
        continue;
      }
      if (s.bias == SLOT_NO_BIAS || c == 0) break;
      c--;
      adv(2);
    }
  }
  r.win = slotPayout(m, r.stop, bet, &r.lines);
  if (r.win && paidSymbol(m, r.stop, bet, nullptr) == CS_7) {
    // .LinedUpSevens: keep the 7 bias 1 in 4 (1 in 8 on the rarer sittings)
    uint8_t x = (uint8_t)random(256);
    if (x & (s.keepRare ? 0x1C : 0x14)) s.bias = SLOT_NO_BIAS;
  }
  return r;
}

SlotResult slotSpin(SlotSession &s, uint8_t bet) {
  bet = clampBet(bet);
  return s.hall == HALL_GOLDENROD ? gsSpin(s, bet) : redSpin(s, bet);
}

// ---------------------------------------------------------------- card flip
uint8_t cardCreature(uint8_t card) { return card % 4; }
uint8_t cardNumber(uint8_t card) { return (uint8_t)(card / 4 + 1); }

uint16_t cardPayout(int8_t creature, int8_t number, uint8_t card) {
  bool c = creature == CARD_ANY || creature == (int8_t)cardCreature(card);
  bool n = number == CARD_ANY || number == (int8_t)cardNumber(card);
  if (!c || !n) return 0;
  if (creature != CARD_ANY && number != CARD_ANY) return 72;
  if (creature != CARD_ANY) return 12;
  if (number != CARD_ANY) return 18;
  return 0;   // a guess of nothing is not a bet
}

void CardDeck::shuffle() {
  for (uint8_t i = 0; i < CARD_DECK; i++) cards[i] = i;
  for (uint8_t i = CARD_DECK - 1; i > 0; i--) {
    uint8_t j = (uint8_t)random(i + 1);
    uint8_t t = cards[i]; cards[i] = cards[j]; cards[j] = t;
  }
  left = CARD_DECK;
}

uint8_t CardDeck::draw() {
  if (!left) shuffle();
  return cards[--left];
}

// ---------------------------------------------------------------- prizes
static uint8_t mvByName(const char *n) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, n)) return (uint8_t)i;
  return 0;
}

// Celadon: FireRed's counter AND LeafGreen's version exclusive (Pinsir, at
// LeafGreen's level and price), and the TMs, plus SURF by the owner's call
// (2026-10-22) -- the shortcut Surfing Pikachu needs. Goldenrod: Gold's counter
// and Silver's exclusive (Sandshrew). Both versions by the owner's call (v3.55).
// TMs are filled in by name at first use, so a renamed move fails the test
// rather than silently selling move 0.
struct PrizeSrc { PrizeKind kind; uint16_t dex; const char *move; uint8_t level; uint16_t price; };
static const PrizeSrc CELADON_SRC[] = {
  { PRIZE_MON, 63, nullptr, 9, 180 },      // ABRA
  { PRIZE_MON, 35, nullptr, 8, 500 },      // CLEFAIRY
  { PRIZE_MON, 147, nullptr, 18, 2800 },   // DRATINI
  { PRIZE_MON, 123, nullptr, 25, 5500 },   // SCYTHER (FireRed)
  { PRIZE_MON, 127, nullptr, 18, 2500 },   // PINSIR (LeafGreen)
  { PRIZE_MON, 137, nullptr, 26, 9999 },   // PORYGON
  { PRIZE_TM, 0, "SURF", 0, 4000 },
  { PRIZE_TM, 0, "ICE BEAM", 0, 4000 },
  { PRIZE_TM, 0, "IRON TAIL", 0, 3500 },
  { PRIZE_TM, 0, "THUNDERBOLT", 0, 4000 },
  { PRIZE_TM, 0, "SHADOW BALL", 0, 4500 },
  { PRIZE_TM, 0, "FLAMETHROWER", 0, 4000 },
};
static const PrizeSrc GOLDENROD_SRC[] = {
  { PRIZE_MON, 63, nullptr, 5, 200 },      // ABRA
  { PRIZE_MON, 23, nullptr, 5, 700 },      // EKANS (Gold)
  { PRIZE_MON, 27, nullptr, 5, 700 },      // SANDSHREW (Silver)
  { PRIZE_MON, 147, nullptr, 10, 2100 },   // DRATINI
  { PRIZE_TM, 0, "THUNDER", 0, 5500 },
  { PRIZE_TM, 0, "BLIZZARD", 0, 5500 },
  { PRIZE_TM, 0, "FIRE BLAST", 0, 5500 },
};
#define SRC_N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
static Prize gPrizes[HALL_CORNERS][16];
static uint8_t gPrizeN[HALL_CORNERS];
static bool gPrizesBuilt = false;

static void buildPrizes() {
  if (gPrizesBuilt) return;
  const PrizeSrc *src[HALL_CORNERS] = { CELADON_SRC, GOLDENROD_SRC };
  uint8_t n[HALL_CORNERS] = { SRC_N(CELADON_SRC), SRC_N(GOLDENROD_SRC) };
  for (uint8_t h = 0; h < HALL_CORNERS; h++) {
    gPrizeN[h] = 0;
    for (uint8_t i = 0; i < n[h]; i++) {
      Prize p;
      p.kind = src[h][i].kind;
      p.what = p.kind == PRIZE_TM ? mvByName(src[h][i].move) : src[h][i].dex;
      p.level = src[h][i].level;
      p.price = src[h][i].price;
      if (!p.what) continue;               // a move this build does not have
      if (gPrizeN[h] >= sizeof(gPrizes[h]) / sizeof(gPrizes[h][0])) break;
      gPrizes[h][gPrizeN[h]++] = p;
    }
  }
  gPrizesBuilt = true;
}

uint8_t hallPrizeCount(uint8_t hall) {
  buildPrizes();
  return hall < HALL_CORNERS ? gPrizeN[hall] : 0;
}

const Prize &hallPrize(uint8_t hall, uint8_t i) {
  buildPrizes();
  if (hall >= HALL_CORNERS) hall = 0;
  if (i >= gPrizeN[hall]) i = 0;
  return gPrizes[hall][i];
}

// ---------------------------------------------------------------- the halls
struct HallDef { const char *name; uint8_t region; uint8_t gate; bool cards; bool beach; };
// Gates are the wild areas' (gate g opens once ladder trainer g-1 is beaten):
// Celadon after Lt. Surge (Kanto's third), Goldenrod after Bugsy (Johto's
// second) -- the owner's call, 2026-10-22.
static const HallDef HALLS[HALL_COUNT] = {
  { "CELADON GAME CORNER", 0, 3, false, false },     // region 0: Kanto
  { "GOLDENROD GAME CORNER", 1, 2, true, false },    // region 1: Johto
  // Yellow's Summer Beach House is on Route 19. Its gate is the sea routes'
  // own (surfSecretOpen() asks the area), so this one is never read.
  { "SUMMER BEACH HOUSE", 0, 0, false, true },
};

const char *hallName(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].name; }
uint8_t hallRegion(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].region; }
uint8_t hallGate(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].gate; }
bool hallHasCards(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].cards; }
bool hallIsBeach(uint8_t h) { return h < HALL_COUNT && HALLS[h].beach; }

bool hallUnlocked(uint8_t h, const Pet &p) {
  if (h >= HALL_COUNT) return false;
  if (HALLS[h].beach) return surfSecretOpen(p);
  return wildGateOpen(HALLS[h].region, HALLS[h].gate, p);
}

uint8_t hallsIn(uint8_t region, const Pet &p, uint8_t out[HALL_COUNT]) {
  uint8_t n = 0;
  for (uint8_t h = 0; h < HALL_COUNT; h++) {
    if (HALLS[h].region != region) continue;
    if (HALLS[h].beach && !surfSecretOpen(p)) continue;   // secret until it opens
    out[n++] = h;
  }
  return n;
}

uint16_t hallListAfter(uint8_t h) {
  if (h >= HALL_COUNT) return 0;
  uint8_t rg = HALLS[h].region;
  uint16_t first = wildAreaFirst(rg), n = wildAreaCount(rg);
  if (HALLS[h].beach) {
    uint16_t a = wildAreaByName(rg, SURF_AREA_NAME);
    return a == WILD_NO_AREA ? n : (uint16_t)(a - first + 1);
  }
  for (uint16_t i = 0; i < n; i++)
    if (wildAreaGate((uint16_t)(first + i)) > HALLS[h].gate) return i;
  return n;
}

bool surfSecretOpen(const Pet &p) {
  if (p.isEgg() || p.speciesId != 25) return false;        // PIKACHU
  static uint8_t surf = 0;
  if (!surf) surf = mvByName("SURF");
  bool knows = false;
  for (int i = 0; i < MOVE_SLOTS; i++) if (surf && p.moves[i] == surf) knows = true;
  if (!knows) return false;
  uint16_t a = wildAreaByName(0, SURF_AREA_NAME);
  return a != WILD_NO_AREA && wildAreaUnlocked(a, p) && wildAreaAllRegistered(a, p);
}

PartyMon prizeToPartyMon(const Prize &pz, Pet &live) {
  Pet tmp;
  tmp.dbgHatchAs((int16_t)pz.what, false);
  tmp.ageMinutes = (uint32_t)(pz.level ? pz.level - 1 : 0) * MINUTES_PER_LEVEL;
  tmp.relearnFromLevel();
  PartyMon m;
  m.dex = (int16_t)pz.what;
  m.level = pz.level;
  m.ivAtk = (uint8_t)random(32); m.ivDef = (uint8_t)random(32);
  m.ivSpe = (uint8_t)random(32); m.ivHp = (uint8_t)random(32);
  m.shiny = 0;
  for (int i = 0; i < MOVE_SLOTS; i++) m.moves[i] = tmp.moves[i];
  // the same shape as a catch (wildToPartyMon): real care state, its age is
  // its level, and an ID from the live pet's counter
  m.stateVersion = 1;
  m.ageMinutes = tmp.ageMinutes;
  m.lastLearnLevel = pz.level;
  m.uid = live.mintUid();
  return m;
}
