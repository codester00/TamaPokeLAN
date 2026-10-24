// The secret codes (v3.59): eight berries tapped in the Memory game's WATCH
// phase (0 up, 1 right, 2 down, 3 left -- simonBerryPos()). DEVELOPERS ONLY:
// never in the README, the release notes or the installer page. The sequences
// are listed in CLAUDE.md. codeApply() in the sketch is the one place a code
// does anything; `CODE <name>` on the console reaches it too.
#pragma once
#include <stdint.h>

enum CodeId : uint8_t { CODE_CELADON, CODE_GOLDENROD, CODE_SURF, CODE_SETTINGS, CODE_CHEATS, CODE_COUNT };
#define CODE_LEN 8

struct SecretCode { const char *name; uint8_t seq[CODE_LEN]; };

static const SecretCode CODES[CODE_COUNT] = {
  { "CELADON",   { 0, 1, 2, 3, 0, 1, 2, 3 } },   // round clockwise, twice
  { "GOLDENROD", { 0, 3, 2, 1, 0, 3, 2, 1 } },   // round anticlockwise, twice
  { "SURF",      { 0, 2, 0, 2, 1, 3, 1, 3 } },
  { "SETTINGS",  { 0, 0, 2, 2, 3, 1, 3, 1 } },   // up up down down left right left right
  { "CHEATS",    { 2, 2, 0, 0, 1, 3, 1, 3 } },   // the same, upside down
};

// Which code the last CODE_LEN taps (oldest first) spell, or -1.
inline int codeMatch(const uint8_t taps[CODE_LEN]) {
  for (int c = 0; c < CODE_COUNT; c++) {
    int k = 0;
    while (k < CODE_LEN && taps[k] == CODES[c].seq[k]) k++;
    if (k == CODE_LEN) return c;
  }
  return -1;
}
