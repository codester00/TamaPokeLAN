// EEVEE's branch, which is the one evolution the DexEntry table cannot express.
//
// evolvesTo is a single field, so only VAPOREON (134) is in the data; the other
// seven live in the generated EEVEE_EVOS list. That list is ALSO what marks all
// eight evolution-only, so "can be reached" and "cannot hatch" come from one
// source and cannot drift -- which they had, badly: the branch was hardcoded
// 134..136 for six generations while ESPEON, UMBREON, LEAFEON, GLACEON and
// SYLVEON sat in the dex as rare BASE forms, hatching straight out of eggs.
//
// The part that must not regress is the filter. A branch is only offered if the
// species has art at all AND its region's sprite pack is on the card. Without
// that, a Kanto-only player's Eevee could turn into Sylveon and be a creature
// that can only ever draw as a dex number -- permanently, since evolution is
// one-way.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "dex.h"
#include "noart.h"
#include "moves.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 99;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0, g_touchY=0; volatile bool g_touchDown=false;
bool wasPressed=false;
uint32_t millis(){ return 0; }
void FakeESP::restart(){ exit(0); }
int FakeSerial::available(){ return 0; }
String FakeSerial::readStringUntil(char){ return String(""); }
void sfxPlay(uint8_t){}

static int bad=0;
static void ck(bool ok,const char*w){ printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++; }

int main(){
  ck(EEVEE_EVO_COUNT == 8, "all eight Eeveelutions are in the generated list");

  // every branch is evolution-ONLY, so none of them can hatch
  {
    int hatchable=0;
    for (int i=0;i<EEVEE_EVO_COUNT;i++)
      if (DEX_TBL[EEVEE_EVOS[i]].rarity != R_EVO) {
        printf("      %s (%d) rarity %u -- can hatch\n",
               DEX_TBL[EEVEE_EVOS[i]].name, EEVEE_EVOS[i], DEX_TBL[EEVEE_EVOS[i]].rarity);
        hatchable++;
      }
    ck(hatchable==0, "and none of them hatches from an egg any more");
  }

  Pet p; p.begin();
  gRegionArt = 0xFFFF;                       // every pack present

  // with everything available, all eight must be offered
  {
    int16_t opts[EEVEE_EVO_COUNT];
    uint8_t n = p.eeveeOptions(opts);
    ck(n == EEVEE_EVO_COUNT, "with every pack on the card, all eight are offered");
  }

  // THE FILTER: only Kanto installed -> only the three Kanto branches
  {
    gRegionArt = 1u << 0;                    // KANTO only
    int16_t opts[EEVEE_EVO_COUNT];
    uint8_t n = p.eeveeOptions(opts);
    bool onlyKanto = true;
    for (uint8_t i=0;i<n;i++) if (opts[i] > 151) onlyKanto = false;
    ck(n == 3 && onlyKanto,
       "with only the Kanto pack, only Vaporeon/Jolteon/Flareon are offered");
  }

  // v3.81: Eevee follows the stones, and its bond branches wait for bond 40
  // and two days. Every one is still filtered by art and pack.
  auto eevee = [](Pet &q, uint32_t hours, uint8_t bond, uint32_t epoch) {
    q.begin(); q.dbgHatchAs(DEX_EEVEE, false);
    q.ageMinutes = hours * 60;
    q.bond = bond;
    q.lastSeenEpoch = epoch;
    q.fullness = q.joy = q.energy = q.hygiene = 100;
  };
  const uint32_t NOON = 20000UL * 86400 + 12 * 3600, MIDNIGHT = 20000UL * 86400 + 1 * 3600;
  {
    gRegionArt = 1u << 0;                    // KANTO only
    Pet q;
    eevee(q, 60, 0, NOON);
    ck(q.evoInfo().status == Pet::EVO_NEEDS_ITEM && !q.canEvolveNow(),
       "Eevee has no level route: at 60 hours it waits for a stone or its bond");
    const int16_t want[3] = { 134, 135, 136 };
    for (int s = 0; s < 3; s++) {            // water, thunder, fire
      eevee(q, 20, 0, NOON);
      uint8_t stone = (uint8_t)(s == 0 ? 1 : s == 1 ? 2 : 0);
      ck(q.useStone(stone) && q.speciesId == want[s], "a Kanto stone evolves it into its Kanto branch");
    }
    eevee(q, 20, 0, NOON);
    ck(!q.useStone(3) && q.speciesId == DEX_EEVEE,
       "a Leaf Stone does nothing with only the Kanto pack: Leafeon cannot be drawn");
    eevee(q, 60, 60, NOON);
    ck(!q.eeveeBondTarget() && !q.canEvolveNow(), "nor does its bond reach Espeon without Johto's pack");
  }
  {
    gRegionArt = 0xFFFF;
    Pet q;
    eevee(q, 20, 0, NOON);
    ck(q.useStone(3) && q.speciesId == 470, "with every pack the Leaf Stone reaches Leafeon (so the check bites)");
    eevee(q, 60, 40, NOON);
    ck(q.canEvolveNow() && q.evoInfo().target == 196, "bond 40 and two days old, by day: Espeon");
    q.evolve();
    ck(q.speciesId == 196, "and it evolves into it");
    eevee(q, 60, 40, MIDNIGHT);
    ck(q.evoInfo().target == 197, "by night: Umbreon");
    eevee(q, 60, 40, 0);
    ck(q.evoInfo().target == 196, "with no clock it counts as day");
    eevee(q, 47, 100, NOON);
    ck(!q.eeveeBondTarget(), "47 hours old is not enough, whatever the bond");
    eevee(q, 60, 39, NOON);
    ck(!q.eeveeBondTarget(), "bond 39 is not enough");
    eevee(q, 60, 40, NOON);
    int fairy = 0;
    for (int m = 1; m < MOVE_COUNT && !fairy; m++) if (MOVE_TBL[m].type == T_FAIRY) fairy = m;
    q.moves[0] = (uint8_t)fairy;
    ck(fairy && q.evoInfo().target == 700, "knowing a Fairy move, it becomes Sylveon");
  }

  // no branch may be a species with no art, whatever the packs say
  {
    gRegionArt = 0xFFFF;
    int16_t opts[EEVEE_EVO_COUNT];
    uint8_t n = p.eeveeOptions(opts);
    int artless=0;
    for (uint8_t i=0;i<n;i++) if (!speciesHasArt(opts[i])) artless++;
    ck(artless==0, "no offered branch is a species with no sprite anywhere");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
