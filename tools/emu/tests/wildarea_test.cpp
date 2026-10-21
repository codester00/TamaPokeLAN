// Explore's areas (v3.39): the generated tables in wild_areas.h and the rules
// wild.cpp applies to them -- gates, time bands, weighted picks, pack filters,
// level bands and floors. docs/explore-plan.md is the design.
//
// The rules are asked through the real functions and a real Pet, and the
// expectations come from the firmware's own tables (WILD_SLOTS, TRAINER_SETS)
// rather than from numbers typed here, so an edit to the data cannot pass by
// matching a stale copy. Every "never" has a companion proving the mechanism
// actually engaged -- CLAUDE.md § 3, the vacuous negative.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "wild.h"
#include "noart.h"
#include "trainers.h"
#include "wild_areas.h"   // this test's own copy of the tables, to compare against
#include <cstdio>
#include <cstring>
uint32_t g_seed=5; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static int areaNamed(const char *n){
  for (int i=0;i<WILD_AREA_COUNT;i++) if(!strcmp(WILD_AREAS[i].name,n)) return i;
  return -1;
}

int main(){
  // ---- the tables ----------------------------------------------------------
  bool sums=true, gates=true, names=true, dexes=true, bands=true, slices=true;
  for (int i=0;i<WILD_AREA_COUNT;i++){
    const WildArea &a=WILD_AREAS[i];
    // Each time band is its own distribution: a slot counts in every band its
    // `when` bits name, and each band must add up to the total on its own.
    for (uint8_t b=WILD_WHEN_MORN;b<=WILD_WHEN_NITE;b<<=1){
      uint32_t t=0;
      for (int k=0;k<a.count;k++) if (WILD_SLOTS[a.first+k].when & b) t+=WILD_SLOTS[a.first+k].weight;
      if (t!=WILD_WEIGHT_TOTAL) sums=false;
    }
    for (int k=0;k<a.count;k++){
      const WildSlot &s=WILD_SLOTS[a.first+k];
      if (s.dex<1 || s.dex>DEX_COUNT || !s.weight || !s.when || (s.when & ~WILD_WHEN_ANY)) dexes=false;
    }
    if (!(a.gate<=TRAINER_GYMS || a.gate==WILD_GATE_CHAMPION)) gates=false;
    size_t n=strlen(a.name);
    if (!n || n>16) names=false;
    for (size_t c=0;c<n;c++) if ((unsigned char)a.name[c]>126 || (unsigned char)a.name[c]<32) names=false;
    if (!a.lvMin || a.lvMin>a.lvMax || a.lvMax>MAX_LEVEL) bands=false;
    if (a.region>=WILD_REGIONS || i<WILD_REGION_AREAS[a.region][0] ||
        i>=WILD_REGION_AREAS[a.region][0]+WILD_REGION_AREAS[a.region][1]) slices=false;
  }
  ck(WILD_AREA_COUNT>0 && wildRegionHasAreas(0), "Kanto has areas");
  ck(sums, "every area's weights add up to WILD_WEIGHT_TOTAL in each time band");
  ck(wildRegionHasAreas(1), "Johto has areas");
  ck(dexes, "every slot is a real species with a weight");
  ck(gates, "every gate is a badge count or the champion");
  ck(names, "every area name is 1-16 printable ASCII characters");
  ck(bands, "every band is a real level range");
  ck(slices, "every area sits inside its own region's slice");
  ck(!wildRegionHasAreas(REGION_ALL) && !wildRegionHasAreas(200),
     "ALL and nonsense regions have no areas");

  // The bands must be the ladder's, read from the firmware's own TRAINER_SETS:
  // gate 0 is 1..cap0-2, gate g is cap_g-10..cap_g-2, the champion's areas
  // cap12-5..cap12+5, caps being running maxima (docs/explore-plan.md § 6.1).
  // Edit a ladder without re-running tools/gen_wild.py and this fails.
  bool ladder=true;
  for (int i=0;i<WILD_AREA_COUNT;i++){
    const WildArea &a=WILD_AREAS[i];
    if (a.region>=GYM_REGIONS) { ladder=false; continue; }
    uint8_t cap[TRAINER_COUNT], run=0;
    for (int t=0;t<TRAINER_COUNT;t++){
      const Trainer &tr=TRAINER_SETS[a.region].list[t];
      for (int k=0;k<tr.count;k++) if (tr.team[k].level>run) run=tr.team[k].level;
      cap[t]=run;
    }
    int lo,hi;
    if (a.gate==WILD_GATE_CHAMPION){ lo=cap[12]-5; hi=cap[12]+5; if(hi>MAX_LEVEL) hi=MAX_LEVEL; }
    else if (a.gate==0){ lo=1; hi=cap[0]-2; }
    else { lo=cap[a.gate]-10; hi=cap[a.gate]-2; }
    if (a.lvMin!=lo || a.lvMax!=hi){
      printf("      %s: L%u-%u, the ladder says L%d-%d\n", a.name, a.lvMin, a.lvMax, lo, hi);
      ladder=false;
    }
  }
  ck(ladder, "every band is the one the gym ladder in trainers.h gives");

  // ---- gates ---------------------------------------------------------------
  Pet p;   // a fresh player: no badges anywhere
  int r1=areaNamed("ROUTE 1"), moon=areaNamed("MT. MOON"), vr=areaNamed("VICTORY ROAD"),
      cave=areaNamed("CERULEAN CAVE"), r22=areaNamed("ROUTE 22"), tanoby=areaNamed("TANOBY RUINS");
  ck(r1>=0 && moon>=0 && vr>=0 && cave>=0 && r22>=0 && tanoby>=0, "the areas this test uses exist");
  ck(wildAreaUnlocked(r1,p), "a gate-0 area is open from the start");
  ck(!wildAreaUnlocked(moon,p), "a gate-1 area is locked with no badges");
  p.winBadge(0,0,false);
  ck(wildAreaUnlocked(moon,p), "and opens with the Boulder Badge");
  ck(!wildAreaUnlocked(vr,p), "while an all-badges area stays shut");
  Pet h;
  h.winBadge(0,0,true);
  ck(wildAreaUnlocked(moon,h), "a HARD badge opens it too (owner's call: both count)");
  ck(!wildAreaUnlocked(moon,Pet()), "and nothing else does");
  Pet c;
  for (uint8_t i=0;i<TRAINER_COUNT-1;i++) c.winBadge(0,i,false);
  ck(!wildAreaUnlocked(cave,c), "the Elite 4 beaten is not the champion beaten");
  c.winBadge(0,TRAINER_COUNT-1,false);
  ck(wildAreaUnlocked(cave,c), "the champion opens the postgame");
  Pet j;
  j.winBadge(1,0,false);   // a JOHTO badge
  ck(!wildAreaUnlocked(moon,j), "another region's badge does not open Kanto");

  // ---- time of day ---------------------------------------------------------
  ck(wildTimeBand(-1)==WILD_WHEN_DAY, "a clock that was never set counts as day");
  // A creature of the night (Crystal has them: Hoothoot on Route 29) is met at
  // night and never by day. Found from the data, so the check cannot pass on
  // an area that happens to have no night-only slot.
  {
    int na=-1; int16_t nd=0;
    for (int i=0;i<WILD_AREA_COUNT && na<0;i++){
      const WildArea &a=WILD_AREAS[i];
      for (int k=0;k<a.count && na<0;k++){
        const WildSlot &s=WILD_SLOTS[a.first+k];
        if (s.when!=WILD_WHEN_NITE) continue;
        bool byDay=false;
        for (int j=0;j<a.count;j++)
          if (WILD_SLOTS[a.first+j].dex==s.dex && (WILD_SLOTS[a.first+j].when & WILD_WHEN_DAY)) byDay=true;
        if (!byDay){ na=i; nd=s.dex; }
      }
    }
    ck(na>=0, "some area has a creature only met at night");
    if (na>=0){
      bool day=false, night=false;
      for (uint32_t r=0;r<WILD_WEIGHT_TOTAL;r++){
        if (wildAreaPick((uint16_t)na,WILD_WHEN_DAY,r)==nd) day=true;
        if (wildAreaPick((uint16_t)na,WILD_WHEN_NITE,r)==nd) night=true;
      }
      printf("      %s: %s\n", WILD_AREAS[na].name, DEX_TBL[nd].name);
      ck(night && !day, "it is met at night and never by day");
    }
  }
  ck(wildTimeBand(3)==WILD_WHEN_NITE && wildTimeBand(4)==WILD_WHEN_MORN &&
     wildTimeBand(9)==WILD_WHEN_MORN && wildTimeBand(10)==WILD_WHEN_DAY &&
     wildTimeBand(17)==WILD_WHEN_DAY && wildTimeBand(18)==WILD_WHEN_NITE,
     "morning 04-10, day 10-18, night 18-04, at every edge");

  // ---- weighted picks ------------------------------------------------------
  // Every roll 0..total-1 once: the counts must equal the weights EXACTLY, so
  // the pick is the game's distribution and not merely "some species".
  gRegionArt=0xFFFF;
  {
    const WildArea &a=WILD_AREAS[r22];
    uint32_t total=0; for(int k=0;k<a.count;k++) total+=WILD_SLOTS[a.first+k].weight;
    uint32_t got[64]={0};
    bool inArea=true;
    for (uint32_t roll=0;roll<total;roll++){
      int16_t d=wildAreaPick((uint16_t)r22,WILD_WHEN_DAY,roll);
      int k=0; while(k<a.count && WILD_SLOTS[a.first+k].dex!=d) k++;
      if (k==a.count) { inArea=false; continue; }
      got[k]++;
    }
    bool exact=true;
    for (int k=0;k<a.count;k++) if (got[k]!=WILD_SLOTS[a.first+k].weight) exact=false;
    ck(inArea, "a pick only ever returns a species of that area");
    ck(exact, "and each species comes up exactly as often as its weight");
  }

  // A missing pack: Tanoby Ruins is Unown (Johto's) and little else.
  {
    bool johtoSeen=false;
    for (uint32_t roll=0;roll<WILD_WEIGHT_TOTAL;roll+=7){
      int16_t d=wildAreaPick((uint16_t)tanoby,WILD_WHEN_DAY,roll);
      if (d>=REGIONS[1].lo && d<=REGIONS[1].hi) johtoSeen=true;
    }
    ck(johtoSeen, "with every pack, a Kanto area can hold a Johto species (the case below is real)");
    gRegionArt=1u<<0;   // Kanto only
    bool leak=false, any=false;
    for (int i=WILD_REGION_AREAS[0][0]; i<WILD_REGION_AREAS[0][0]+WILD_REGION_AREAS[0][1]; i++)
      for (uint32_t roll=0;roll<WILD_WEIGHT_TOTAL;roll+=13){
        int16_t d=wildAreaPick((uint16_t)i,WILD_WHEN_DAY,roll);
        if (d) any=true;
        if (d && regionOfDex(d)!=0) leak=true;
      }
    ck(any && !leak, "with only Kanto's pack, no other region's species is ever met");
    gRegionArt=0;
    ck(!wildAreaHasLive((uint16_t)r1,WILD_WHEN_DAY) && wildAreaPick((uint16_t)r1,WILD_WHEN_DAY,1)==0,
       "with no pack at all, an area has nothing live and picks nothing");
    gRegionArt=0xFFFF;
    ck(wildAreaHasLive((uint16_t)r1,WILD_WHEN_DAY), "and comes back with the packs");
  }
  {
    bool art=true;
    for (int i=0;i<WILD_AREA_COUNT;i++)
      for (uint32_t roll=0;roll<WILD_WEIGHT_TOTAL;roll+=97){
        int16_t d=wildAreaPick((uint16_t)i,WILD_WHEN_DAY,roll);
        if (d && !speciesHasArt(d)) art=false;
      }
    ck(art, "no area ever offers a species with no art");
  }

  // ---- levels --------------------------------------------------------------
  {
    const WildArea &a=WILD_AREAS[r1];
    uint8_t span=(uint8_t)(a.lvMax-a.lvMin+1);
    int16_t base=WILD_SLOTS[a.first].dex;   // Route 1 is Pidgey/Rattata: no floor
    ck(wildAreaLevel((uint16_t)r1,base,0)==a.lvMin, "roll 0 is the bottom of the band");
    ck(wildAreaLevel((uint16_t)r1,base,span-1)==a.lvMax, "the last roll is the top of it");
    ck(wildAreaLevel((uint16_t)r1,base,span)==a.lvMin, "and it wraps rather than overshooting");
  }
  {
    // An evolved form whose floor is above the band stays in its area and
    // appears AT its floor (owner's call). Found from the data, not named.
    int fa=-1, fk=-1;
    for (int i=0;i<WILD_AREA_COUNT && fa<0;i++)
      for (int k=0;k<WILD_AREAS[i].count;k++)
        if (WILD_SLOTS[WILD_AREAS[i].first+k].floor > WILD_AREAS[i].lvMax){ fa=i; fk=k; break; }
    ck(fa>=0, "some area holds an evolved form above its band (Route 22's Gyarados)");
    if (fa>=0){
      const WildSlot &s=WILD_SLOTS[WILD_AREAS[fa].first+fk];
      bool atFloor=true;
      for (uint32_t roll=0;roll<64;roll++) if (wildAreaLevel((uint16_t)fa,s.dex,roll)!=s.floor) atFloor=false;
      ck(atFloor, "and it always appears at its floor");
    }
    // and a floor INSIDE the band only lifts the rolls below it
    int ia=-1, ik=-1;
    for (int i=0;i<WILD_AREA_COUNT && ia<0;i++)
      for (int k=0;k<WILD_AREAS[i].count;k++){
        uint8_t f=WILD_SLOTS[WILD_AREAS[i].first+k].floor;
        if (f>WILD_AREAS[i].lvMin && f<WILD_AREAS[i].lvMax){ ia=i; ik=k; break; }
      }
    ck(ia>=0, "some area has a floor inside its band");
    if (ia>=0){
      const WildArea &a=WILD_AREAS[ia]; const WildSlot &s=WILD_SLOTS[a.first+ik];
      bool ok=true, top=false;
      for (uint32_t roll=0;roll<=(uint32_t)(a.lvMax-a.lvMin);roll++){
        uint8_t l=wildAreaLevel((uint16_t)ia,s.dex,roll);
        if (l<s.floor || l>a.lvMax) ok=false;
        if (l==a.lvMax) top=true;
      }
      ck(ok && top, "rolls below the floor are lifted to it, the rest stay as rolled");
    }
  }

  printf(bad?"FAILED %d\n":"OK\n", bad);
  return bad?1:0;
}
