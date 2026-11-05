// The Battle Zone's tournament (v4.06, postgame-plan § 2): 16 contestants,
// single elimination, the player plus 15 drawn from every existing opponent
// whose team can be drawn. Pure rules -- the draw, the seeding, who an
// opponent is -- no screen and no battle: the sketch's zone code drives them.
#pragma once
#include <stdint.h>
#include "zone.h"
#include "party.h"

#define TOUR_SIZE 16
#define TOUR_ROUNDS 4
#define TOUR_PLAYER 0xFFFF        // the player's entry in the bracket
#define TOUR_GEN 0x8000           // a generated trainer: TOUR_GEN | k
// A generated filler (only when too few packs are installed for 15 real
// opponents): a gauntlet challenger of this round and level (placeholders).
#define TOUR_GEN_ROUND 8
#define TOUR_GEN_LEVEL 50

// A ladder seat as a contestant id: region * TRAINER_COUNT + seat.
uint16_t tourSeatId(uint8_t region, uint8_t seat);
// Can every creature of this contestant's team be drawn (art and pack)?
bool tourSeatDrawable(uint16_t id);
// THE draw. Fills out[] in BRACKET order, out[0] the player: 15 opponents at
// random from the drawable seats (generated trainers if fewer), seeded by
// strength (the team's best level) so the strongest meet the player late --
// seed 2 only in the final. The same seed always gives the same draw.
void tourDraw(uint32_t seed, uint16_t out[TOUR_SIZE]);
// A contestant as a zone opponent for round `round` (0 = the round of 16):
// a seat's own team at its roster levels, IV 31, its hard training
// (HARD_TRAINING), the smart AI; generated ones from the gauntlet's builder.
bool tourFoe(uint16_t id, uint8_t round, uint32_t seed, uint8_t avatarCount, ZoneFoe &out);
// The best level on a contestant's team (the seeding's strength, and the cap
// the player's team fights at -- the hard ladder's own rule).
uint8_t tourBestLevel(uint16_t id, uint32_t seed);
// Opponents switch (aiSwitchTarget()) in the semi-final and the final only
// (v4.06): the late rounds, where the strongest seeds are.
bool tourSwitches(uint8_t round);

// The saved tournament (NVS key "tour"): one at a time, kept between rounds,
// like the gauntlet. `fighting` is saved before the player's battle opens; a
// reboot with it set is the loss. oppUsed[] is the PP each contestant's
// creatures have used (0 = full), so their PP carries between their battles.
#define TOUR_SAVE_MAGIC 0x54   // 'T'
enum : uint8_t { TOUR_RES_NONE = 0, TOUR_RES_CHAMPION, TOUR_RES_OUT, TOUR_RES_BROKEN };
struct TourSave {
  uint8_t magic = TOUR_SAVE_MAGIC;
  uint8_t version = 1;
  uint8_t active = 0;
  uint8_t fighting = 0;
  uint8_t round = 0;              // 0 = the round of 16 .. 3 = the final
  uint8_t result = TOUR_RES_NONE; // how the last run ended (the page says so)
  uint8_t resultRound = 0;        // the round it ended in
  uint8_t n = 0;
  uint32_t seed = 0;
  uint16_t bracket[TOUR_SIZE] = {};
  uint16_t alive = 0;             // bit i: bracket[i] is still in
  CreatureUid uid[6];
  uint8_t pp[6][MOVE_SLOTS] = {};
  uint8_t oppUsed[TOUR_SIZE][6][MOVE_SLOTS] = {};
};
// The opponent bracket index the player meets in this round (the second
// contestant still in, in bracket order); -1 if none.
int8_t tourPlayerOpponent(const TourSave &t);
