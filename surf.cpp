// Surfing Pikachu's rules -- see surf.h.
#include "surf.h"
#include <math.h>

static const float TWO_PI_F = 6.2831853f;

// One long swell a person can read and lean into, and a gentle second one
// that keeps the crests uneven. The v3.54 sea added a 77 px ripple and a
// 170 px swell, so the slope under Pikachu flipped several times a second.
float surfHeight(float x) {
  return 58.0f * sinf(x * TWO_PI_F / 900.0f) +
         16.0f * sinf(x * TWO_PI_F / 360.0f + 1.3f);
}

float surfSlopeAngle(float x) {
  const float d = 2.0f;
  return atanf((surfHeight(x + d) - surfHeight(x - d)) / (2 * d));
}

void SurfGame::start() { *this = SurfGame(); }

void SurfGame::step(uint32_t dt, float tilt) {
  if (over() || !dt) return;
  if (dt > 200) dt = 200;                  // a stalled frame is not a teleport
  elapsed += dt;
  float s = dt / 1000.0f;
  if (tilt > 1) tilt = 1;
  if (tilt < -1) tilt = -1;
  // the board follows the player's tilt, quickly but not instantly
  float target = tilt * 1.1f;
  angle += (target - angle) * (s * 8 > 1 ? 1 : s * 8);

  worldX += speed * s;
  float px = worldX + SURF_PIKA_X;
  float surfaceY = SURF_SEA_Y - surfHeight(px);
  float slope = surfSlopeAngle(px);

  if (wipeUntil && elapsed < wipeUntil) {   // tumbling: no control, no points
    air = false; y = surfaceY; angle = slope; prevSlope = slope;
    return;
  }
  wipeUntil = 0;

  if (air) {
    airMs += dt;
    vy += SURF_GRAVITY * s;
    y += vy * s;
    int8_t side = angle > SURF_TRICK_ANGLE ? 1 : angle < -SURF_TRICK_ANGLE ? -1 : 0;
    if (side && side != lastSide) {
      if (lastSide) tricks++;                // a full swing across
      lastSide = side;
    }
    if (y >= surfaceY) {                     // back on the water
      air = false;
      y = surfaceY;
      vy = 0;
      lastLandAt = elapsed;
      lastTricks = tricks;
      if (fabsf(angle - slope) <= SURF_LAND_OK) {
        lastBank = 10 + airMs / 50 + 50UL * tricks;
        score += lastBank;
        cleanLandings++;
      } else {
        lastBank = 0;
        wipeouts++;
        speed = SURF_SPEED_MIN;
        wipeUntil = elapsed + SURF_WIPE_MS;
      }
    }
    prevSlope = slope;
    return;
  }

  // on the water: leaning WITH the wave builds speed -- forward (nose down)
  // going down a face, back going up one -- and against it costs speed. It
  // rewards the direction of the lean, not an exact angle: a person holds a
  // side or tilts roughly, and v3.54's exact-angle rule made a held lean worse
  // than doing nothing on every gentle slope. Riding level slowly loses speed.
  y = surfaceY;
  float lean = angle / (SURF_LEAN * 1.1f);
  float wave = slope / SURF_WAVE_FULL;
  if (lean > 1) lean = 1;
  if (lean < -1) lean = -1;
  if (wave > 1) wave = 1;
  if (wave < -1) wave = -1;
  float agree = lean * wave;                              // +1 with it, -1 against
  speed += (agree - SURF_DRAG) * SURF_ACCEL * s;
  if (speed < SURF_SPEED_MIN) speed = SURF_SPEED_MIN;
  if (speed > SURF_SPEED_MAX) speed = SURF_SPEED_MAX;
  if (agree > 0.3f) score += (uint32_t)(speed * s / 10.0f + 0.5f);   // riding well

  // over a crest (the slope turns from rising to falling) fast enough: airborne
  if (prevSlope > 0.05f && slope <= 0.05f && speed >= SURF_JUMP_SPEED) {
    air = true;
    airMs = 0;
    tricks = 0;
    lastSide = 0;
    vy = -speed;
    jumps++;
  }
  prevSlope = slope;
}

uint16_t surfCoins(uint32_t score) {
  uint32_t c = score / 10;
  return (uint16_t)(c > 500 ? 500 : c);
}

uint8_t surfJoyScore(uint32_t score) {
  uint32_t j = score / 25;
  return (uint8_t)(j > 40 ? 40 : j);
}
