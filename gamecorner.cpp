// The Game Corners' rules -- see gamecorner.h.
#include "gamecorner.h"
#include <Arduino.h>
#include <string.h>
#include "moves.h"
#include "wild.h"

// ---------------------------------------------------------------- the reels
// Celadon: pokered's SlotMachineWheel1-3 (data/events/slot_machine_wheels.asm),
// 18 stops each. Symbols: 7, BAR, CHERRY, FISH, BIRD, MOUSE (the art's order).
enum : uint8_t { RS_7, RS_BAR, RS_CHERRY, RS_FISH, RS_BIRD, RS_MOUSE, RS_COUNT };
static const uint8_t RED_W1[18] = { RS_7, RS_MOUSE, RS_FISH, RS_BAR, RS_CHERRY, RS_7, RS_FISH, RS_BIRD, RS_BAR,
                                    RS_CHERRY, RS_7, RS_MOUSE, RS_BIRD, RS_BAR, RS_CHERRY, RS_7, RS_MOUSE, RS_FISH };
static const uint8_t RED_W2[18] = { RS_7, RS_FISH, RS_CHERRY, RS_BIRD, RS_MOUSE, RS_BAR, RS_CHERRY, RS_FISH, RS_BIRD,
                                    RS_CHERRY, RS_BAR, RS_FISH, RS_BIRD, RS_CHERRY, RS_MOUSE, RS_7, RS_FISH, RS_CHERRY };
static const uint8_t RED_W3[18] = { RS_7, RS_BIRD, RS_FISH, RS_CHERRY, RS_MOUSE, RS_BIRD, RS_FISH, RS_CHERRY, RS_MOUSE,
                                    RS_BIRD, RS_FISH, RS_CHERRY, RS_MOUSE, RS_BIRD, RS_BAR, RS_7, RS_BIRD, RS_FISH };
// Red/Blue: 7s 300, BARs 100, three cherries 8, the creatures 15.
static const uint16_t RED_PAY[RS_COUNT] = { 300, 100, 8, 15, 15, 15 };

// Goldenrod: pokecrystal's Reel1Tilemap-Reel3Tilemap (engine/games/
// slot_machine.asm), 15 stops each. Symbols in SLOTS_* order: 7, POKE BALL,
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
// What the BIAS lines up, small prizes far more often than big ones.
static const uint8_t GS_BIAS[CS_COUNT] = { 2, 6, 40, 25, 15, 12 };

static const SlotMachine MACHINES[HALL_COUNT] = {
  { { RED_W1, RED_W2, RED_W3 }, 18, RS_COUNT, RED_PAY, nullptr, 0 },
  { { GS_W1, GS_W2, GS_W3 }, 15, CS_COUNT, GS_PAY, GS_BIAS, 25 },
};

const SlotMachine &slotMachine(uint8_t hall) {
  return MACHINES[hall < HALL_COUNT ? hall : 0];
}

uint8_t slotSymbol(const SlotMachine &m, uint8_t reel, uint8_t stop, int8_t row) {
  int p = ((int)stop + row) % m.len;
  if (p < 0) p += m.len;
  return m.reel[reel % SLOT_REELS][p];
}

uint16_t slotPayout(const SlotMachine &m, const uint8_t stop[SLOT_REELS], uint8_t bet,
                    uint8_t *lines) {
  // rows per line, for reels 0, 1, 2
  static const int8_t LINE[5][3] = {
    { 0, 0, 0 },      // middle
    { -1, -1, -1 },   // top
    { 1, 1, 1 },      // bottom
    { -1, 0, 1 },     // diagonal down
    { 1, 0, -1 },     // diagonal up
  };
  uint8_t nLines = bet <= 1 ? 1 : bet == 2 ? 3 : 5;
  uint16_t win = 0;
  uint8_t mask = 0;
  for (uint8_t l = 0; l < nLines; l++) {
    uint8_t a = slotSymbol(m, 0, stop[0], LINE[l][0]);
    if (a != slotSymbol(m, 1, stop[1], LINE[l][1]) || a != slotSymbol(m, 2, stop[2], LINE[l][2]))
      continue;
    win += m.pay[a];
    mask |= (uint8_t)(1 << l);
  }
  if (lines) *lines = mask;
  return win;
}

static uint8_t clampBet(uint8_t bet) {
  return bet < 1 ? 1 : bet > SLOT_MAX_BET ? SLOT_MAX_BET : bet;
}

// Every stop of `reel` that shows `sym` on the middle line; how many.
static uint8_t stopsOf(const SlotMachine &m, uint8_t reel, uint8_t sym, uint8_t *at) {
  uint8_t n = 0;
  for (uint8_t p = 0; p < m.len; p++) if (m.reel[reel][p] == sym) at[n++] = p;
  return n;
}

uint32_t slotExpected(uint8_t hall, uint8_t bet) {
  static uint32_t cache[HALL_COUNT][SLOT_MAX_BET + 1];
  const SlotMachine &m = slotMachine(hall);
  if (hall >= HALL_COUNT) hall = 0;
  bet = clampBet(bet);
  if (cache[hall][bet]) return cache[hall][bet];
  // the bare strips: every stop equally likely
  uint64_t won = 0, n = 0;
  uint8_t s[SLOT_REELS];
  for (s[0] = 0; s[0] < m.len; s[0]++)
    for (s[1] = 0; s[1] < m.len; s[1]++)
      for (s[2] = 0; s[2] < m.len; s[2]++) { won += slotPayout(m, s, bet); n++; }
  uint64_t bare = won * 1000000ULL / n;            // in millionths of a coin
  uint64_t ev = bare;
  if (m.biasWeight && m.biasPerMille) {
    // a biased spin: a symbol by weight, then each reel at a random stop of it
    uint32_t total = 0;
    for (uint8_t k = 0; k < m.symbols; k++) total += m.biasWeight[k];
    uint64_t biased = 0;   // sum over symbols of weight * mean payout, millionths
    for (uint8_t sym = 0; sym < m.symbols; sym++) {
      uint8_t a[32], b[32], c[32];
      uint8_t na = stopsOf(m, 0, sym, a), nb = stopsOf(m, 1, sym, b), nc = stopsOf(m, 2, sym, c);
      if (!na || !nb || !nc) continue;
      uint64_t w = 0;
      for (uint8_t i = 0; i < na; i++) for (uint8_t j = 0; j < nb; j++) for (uint8_t k = 0; k < nc; k++) {
        uint8_t st[SLOT_REELS] = { a[i], b[j], c[k] };
        w += slotPayout(m, st, bet);
      }
      biased += (uint64_t)m.biasWeight[sym] * w * 1000000ULL / ((uint64_t)na * nb * nc);
    }
    biased /= total;
    ev = (bare * (1000 - m.biasPerMille) + biased * m.biasPerMille) / 1000;
  }
  uint32_t perMille = (uint32_t)(ev / bet / 1000);
  cache[hall][bet] = perMille ? perMille : 1;
  return cache[hall][bet];
}

SlotResult slotSpin(uint8_t hall, uint8_t bet) {
  const SlotMachine &m = slotMachine(hall);
  bet = clampBet(bet);
  SlotResult r;
  for (int i = 0; i < SLOT_REELS; i++) r.stop[i] = (uint8_t)random(m.len);
  if (m.biasWeight && (uint32_t)random(1000) < m.biasPerMille) {
    // line up one symbol on the middle line
    uint16_t total = 0;
    for (uint8_t s = 0; s < m.symbols; s++) total += m.biasWeight[s];
    uint16_t roll = (uint16_t)random(total);
    uint8_t sym = 0;
    while (sym + 1 < m.symbols && roll >= m.biasWeight[sym]) roll -= m.biasWeight[sym++];
    for (int i = 0; i < SLOT_REELS; i++) {
      uint8_t at[32];
      uint8_t n = stopsOf(m, (uint8_t)i, sym, at);
      if (n) r.stop[i] = at[random(n)];
    }
  }
  r.win = slotPayout(m, r.stop, bet, &r.lines);
  // VETO, at whatever rate brings this bet down to the target return
  uint32_t ev = slotExpected(hall, bet);
  if (r.win && ev > SLOT_RETURN_PM &&
      // per 10,000: random() is 16 bits in the emulator, so a wider roll
      // would never reach the top of its range there
      (uint32_t)random(10000) < (uint32_t)((uint64_t)(ev - SLOT_RETURN_PM) * 10000ULL / ev)) {
    // the third reel slips on until nothing lines up, as a rigged reel does
    for (uint8_t k = 0; k < m.len && r.win; k++) {
      r.stop[2] = (uint8_t)((r.stop[2] + 1) % m.len);
      r.win = slotPayout(m, r.stop, bet, &r.lines);
    }
  }
  return r;
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

// FireRed's Celadon counter (Scyther; LeafGreen's is Pinsir) and its TMs, plus
// SURF by the owner's call (2026-10-22) -- the shortcut Surfing Pikachu needs.
// Gold's Goldenrod counter (Ekans; Silver's is Sandshrew) and its TMs.
// TMs are filled in by name at first use, so a renamed move fails the test
// rather than silently selling move 0.
struct PrizeSrc { PrizeKind kind; uint16_t dex; const char *move; uint8_t level; uint16_t price; };
static const PrizeSrc CELADON_SRC[] = {
  { PRIZE_MON, 63, nullptr, 9, 180 },      // ABRA
  { PRIZE_MON, 35, nullptr, 8, 500 },      // CLEFAIRY
  { PRIZE_MON, 147, nullptr, 18, 2800 },   // DRATINI
  { PRIZE_MON, 123, nullptr, 25, 5500 },   // SCYTHER
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
  { PRIZE_MON, 23, nullptr, 5, 700 },      // EKANS
  { PRIZE_MON, 147, nullptr, 10, 2100 },   // DRATINI
  { PRIZE_TM, 0, "THUNDER", 0, 5500 },
  { PRIZE_TM, 0, "BLIZZARD", 0, 5500 },
  { PRIZE_TM, 0, "FIRE BLAST", 0, 5500 },
};
#define SRC_N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
static Prize gPrizes[HALL_COUNT][12];
static uint8_t gPrizeN[HALL_COUNT];
static bool gPrizesBuilt = false;

static void buildPrizes() {
  if (gPrizesBuilt) return;
  const PrizeSrc *src[HALL_COUNT] = { CELADON_SRC, GOLDENROD_SRC };
  uint8_t n[HALL_COUNT] = { SRC_N(CELADON_SRC), SRC_N(GOLDENROD_SRC) };
  for (uint8_t h = 0; h < HALL_COUNT; h++) {
    gPrizeN[h] = 0;
    for (uint8_t i = 0; i < n[h]; i++) {
      Prize p;
      p.kind = src[h][i].kind;
      p.what = p.kind == PRIZE_TM ? mvByName(src[h][i].move) : src[h][i].dex;
      p.level = src[h][i].level;
      p.price = src[h][i].price;
      if (!p.what) continue;               // a move this build does not have
      gPrizes[h][gPrizeN[h]++] = p;
    }
  }
  gPrizesBuilt = true;
}

uint8_t hallPrizeCount(uint8_t hall) {
  buildPrizes();
  return hall < HALL_COUNT ? gPrizeN[hall] : 0;
}

const Prize &hallPrize(uint8_t hall, uint8_t i) {
  buildPrizes();
  if (hall >= HALL_COUNT) hall = 0;
  if (i >= gPrizeN[hall]) i = 0;
  return gPrizes[hall][i];
}

// ---------------------------------------------------------------- the halls
struct HallDef { const char *name; uint8_t region; uint8_t gate; bool cards; };
// Gates are the wild areas' (gate g opens once ladder trainer g-1 is beaten):
// Celadon after Lt. Surge (Kanto's third), Goldenrod after Bugsy (Johto's
// second) -- the owner's call, 2026-10-22.
static const HallDef HALLS[HALL_COUNT] = {
  { "CELADON GAME CORNER", 0, 3, false },     // region 0: Kanto
  { "GOLDENROD GAME CORNER", 1, 2, true },    // region 1: Johto
};

const char *hallName(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].name; }
uint8_t hallRegion(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].region; }
uint8_t hallGate(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].gate; }
bool hallHasCards(uint8_t h) { return HALLS[h < HALL_COUNT ? h : 0].cards; }
bool hallUnlocked(uint8_t h, const Pet &p) {
  return h < HALL_COUNT && wildGateOpen(HALLS[h].region, HALLS[h].gate, p);
}

uint8_t hallsIn(uint8_t region, uint8_t out[HALL_COUNT]) {
  uint8_t n = 0;
  for (uint8_t h = 0; h < HALL_COUNT; h++) if (HALLS[h].region == region) out[n++] = h;
  return n;
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
