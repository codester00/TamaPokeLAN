#pragma once
// The Game Corners (v3.53): Celadon's slots (Red), Goldenrod's slots and card
// flip (Crystal), their prize counters, and the coins. The rules live here, with
// no drawing and no touch, so gamecorner_test can play thousands of rounds.
// The screens are in TamaPoke.ino (renderHall/hallTap).
//
// Faithful where it is cheap to be: the reel strips are the games' own
// (pokered's slot_machine_wheels.asm, pokecrystal's Reel1Tilemap..), so are the
// payout tables and Crystal's card-flip odds. Where the games' machines rig the
// odds in engine code we do it more simply, with one rule tuned per hall so a
// coin returns a little under a coin on average (gamecorner_test measures it):
//   - VETO: a winning spin is turned into a losing one this often (Red's
//     machines, which would otherwise pay out more than they take);
//   - BIAS: this often the middle line is set up to win, weighted toward the
//     small prizes (Crystal's machines, which otherwise pay out half).
#include <stdint.h>
#include "pet.h"
#include "party.h"

#define COINS_PER_BUY 50       // the games' price: 50 coins for $1,000
#define COINS_BUY_PRICE 1000
#define SLOT_REELS 3
#define SLOT_MAX_BET 3          // 1: the middle line; 2: + top and bottom; 3: + diagonals
#define CARD_BET 3              // Crystal's card flip always bets 3
#define CARD_DECK 24            // 4 creatures x numbers 1-6

// The two Game Corners have machines and prize counters; the third "hall" is
// the Summer Beach House, Surfing Pikachu's secret (v3.54), which has neither.
enum HallId : uint8_t { HALL_CELADON, HALL_GOLDENROD, HALL_BEACH, HALL_COUNT };
#define HALL_CORNERS 2           // the halls with machines and prizes: 0..1

struct SlotMachine {
  const uint8_t *reel[SLOT_REELS];   // symbol per stop
  uint8_t len;                       // stops per reel
  uint8_t symbols;
  const uint16_t *pay;               // coins per winning line, by symbol
  const uint8_t *biasWeight;         // BIAS: how often each symbol is chosen
  uint8_t biasPerMille;
};
#define SLOT_RETURN_PM 880             // coins back per 1000 bet, on average

struct SlotResult {
  uint8_t stop[SLOT_REELS];   // the stop under the middle line, per reel
  uint16_t win;               // coins paid
  uint8_t lines;              // winning lines (bit 0 middle, 1 top, 2 bottom, 3 and 4 diagonals)
};

const SlotMachine &slotMachine(uint8_t hall);
// The symbol a reel shows `row` rows from its middle-line stop (-1 top, +1 bottom).
uint8_t slotSymbol(const SlotMachine &m, uint8_t reel, uint8_t stop, int8_t row);
// What a set of stops pays at a bet of 1-3. The ONE payout rule: the spin and
// the screen both ask it, so what is shown is what is paid.
uint16_t slotPayout(const SlotMachine &m, const uint8_t stop[SLOT_REELS], uint8_t bet,
                    uint8_t *lines = nullptr);
// The average return of a spin at `bet`, per mille of the bet, before any
// veto: the bare strips plus the bias. Exact, by enumerating every stop.
uint32_t slotExpected(uint8_t hall, uint8_t bet);
// One spin. Decides the stops (the screen only reveals them) and the win.
SlotResult slotSpin(uint8_t hall, uint8_t bet);

// ---- card flip (Goldenrod) --------------------------------------------------
// A card is creature + 4 * (number - 1): creature 0-3 (PIKACHU, JIGGLYPUFF,
// POLIWAG, ODDISH), number 1-6. A guess names a creature, a number, or both
// (-1 = any); Crystal's payouts on a bet of 3: a creature 12, a number 18, the
// exact card 72 -- each exactly fair, as in the game.
#define CARD_ANY -1
uint16_t cardPayout(int8_t creature, int8_t number, uint8_t card);
struct CardDeck {
  uint8_t cards[CARD_DECK];
  uint8_t left = 0;
  void shuffle();
  uint8_t draw();             // shuffles first when empty, as Crystal does
};
uint8_t cardCreature(uint8_t card);
uint8_t cardNumber(uint8_t card);

// ---- prizes -----------------------------------------------------------------
enum PrizeKind : uint8_t { PRIZE_MON, PRIZE_TM };
struct Prize {
  PrizeKind kind;
  uint16_t what;     // a dex number, or a move index for a TM
  uint8_t level;     // a creature's level
  uint16_t price;    // coins
};
uint8_t hallPrizeCount(uint8_t hall);
const Prize &hallPrize(uint8_t hall, uint8_t i);

// ---- the halls --------------------------------------------------------------
const char *hallName(uint8_t hall);
uint8_t hallRegion(uint8_t hall);
uint8_t hallGate(uint8_t hall);       // the same ladder gate as a wild area
bool hallHasCards(uint8_t hall);
bool hallIsBeach(uint8_t hall);
// Open: a Game Corner on the ladder's gate; the beach house only while its
// secret holds (surfSecretOpen()).
bool hallUnlocked(uint8_t hall, const Pet &p);
// The halls a player can SEE in a region, in order; how many. A Game Corner is
// always listed (greyed while locked); the beach house only while it is open --
// nothing hints at it before then, by the owner's call.
uint8_t hallsIn(uint8_t region, const Pet &p, uint8_t out[HALL_COUNT]);

// Surfing Pikachu's secret, the ONE answer the area list and the tap ask:
// the live pet is a PIKACHU that knows SURF, SEA ROUTES 19-21 is unlocked, and
// every species that area offers is registered in the Pokedex, caught or raised
// anywhere (the owner: "all pokemon are the same regardless of where or how
// they're acquired"). Nothing new is saved for it.
#define SURF_AREA_NAME "SEA ROUTES 19-21"
bool surfSecretOpen(const Pet &p);

// A prize creature as a storable record, minted from the live pet's counter
// like a catch.
PartyMon prizeToPartyMon(const Prize &pz, Pet &live);
