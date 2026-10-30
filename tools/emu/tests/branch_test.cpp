// Branches with no item (v3.82, plan § E3/E6/E8): Tyrogue by Attack against
// Defense, Wurmple and Cosmoem by the creature's ID -- Cosmoem forced to the
// one of Solgaleo and Lunala the player does not own, and an evolution owning
// it as a catch does -- Applin's and Charcadet's choice through the dialog, and
// Nincada's Shedinja for a Poke Ball and room. Played through onTap().
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "wild.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0xB8A4;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void onTap(int16_t x, int16_t y);
void render();
void uiEvolveBtnCenter(int *x, int *y);
void evoPickRect(int i, int *top, int *bot);
uint8_t ownedCount(int16_t dex);
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
extern Pet pet;
extern Party party;
extern Inventory bag;
extern uint8_t choiceKind;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void raise(Pet &p, int16_t dex, uint8_t lvl) {
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.energy = p.fullness = p.joy = p.hygiene = 100;
  p.careMistakes = 0;
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 15;
  p.trAtk = p.trDef = p.trSpe = 0;
  p.relearnFromLevel();
  while (p.hasLearnOffer()) p.declineLearn();
}
static int legendOf(int16_t dex) {
  for (uint8_t i = 0; i < wildLegendCount(); i++) if (wildLegendDex(i) == dex) return i;
  return -1;
}
static void tapEvolve() {
  int x, y;
  uiEvolveBtnCenter(&x, &y);
  onTap((int16_t)x, (int16_t)y);
}

int main() {
  setup();
  gRegionArt = 0xFFFF;
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  ck(DEX_TBL[107].rarity == R_EVO && DEX_TBL[237].rarity == R_EVO && DEX_TBL[268].rarity == R_EVO &&
     DEX_TBL[292].rarity == R_EVO && DEX_TBL[842].rarity == R_EVO && DEX_TBL[1011].rarity == R_EVO &&
     DEX_TBL[937].rarity == R_EVO, "Hitmonchan/top, Cascoon, Shedinja, Appletun, Dipplin, Ceruledge no longer hatch");

  // ---- Tyrogue ------------------------------------------------------------------------
  {
    Pet p;
    raise(p, 236, 25);
    ck(p.atkStat() == p.defStat() && p.evoInfo().target == 237, "Attack equal to Defense: Hitmontop");
    p.trAtk = 30;
    ck(p.atkStat() > p.defStat() && p.evoInfo().target == 106, "higher Attack: Hitmonlee");
    p.trAtk = 0; p.trDef = 30;
    ck(p.evoInfo().target == 107, "higher Defense: Hitmonchan");
    p.evolve();
    ck(p.speciesId == 107, "and it becomes that one");
  }

  // ---- Wurmple: by the ID, fixed, and not the gender ----------------------------------
  {
    Pet p;
    int sil = 0, cas = 0, sameAsGender = 0, n = 400;
    for (int i = 0; i < n; i++) {
      raise(p, 265, 10);
      p.uid = p.mintUid();
      int16_t t = p.evoInfo().target;
      if (t == 266) sil++; else if (t == 268) cas++;
      if ((t == 268) == (p.gender() == GENDER_FEMALE)) sameAsGender++;
      if (p.evoInfo().target != t) { cas = -9999; break; }
    }
    ck(sil > n * 40 / 100 && cas > n * 40 / 100, "Wurmple: Silcoon or Cascoon, about half each, steady per creature");
    ck(sameAsGender > n * 35 / 100 && sameAsGender < n * 65 / 100, "and not tied to its gender");
  }

  // ---- Cosmoem ---------------------------------------------------------------------------
  {
    int sol = legendOf(791), lun = legendOf(792);
    ck(sol >= 0 && lun >= 0 && wildLegendHome(sol) == wildLegendHome(lun), "(Solgaleo and Lunala share a home)");
    Pet p;
    bool sawSol = false, sawLun = false;
    for (int i = 0; i < 200; i++) {
      raise(p, 790, 60); p.uid = p.mintUid();
      int16_t t = p.evoInfo().target;
      if (t == 791) sawSol = true;
      if (t == 792) sawLun = true;
    }
    ck(sawSol && sawLun, "owning neither, the ID decides: both come up");
    p.setLegendCaught((uint8_t)sol);
    bool allLun = true;
    for (int i = 0; i < 200; i++) { p.uid = p.mintUid(); if (p.evoInfo().target != 792) allLun = false; }
    ck(allLun, "owning Solgaleo, every Cosmoem becomes Lunala");
    Pet q;
    raise(q, 790, 60);
    q.setLegendCaught((uint8_t)lun);
    bool allSol = true;
    for (int i = 0; i < 200; i++) { q.uid = q.mintUid(); if (q.evoInfo().target != 791) allSol = false; }
    ck(allSol, "owning Lunala, every Cosmoem becomes Solgaleo");

    // An evolution owns it exactly as a catch would, and leaves the other's
    // guaranteed meeting where it was.
    Pet r;
    for (uint8_t b = 0; b < 13; b++) r.winBadge(6, b, false);
    raise(r, 790, 60);
    do r.uid = r.mintUid(); while (r.evoInfo().target != 791);
    r.evolve();
    ck(r.speciesId == 791 && r.legendCaught((uint8_t)sol) && !r.legendCaught((uint8_t)lun),
       "a Cosmoem evolved into Solgaleo owns Solgaleo, and only Solgaleo");
    int lunaGuaranteed = 0, solAgain = 0;
    for (uint32_t roll = 0; roll < 1000; roll++) {
      int l = wildLegendFor(wildLegendHome(lun), r, roll);
      if (l == lun) lunaGuaranteed++;
      if (l == sol) solAgain++;
    }
    ck(lunaGuaranteed == 1000 && solAgain == 0,
       "the next Explore there is Lunala's guaranteed meeting; Solgaleo never comes back");
  }

  // ---- Applin's choice, through the dialog ------------------------------------------------
  {
    raise(pet, 840, 31);
    int16_t opts[4];
    ck(pet.evoChoices(opts) == 3 && pet.canEvolveNow(), "a level 31 Applin offers three branches");
    choiceKind = 0;
    tapEvolve();
    ck(choiceKind == 4, "EVOLVE opens the branch choice");
    render();
    int t, b;
    evoPickRect(3, &t, &b);           // KEEP
    onTap(233, (int16_t)((t + b) / 2));
    ck(pet.speciesId == 840 && choiceKind == 0, "KEEP keeps it an Applin");
    raise(pet, 840, 33);
    tapEvolve();
    int pick = -1;
    pet.evoChoices(opts);
    for (int i = 0; i < 3; i++) if (opts[i] == 842) pick = i;
    evoPickRect(pick, &t, &b);
    onTap(233, (int16_t)((t + b) / 2));
    ck(pet.speciesId == 842, "tapping APPLETUN makes an Appletun");
    raise(pet, 935, 31);
    ck(pet.evoChoices(opts) == 2, "Charcadet offers two");
    ck(!pet.evolveChoice(842) && pet.speciesId == 935, "and refuses a branch that is not its own");
    raise(pet, 1011, 35);
    ck(!pet.canEvolveNow() && pet.evoInfo().levelsLeft == 5, "a Dipplin waits for 40 to become Hydrapple");
  }

  // ---- Nincada's Shedinja ------------------------------------------------------------------
  {
    for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = PartyMon();
    party.save();
    for (ItemKey k = 1; k < ITEM_COUNT; k++) while (bag.count(k)) bag.consume(k, bag.count(k));
    raise(pet, 290, 35);   // past the 31 the Applin KEEP above declined at
    bag.add(IT_POKEBALL, 2);
    uint8_t before = ownedCount(292);
    int c1t, c1b, c2t, c2b;
    uiConfirmRects(&c1t, &c1b, &c2t, &c2b);
    choiceKind = 0;
    tapEvolve();
    ck(choiceKind == 1, "(Nincada's ordinary evolve dialog)");
    onTap(233, (int16_t)((c1t + c1b) / 2));   // the dialog's EVOLVE
    ck(pet.speciesId == 291, "Nincada becomes Ninjask");
    ck(ownedCount(292) == before + 1 && bag.count(IT_POKEBALL) == 1,
       "and with a Poke Ball and room, a Shedinja appears and one ball is spent");
    raise(pet, 290, 35);   // past the 31 the Applin KEEP above declined at
    while (bag.count(IT_POKEBALL)) bag.consume(IT_POKEBALL, bag.count(IT_POKEBALL));
    before = ownedCount(292);
    tapEvolve();
    onTap(233, (int16_t)((c1t + c1b) / 2));
    ck(pet.speciesId == 291 && ownedCount(292) == before, "with no ball, only the Ninjask");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
