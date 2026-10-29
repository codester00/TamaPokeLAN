// An evolution teaches the new form the moves it has passed (v3.72).
//
// Reported from a board: an ABRA hatched and raised to 100 stood there as a
// KADABRA knowing nothing. The level gates had run against ABRA's list (no
// level-up move here), and evolution only checked a gate AT the evolving level,
// so evolved at 40 every KADABRA move (1, 11, 19, 21, 25, 36) counted as passed.
// Now empty slots fill and the rest are offered -- but never a move the old form
// learned by then too, which was already offered once.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0xAB4; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
static uint32_t gNow = 1; uint32_t millis() { return gNow; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint8_t mvIdx(const char *name) {
  for (int i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, name)) return (uint8_t)i;
  return 0;
}
static bool offered(const Pet &p, uint8_t mv) {
  for (uint8_t q = 0; q < p.learnQCount; q++) if (p.learnQueue[q] == mv) return true;
  return false;
}
// live minutes, cared for, so the gates run exactly as on a board
static void growTo(Pet &p, uint8_t lvl) {
  while (p.level() < lvl) {
    p.fullness = p.joy = p.energy = p.hygiene = 100; p.sleeping = false;
    gNow += PET_TICK_MS; p.update(gNow);
  }
}
static void show(const Pet &p) {
  printf("      %s L%u:", DEX_TBL[p.speciesId].name, p.level());
  for (int i = 0; i < MOVE_SLOTS; i++) printf(" %s", p.moves[i] ? MOVE_TBL[p.moves[i]].name : "-");
  printf("  offers=%u\n", p.learnQCount);
}

int main() {
  const uint8_t CONF = mvIdx("CONFUSION"), CALM = mvIdx("CALM MIND"), NSHADE = mvIdx("NIGHT SHADE"),
                PBEAM = mvIdx("PSYBEAM"), RECOVER = mvIdx("RECOVER"), PSYCHIC = mvIdx("PSYCHIC");
  ck(CONF && CALM && NSHADE && PBEAM && RECOVER && PSYCHIC, "(the moves exist)");

  // ---- the report: an ABRA evolved late, at 40
  {
    Pet p; p.begin();
    p.ageMinutes = 0;                               // hatch() keeps the age it finds
    p.dbgHatchAs(63, false);
    growTo(p, 40);
    ck(p.moveCount() == 0, "(ABRA reaches 40 knowing nothing -- its list has no level-up move)");
    ck(p.canEvolveNow(), "(and could have evolved since 16)");
    p.evolve();
    show(p);
    ck(p.speciesId == 64, "(it is a KADABRA)");
    ck(p.knowsMove(CONF) && p.knowsMove(CALM) && p.knowsMove(NSHADE) && p.knowsMove(PBEAM),
       "the four earliest KADABRA moves fill its empty slots");
    ck(offered(p, RECOVER) && offered(p, PSYCHIC) && p.learnQCount == 2,
       "and the two that do not fit are offered");
    while (p.learnQCount) p.declineLearn();
    growTo(p, 100);
    ck(p.moveCount() == 4, "it reaches 100 with four moves");
  }

  // ---- evolved on time, at 16: CONFUSION and CALM MIND come with it
  {
    Pet p; p.begin();
    p.ageMinutes = 0;                               // hatch() keeps the age it finds
    p.dbgHatchAs(63, false);
    growTo(p, 16);
    p.evolve();
    show(p);
    ck(p.knowsMove(CONF) && p.knowsMove(CALM) && p.moveCount() == 2,
       "an on-time KADABRA learns the moves it has passed");
    growTo(p, 21);
    ck(p.knowsMove(NSHADE) && p.knowsMove(PBEAM), "and the gates fill the rest as it grows");
    growTo(p, 26);
    ck(offered(p, RECOVER), "then offer the fifth");
  }

  // ---- a move the old form already offered is not offered again
  {
    Pet p; p.begin();
    p.ageMinutes = 0;                               // hatch() keeps the age it finds
    p.dbgHatchAs(4, false);                         // CHARMANDER
    growTo(p, 16);
    while (p.learnQCount) p.declineLearn();         // declined everything offered
    uint8_t before[MOVE_SLOTS];
    memcpy(before, p.moves, sizeof(before));
    p.fullness = p.joy = p.energy = p.hygiene = 100;
    ck(p.canEvolveNow(), "(CHARMANDER can evolve at 16)");
    p.evolve();
    show(p);
    ck(p.speciesId == 5, "(it is a CHARMELEON)");
    ck(!memcmp(before, p.moves, sizeof(before)), "a chosen moveset is never overwritten");
    bool reoffer = false;
    for (uint8_t q = 0; q < p.learnQCount; q++) {
      uint8_t mv = p.learnQueue[q];
      // anything CHARMANDER learns by 16 was offered already
      uint8_t n = learnCount(4);
      for (uint8_t i = 0; i < n; i++)
        if (learnLevel(4, i) && learnLevel(4, i) <= 16 && learnMove(4, i) == mv) reoffer = true;
    }
    ck(!reoffer, "nothing CHARMANDER was offered comes back on evolving");
  }

  // ---- becoming the live pet fills empty slots, without offers
  {
    Pet p; p.begin();
    p.dbgHatchAs(64, false);                        // a KADABRA
    p.ageMinutes = 39UL * MINUTES_PER_LEVEL;        // L40
    PartyMon m = p.toPartyMon();
    for (int i = 0; i < MOVE_SLOTS; i++) m.moves[i] = 0;
    m.moves[0] = PSYCHIC;                           // one chosen, three empty
    m.lastLearnLevel = 40;
    p.dbgHatchAs(4, false);
    p.switchTo(m);
    show(p);
    ck(p.speciesId == 64 && p.moves[0] == PSYCHIC, "(the KADABRA is live, its choice kept)");
    ck(p.moveCount() == 4, "its empty slots fill from the moves it has passed");
    ck(p.learnQCount == 0, "and nothing is offered for a swap");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
