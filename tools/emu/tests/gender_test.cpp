// Gender (v3.80, plan § E4): hidden, from the creature's own ID -- forced for
// the games' one-gender and genderless species, 50/50 otherwise and NOT in hatch
// order -- the same after banking and a trade, and drawn on the card. The
// gender-decided level evolutions: Burmy splits, only a female Combee or
// Salandit evolves, and a male one's farewell does not wait for it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x6E7D;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void renderCard();
void renderMonSheet(const PartyMon &m, bool fromBox);
extern Pet pet;
extern bool cardOpen;
extern uint8_t cardPage;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const uint16_t MALE_INK = 0x339C, FEMALE_INK = 0xE20A;
// Is that ink anywhere on the name's line (y 28..64)?
static bool inkOnNameLine(uint16_t ink) {
  const uint16_t *fb = gfx->buffer();
  for (int y = 28; y < 64; y++)
    for (int x = 0; x < 466; x++)
      if (fb[y * 466 + x] == ink) return true;
  return false;
}
// A creature of `dex` at `lvl` whose ID gives `want`.
static void raiseAs(Pet &p, int16_t dex, uint8_t lvl, uint8_t want) {
  p.dbgHatchAs(dex, false);
  for (int i = 0; i < 64; i++) {
    p.uid = p.mintUid();
    if (creatureGender(dex, p.uid) == want) break;
  }
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.energy = p.fullness = p.joy = p.hygiene = 100;
  p.careMistakes = 0;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);

  // ---- the rule ------------------------------------------------------------------
  {
    CreatureUid u = pet.mintUid();
    ck(creatureGender(81, u) == GENDER_NONE, "Magnemite is genderless");
    ck(creatureGender(128, u) == GENDER_MALE && creatureGender(106, u) == GENDER_MALE,
       "Tauros and Hitmonlee are always male");
    ck(creatureGender(113, u) == GENDER_FEMALE && creatureGender(416, u) == GENDER_FEMALE,
       "Chansey and Vespiquen always female");
    int fem = 0, flips = 0, n = 2000;
    uint8_t prev = GENDER_NONE;
    for (int i = 0; i < n; i++) {
      CreatureUid v = pet.mintUid();
      uint8_t g = creatureGender(25, v);
      if (g == GENDER_FEMALE) fem++;
      if (i && g != prev) flips++;
      prev = g;
    }
    ck(fem > n * 45 / 100 && fem < n * 55 / 100, "two-gender species: about half female");
    ck(flips > n * 40 / 100 && flips < n * 60 / 100,
       "and not alternating in hatch order (the serial's parity is not the rule)");
    ck(creatureGender(25, CreatureUid()) == GENDER_NONE, "a creature with no ID yet has none");
  }

  // ---- it travels -------------------------------------------------------------------
  {
    Pet p;
    raiseAs(p, 25, 20, GENDER_FEMALE);
    PartyMon m = p.toPartyMon();
    ck(p.gender() == GENDER_FEMALE && creatureGender(m.dex, m.uid) == GENDER_FEMALE,
       "banked, it keeps its gender");
    Pet q;
    q.switchTo(m);
    ck(q.gender() == GENDER_FEMALE, "and raised again (or received in a trade), the same");
  }

  // ---- the gender-decided evolutions -------------------------------------------------
  {
    Pet p;
    raiseAs(p, 415, 30, GENDER_MALE);
    ck(p.gender() == GENDER_MALE && p.evoInfo().status == Pet::EVO_FINAL && !p.evolutionAhead(),
       "a male Combee is final, and its farewell waits for nothing");
    raiseAs(p, 415, 30, GENDER_FEMALE);
    ck(p.evoInfo().status == Pet::EVO_READY && p.evoInfo().target == 416, "a female Combee evolves");
    p.evolve();
    ck(p.speciesId == 416, "into Vespiquen");
    raiseAs(p, 757, 40, GENDER_MALE);
    ck(p.evoInfo().status == Pet::EVO_FINAL, "a male Salandit is final");
    raiseAs(p, 757, 40, GENDER_FEMALE);
    p.evolve();
    ck(p.speciesId == 758, "a female one becomes Salazzle");
    raiseAs(p, 412, 25, GENDER_MALE);
    p.evolve();
    ck(p.speciesId == 414, "a male Burmy becomes Mothim");
    raiseAs(p, 412, 25, GENDER_FEMALE);
    p.evolve();
    ck(p.speciesId == 413, "a female Burmy becomes Wormadam");
    raiseAs(p, 412, 10, GENDER_MALE);
    ck(p.evoInfo().status == Pet::EVO_LEVELS && p.evoInfo().target == 414,
       "and before level 20 the card names its own branch");
    Pet e;
    bool mothim = false;
    for (int i = 0; i < 4000; i++)
      for (uint8_t t = R_COMUN; t <= R_LEGENDARIO; t++)
        if (e.rollInRegion(3, t) == 414) mothim = true;
    ck(!mothim && DEX_TBL[414].rarity == R_EVO, "Mothim no longer hatches: it is Burmy's");
  }

  // ---- on the card and the sheet ------------------------------------------------------
  {
    gRegionArt = 0xFFFF;
    raiseAs(pet, 25, 20, GENDER_FEMALE);
    cardOpen = true; cardPage = 0;
    renderCard();
    ck(inkOnNameLine(FEMALE_INK) && !inkOnNameLine(MALE_INK), "a female's card shows the red symbol");
    raiseAs(pet, 25, 20, GENDER_MALE);
    renderCard();
    ck(inkOnNameLine(MALE_INK) && !inkOnNameLine(FEMALE_INK), "a male's the blue one");
    raiseAs(pet, 81, 20, GENDER_NONE);
    renderCard();
    ck(!inkOnNameLine(MALE_INK) && !inkOnNameLine(FEMALE_INK), "a genderless one neither");
    raiseAs(pet, 113, 20, GENDER_FEMALE);
    PartyMon m = pet.toPartyMon();
    renderMonSheet(m, false);
    ck(inkOnNameLine(FEMALE_INK), "and a banked creature's sheet shows it too");
    cardOpen = false;
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
