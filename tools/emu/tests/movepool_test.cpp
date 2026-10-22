// The move expansion (v3.50): every species can attack by level-up, and a
// damaging move's stat change (Bubble Beam, Close Combat) really lands.
//
// The owner's rule: no creature may be left without an attack beyond the ones
// the real games leave that way. EXEMPT is that list, by name -- a new entry
// here is a decision, not a fix.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "moves.h"
#include "dex.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0x3A11;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// No attack in the real games either (Harden, Teleport, Transform, Sketch,
// Splash...), and five whose only real attacks are one-off effects this
// engine does not have: Hidden Power, Counter/Mirror Coat, Bide, Astonish.
static const char *const EXEMPT[] = {
  "METAPOD", "KAKUNA", "ABRA", "DITTO", "SMEARGLE", "SPEWPA", "COSMOG", "COSMOEM",
  "UNOWN", "WOBBUFFET", "WYNAUT", "PYUKUMUKU", "APPLIN",
};

static bool exempt(int16_t d) {
  for (const char *n : EXEMPT) if (!strcmp(DEX_TBL[d].name, n)) return true;
  return false;
}

static uint8_t moveId(const char *name) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, name)) return (uint8_t)i;
  return 0;
}

static void mkCombatant(Combatant &c, int16_t dex, uint8_t lvl) {
  Pet p;
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  combatantFromPet(c, p);
}

int main() {
  // --- 1. every species has an attacking level-up move, bar the exempt
  int missing = 0, exemptSeen = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    bool attack = false;
    for (uint8_t i = 0; i < learnCount(d); i++) {
      uint8_t mv = learnMove(d, i);
      if (learnLevel(d, i) > 0 && mv && mv < MOVE_COUNT && MOVE_TBL[mv].cat != MC_STATUS) attack = true;
    }
    if (exempt(d)) { exemptSeen++; continue; }
    if (!attack) { printf("      %s (%d) has no attacking level-up move\n", DEX_TBL[d].name, d); missing++; }
  }
  ck(missing == 0, "every species but the exempt can attack by level-up");
  ck(exemptSeen == (int)(sizeof(EXEMPT) / sizeof(EXEMPT[0])), "and every exempt name is a real species");

  // --- 2. the move table stays inside its one-byte index
  ck(MOVE_COUNT <= 256, "the move table fits the one-byte index every save stores");
  int hits = 0;
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (MOVE_TBL[i].hitMask) hits++;
  ck(hits >= 40, "damaging moves carry their stat changes");
  ck(MOVE_TBL[moveId("PSYCHIC")].hitMask == ST_SPD, "PSYCHIC regained its SP. DEF drop");
  for (uint16_t i = 1; i < MOVE_COUNT; i++)
    if (MOVE_TBL[i].hitMask && MOVE_TBL[i].cat == MC_STATUS) { ck(false, "a status move with a hit stat change"); break; }

  // --- 3. a guaranteed stat drop on the target lands, and is logged
  uint8_t mud = moveId("MUD SHOT");
  ck(mud && MOVE_TBL[mud].hitChance == 100 && MOVE_TBL[mud].hitTarget == TG_FOE, "MUD SHOT always slows the target");
  Combatant a, b;
  mkCombatant(a, 50, 40);     // DIGLETT
  mkCombatant(b, 129, 40);    // MAGIKARP: survives one Mud Shot
  TurnLog lg;
  int tries = 0;
  do { mkCombatant(b, 129, 40); battleAct(a, b, mud, lg); } while ((lg.missed || lg.damage == 0) && ++tries < 50);
  ck(lg.damage > 0 && b.stage[SI_SPE] == -1, "the target's SPEED fell one stage");
  ck(lg.stageMask == ST_SPE && lg.stageDelta == -1 && !lg.stageOnUser, "and the log says so, on the target");

  // --- 4. one on the user lands on the user (CLOSE COMBAT)
  uint8_t cc = moveId("CLOSE COMBAT");
  ck(cc && MOVE_TBL[cc].hitTarget == TG_SELF && MOVE_TBL[cc].hitStages == -1, "CLOSE COMBAT lowers the user");
  mkCombatant(a, 68, 60);     // MACHAMP
  tries = 0;
  do { mkCombatant(b, 143, 60); battleAct(a, b, cc, lg); } while ((lg.missed || lg.damage == 0) && ++tries < 50);
  ck(a.stage[SI_DEF] == -1 && a.stage[SI_SPD] == -1 && b.stage[SI_DEF] == 0, "the user's defences fell, not the target's");
  ck(lg.stageOnUser, "and the log puts it on the user");

  // --- 5. a target that faints takes no stat change
  mkCombatant(a, 50, 100);
  mkCombatant(b, 129, 2);
  tries = 0;
  do { mkCombatant(b, 129, 2); battleAct(a, b, mud, lg); } while (lg.missed && ++tries < 50);
  ck(b.fainted() && b.stage[SI_SPE] == 0 && lg.stageMask == 0, "a fainted target is not slowed");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
