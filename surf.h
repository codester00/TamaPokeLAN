#pragma once
// Surfing Pikachu (v3.54): the rules of the ride, with no drawing, no touch and
// no sensor, so surf_test can play whole runs with a hand-held clock.
//
// After Pokemon Yellow's Summer Beach House minigame, simplified for a round
// panel. Pikachu travels right to left, as in Yellow (the screen mirrors the
// sea; these rules only know "ahead"). The player leans the board -- holding a
// side of the screen, or tilting it -- to set its ANGLE:
//   - leaning with the wave builds speed (forward going down a face, back
//     going up one), against it loses speed;
//   - fast over a crest, it leaves the water. In the air every swing of the
//     board from one side to the other is a TRICK;
//   - it must land at roughly the slope it lands on: a clean landing banks the
//     jump's points, a bad one is a wipeout ("Oh no...") that loses them and
//     most of the speed.
// v3.67 made it playable by hand: the v3.54 sea flipped its slope several
// times a second (only a perfect slope-follower could build speed, and the
// air was too short for a trick). surf_test now plays it as a person would --
// late, with three inputs -- not as a robot.
// The run lasts SURF_RUN_MS, Start to Goal, as Yellow's course does.
#include <stdint.h>

#define SURF_RUN_MS 60000UL
#define SURF_PIKA_X 150           // Pikachu's place on the screen, px
#define SURF_SEA_Y 300            // the calm sea level, px
#define SURF_SPEED_MIN 60.0f      // px a second
#define SURF_SPEED_MAX 260.0f
#define SURF_JUMP_SPEED 150.0f    // fast enough over a crest to take off
#define SURF_LAND_OK 0.6f         // radians off the slope a landing may be
#define SURF_WAVE_FULL 0.35f      // a slope this steep (radians) counts as a full face
#define SURF_ACCEL 170.0f         // px/s^2 for leaning fully with a full face
#define SURF_DRAG 0.12f           // what riding level costs, in the same units
#define SURF_TRICK_ANGLE 0.3f     // radians past level that counts as a swing
#define SURF_GRAVITY 380.0f       // px/s^2 in the air
#define SURF_LEAN 0.45f           // the input a held side gives (of +-1)
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
  uint8_t lastTricks = 0;   // and the tricks in it
  uint32_t lastLandAt = 0;  // elapsed time of the last landing
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
