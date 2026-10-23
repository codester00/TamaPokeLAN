// The legendary safety net (v3.56): after LEGEND_SAFETY_EGGS farewell eggs in a
// row without a legendary, the next farewell egg is one. SECRET -- nothing on
// screen says so -- so this suite is the only thing that says it works.
//
// Every guard has a companion showing the mechanism engaged: "never more than
// ten dry farewell eggs" is vacuous unless the net actually fired, and it is
// only interesting because, without it, longer runs DO happen (section 2).
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "noart.h"
#include "wild.h"
#include <cstdio>
uint32_t g_seed = 0x1E6E; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const uint8_t UNOVA = 4;   // REGIONS order; seven egg legendaries
static bool isLeg(int16_t d) { return d >= 1 && d <= DEX_COUNT && DEX_TBL[d].rarity == R_LEGENDARIO; }
static void reg(Pet &p, int16_t d) { p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7)); }

// A pet past the 25-species gate, its eggs from Unova, nothing of Unova seen.
static void fresh(Pet &p) {
  p.begin();
  // begin() loads what an earlier section saved: start from a clean Pokedex
  memset(p.dexReg, 0, sizeof(p.dexReg));
  for (int16_t d = 1; d <= 60; d++) reg(p, d);
  p.region = UNOVA;
  p.legendDry = 0;
  p.legendAttempts = 0;
}
// Lay `n` eggs after the given ending; the eggs are never hatched, so whatever
// was unregistered stays unregistered and the net stays in play. Returns the
// longest run of non-legendary eggs, and counts the eggs the net decided.
static int lay(Pet &p, uint8_t ending, int n, int *forced, int *legs) {
  int run = 0, longest = 0;
  *forced = 0; *legs = 0;
  for (int i = 0; i < n; i++) {
    bool due = p.legendDry >= LEGEND_SAFETY_EGGS;
    p.lastEnd = ending;
    p.newEgg();
    int16_t d = p.eggPeek();
    if (isLeg(d)) { (*legs)++; if (due) (*forced)++; run = 0; }
    else if (++run > longest) longest = run;
  }
  return longest;
}

int main() {
  gRegionArt = 0xFFFF;

  // --- 1. farewell eggs: never more than LEGEND_SAFETY_EGGS dry, and the net fires
  {
    Pet p; fresh(p);
    ck(p.legendSafetyActive(), "past 25 species, with Unova's legendaries unseen, the net is in play");
    int forced, legs;
    int longest = lay(p, CER_FAREWELL, 3000, &forced, &legs);
    printf("      3000 farewell eggs: %d legendary, %d of them the net's, longest dry run %d\n", legs, forced, longest);
    ck(longest <= LEGEND_SAFETY_EGGS, "no more than LEGEND_SAFETY_EGGS farewell eggs in a row without a legendary");
    ck(forced > 0, "and the net really did decide some of them");
    ck(legs - forced > 0, "while the 10% roll still hands them out on its own");
  }

  // --- 2. release and early retire are not the good ending: no net
  {
    Pet p; fresh(p);
    int forced, legs;
    int longest = lay(p, CER_RELEASE, 3000, &forced, &legs);
    printf("      3000 release eggs: %d legendary, longest dry run %d\n", legs, longest);
    ck(forced == 0 && longest > LEGEND_SAFETY_EGGS, "after a release the net never fires, and long dry runs happen");
    ck(p.legendDry == 0, "and a release egg never builds the count");
    // a farm of early retires cannot bank a count for a later farewell
    p.legendDry = 3;
    p.lastEnd = CER_RELEASE;
    int16_t before = p.legendDry;
    for (int i = 0; i < 20; i++) { p.lastEnd = CER_RELEASE; p.newEgg(); if (isLeg(p.eggPeek())) before = 0; }
    ck(p.legendDry == before, "release eggs leave a farewell count where it was (or reset it on a legendary)");
  }

  // --- 3. the console's EGGS simulation neither builds nor spends it
  {
    Pet p; fresh(p);
    p.lastEnd = CER_FAREWELL;
    p.legendDry = LEGEND_SAFETY_EGGS;
    for (int i = 0; i < 200; i++) p.pickEggSpecies();
    ck(p.legendDry == LEGEND_SAFETY_EGGS, "pickEggSpecies() alone does not move the count");
    bool allLeg = true;
    for (int i = 0; i < 200; i++) if (!isLeg(p.pickEggSpecies())) allLeg = false;
    ck(allLeg, "with the count due, every simulated farewell roll is a legendary");
  }

  // --- 4. nothing left to hatch: the net steps aside
  {
    Pet p; fresh(p);
    const RegionInfo &rg = REGIONS[UNOVA];
    for (int16_t d = rg.lo; d <= rg.hi; d++) if (isLeg(d)) reg(p, d);
    ck(!p.legendSafetyActive(), "with every Unova legendary registered the net is not in play");
    int forced, legs;
    lay(p, CER_FAREWELL, 300, &forced, &legs);
    ck(p.legendDry == 0 && forced == 0, "and farewell eggs neither count nor force");
    Pet q; q.begin();   // under the 25-species gate
    q.region = UNOVA;
    ck(!q.legendSafetyActive(), "nor below the 25 species the legendary tier itself waits for");
  }

  // --- 5. the count survives a reload, through the checkpoint and the legacy key
  {
    Pet p; fresh(p);
    p.legendDry = 7;
    p.renameTrainer("ASH");   // a public path that saves
    { Pet r; r.begin(); ck(r.legendDry == 7, "the count survives a reload"); }
    // the legacy key alone (as a downgrade or an IMPORT of old keys would see it)
    Preferences pr; pr.begin("tamapoke", false);
    pr.remove("plyA"); pr.remove("plyB");
    pr.end();
    { Pet r; r.begin(); ck(r.legendDry == 7, "and through the legacy key alone"); }
  }

  // --- 6. the farming lockout: release eggs past the free attempts roll no legendary
  {
    // an honest mistake: the first LEGEND_FREE_ATTEMPTS release eggs keep 3 %
    Pet p; fresh(p);
    int legs = 0;
    p.lastEnd = CER_RELEASE;
    for (int i = 0; i < 4000; i++) {
      p.legendAttempts = (uint8_t)(i % LEGEND_FREE_ATTEMPTS);
      if (isLeg(p.pickEggSpecies())) legs++;
    }
    printf("      4000 rolls within the free attempts: %d legendary\n", legs);
    ck(legs > 60 && legs < 200, "the first release eggs since a farewell keep the plain 3%");
    p.legendAttempts = LEGEND_FREE_ATTEMPTS - 1;
    ck(!p.legendLockedFor(CER_RELEASE), "the last free attempt is not locked");
    p.legendAttempts = LEGEND_FREE_ATTEMPTS;
    ck(p.legendLockedFor(CER_RELEASE), "the one after it is");

    // a farm: one release egg after another
    Pet f; fresh(f);
    int late = 0, rare = 0, all = 0;
    for (int i = 0; i < 4000; i++) {
      f.lastEnd = CER_RELEASE;
      f.newEgg();
      int16_t d = f.eggPeek();
      if (i >= LEGEND_FREE_ATTEMPTS) {
        all++;
        if (isLeg(d)) late++;
        if (d >= 1 && d <= DEX_COUNT && DEX_TBL[d].rarity == R_RARO) rare++;
      }
    }
    printf("      4000 release eggs in a row: %d legendary past the free ones, %d rare of %d\n", late, rare, all);
    ck(f.legendAttempts >= LEGEND_FREE_ATTEMPTS, "release eggs build the attempt count");
    ck(late == 0, "past the free attempts no release egg is a legendary");
    ck(rare > all / 10, "but the rare tier is untouched -- only the legendary chance is locked");

    // a farewell clears it, and is never locked itself
    f.lastEnd = CER_FAREWELL;
    ck(!f.legendLockedFor(CER_FAREWELL), "a farewell egg is never locked out");
    int flegs = 0;
    uint8_t keep = f.legendAttempts;
    f.legendDry = 0;
    for (int i = 0; i < 2000; i++) if (isLeg(f.pickEggSpecies())) flegs++;
    ck(flegs > 0 && f.legendAttempts == keep, "a locked player's farewell rolls still give legendaries (and EGGS moves nothing)");
    f.newEgg();
    ck(f.legendAttempts == 0, "laying the farewell egg clears the count");
    ck(!f.legendLockedFor(CER_RELEASE), "so the next release egg has its 3% back");

    // a runaway neither counts nor clears
    f.legendAttempts = 5;
    f.lastEnd = CER_RUNAWAY;
    f.newEgg();
    ck(f.legendAttempts == 5, "a runaway egg leaves the count where it was");
  }

  // --- 7. the attempt count survives a reload, through the checkpoint and the legacy key
  {
    Pet p; fresh(p);
    p.legendAttempts = 9;
    p.renameTrainer("RED");
    { Pet r; r.begin(); ck(r.legendAttempts == 9, "the attempt count survives a reload"); }
    Preferences pr; pr.begin("tamapoke", false);
    pr.remove("plyA"); pr.remove("plyB");
    pr.end();
    { Pet r; r.begin(); ck(r.legendAttempts == 9, "and through the legacy key alone"); }
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
