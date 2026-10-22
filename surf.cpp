// Surfing Pikachu's rules -- see surf.h.
#include "surf.h"
#include <math.h>

static const float TWO_PI_F = 6.2831853f;

// Two swells of different lengths, so the sea never repeats in a way a player
// can learn by rote, and a gentle third that keeps the crests uneven.
float surfHeight(float x) {
  return 46.0f * sinf(x * TWO_PI_F / 420.0f) +
         20.0f * sinf(x * TWO_PI_F / 170.0f + 1.3f) +
         8.0f * sinf(x * TWO_PI_F / 77.0f + 0.4f);
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
    vy += 600.0f * s;                        // gravity, px/s^2
    y += vy * s;
    int8_t side = angle > 0.6f ? 1 : angle < -0.6f ? -1 : 0;
    if (side && side != lastSide) {
      if (lastSide) tricks++;                // a full swing across
      lastSide = side;
    }
    if (y >= surfaceY) {                     // back on the water
      air = false;
      y = surfaceY;
      vy = 0;
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

  // on the water: riding the face builds speed, fighting it costs speed, and
  // running down a face (the surface falling away ahead) is free speed
  y = surfaceY;
  float miss = fabsf(angle - slope);
  float q = 1.0f - (miss > 0.6f ? 1.0f : miss / 0.6f);   // 1 = matched
  speed += (q - 0.35f) * 220.0f * s - slope * 60.0f * s;
  if (speed < SURF_SPEED_MIN) speed = SURF_SPEED_MIN;
  if (speed > SURF_SPEED_MAX) speed = SURF_SPEED_MAX;
  if (q > 0.7f) score += (uint32_t)(speed * s / 10.0f + 0.5f);   // riding well

  // over a crest (the slope turns from rising to falling) fast enough: airborne
  if (prevSlope > 0.05f && slope <= 0.05f && speed >= SURF_JUMP_SPEED) {
    air = true;
    airMs = 0;
    tricks = 0;
    lastSide = 0;
    vy = -speed * 0.9f;
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
