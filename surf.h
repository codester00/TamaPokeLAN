#pragma once
// Surfing Pikachu (v3.54): the rules of the ride, with no drawing, no touch and
// no sensor, so surf_test can play whole runs with a hand-held clock.
//
// After Pokemon Yellow's Summer Beach House minigame, simplified for a round
// panel and a tilt sensor. Pikachu rides a sea whose surface scrolls in from
// the right; the player tilts the board to set its ANGLE:
//   - matching the slope of the wave under it builds speed, fighting it loses
//     speed;
//   - fast over a crest, it leaves the water. In the air every swing of the
//     board from one side to the other is a TRICK;
//   - it must land at roughly the slope it lands on: a clean landing banks the
//     jump's points, a bad one is a wipeout ("Oh no...") that loses them and
//     most of the speed.
// The run lasts SURF_RUN_MS, Start to Goal, as Yellow's course does.
#include <stdint.h>

#define SURF_RUN_MS 60000UL
#define SURF_PIKA_X 150           // Pikachu's place on the screen, px
#define SURF_SEA_Y 300            // the calm sea level, px
#define SURF_SPEED_MIN 60.0f      // px a second
#define SURF_SPEED_MAX 260.0f
#define SURF_JUMP_SPEED 150.0f    // fast enough over a crest to take off
#define SURF_LAND_OK 0.55f        // radians off the slope a landing may be
#define SURF_WIPE_MS 1200         // how long a wipeout lasts

struct SurfGame {
  uint32_t elapsed = 0;     // ms since the start
  float worldX = 0;         // how far the sea has scrolled
  float speed = SURF_SPEED_MIN;
  float angle = 0;          // the board, radians, nose up positive
  float y = SURF_SEA_Y;     // Pikachu's height on the screen
  float vy = 0;
  bool air = false;
  uint32_t airMs = 0;
  uint8_t tricks = 0;       // swings while airborne this jump
  int8_t lastSide = 0;      // which way the board last swung past the middle
  uint32_t wipeUntil = 0;   // elapsed time a wipeout lasts until
  uint32_t score = 0;
  uint16_t jumps = 0, cleanLandings = 0, wipeouts = 0;
  uint32_t lastBank = 0;    // what the last landing was worth, for the screen
  float prevSlope = 0;

  void start();
  // One step: dt in ms, `tilt` the player's input in -1..1 (positive = nose up).
  void step(uint32_t dt, float tilt);
  bool over() const { return elapsed >= SURF_RUN_MS; }
};

// The sea: its height above SURF_SEA_Y at a world position, and its slope as
// an angle (radians, positive where it rises toward the right).
float surfHeight(float worldX);
float surfSlopeAngle(float worldX);

// What a run pays: coins for the score, and the joy/bond score handed to
// Pet::playJoyGame(). The high score is the caller's to keep.
uint16_t surfCoins(uint32_t score);
uint8_t surfJoyScore(uint32_t score);
