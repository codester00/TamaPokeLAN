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
// Region by region: Alola has its own ROUTE 1.
static int areaIn(uint8_t region, const char *n){
  for (uint8_t k=0;k<wildAreaCount(region);k++)
    if (!strcmp(wildAreaName(wildAreaFirst(region)+k),n)) return wildAreaFirst(region)+k;
  return -1;
}
// Over every roll: how often `dex` is picked in `area` by day.
static int picks(int area, const Pet &p, int16_t dex){
  int n=0;
  for (uint32_t r=0;r<WILD_WEIGHT_TOTAL;r++) if (wildAreaPick((uint16_t)area,WILD_WHEN_DAY,p,r)==dex) n++;
  return n;
}

int main(){
  // Every gate open in both regions: the picks below are about the tables, the
  // pack and the clock, not about progress (the best rod's gate has its own
  // section).
  Pet all;
  for (uint8_t r=0;r<WILD_REGIONS;r++) for (uint8_t t=0;t<TRAINER_COUNT;t++) all.winBadge(r,t,false);

  // ---- the tables ----------------------------------------------------------
  bool sums=true, gates=true, names=true, dexes=true, bands=true, slices=true;
  for (int i=0;i<WILD_AREA_COUNT;i++){
    const WildArea &a=WILD_AREAS[i];
    // Each time band is its own distribution: a slot counts in every band its
    // `when` bits name, and each band must add up to the total on its own --
    // in each weather the area has, and in the all-weather average (v3.48).
    for (uint8_t b=WILD_WHEN_MORN;b<=WILD_WHEN_NITE;b<<=1)
      for (int c=-1;c<WILD_WX_COUNT;c++){
        if (c>=0 && !((a.wxMask>>c)&1)) continue;
        uint16_t bit = c<0 ? WILD_WX_AVG : (uint16_t)(1u<<c);
        uint32_t t=0;
        for (int k=0;k<a.count;k++){
          const WildSlot &sl=WILD_SLOTS[a.first+k];
          if ((sl.when & b) && (sl.wx & bit)) t+=sl.weight;
        }
        if (t!=WILD_WEIGHT_TOTAL) sums=false;
      }
    for (int k=0;k<a.count;k++){
      const WildSlot &s=WILD_SLOTS[a.first+k];
      if (s.dex<1 || s.dex>DEX_COUNT || !s.weight || !s.when || (s.when & ~WILD_WHEN_ANY)) dexes=false;
      // A slot's own gate (the best rod's) is a real gate, past its area's.
      if (s.gate && (s.gate<=a.gate || !(s.gate<=TRAINER_GYMS || s.gate==WILD_GATE_CHAMPION))) gates=false;
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
  ck(sums, "every area's weights add up to WILD_WEIGHT_TOTAL in each time band and weather");
  ck(wildRegionHasAreas(1), "Johto has areas");
  ck(wildRegionHasAreas(2), "Hoenn has areas");
  ck(wildRegionHasAreas(3), "Sinnoh has areas");
  ck(wildRegionHasAreas(4), "Unova has areas");
  ck(wildRegionHasAreas(5), "Kalos has areas");
  ck(wildRegionHasAreas(6), "Alola has areas");
  ck(wildRegionHasAreas(7), "Galar has areas");
  {
    // A locked row says which badge it needs; a region with areas whose badge
    // names were never filled in would name a leader instead.
    bool named=true;
    for (uint8_t r=0;r<WILD_REGIONS;r++)
      if (wildRegionHasAreas(r))
        for (uint8_t g=0;g<TRAINER_GYMS;g++) if (!BADGE_NAMES[r][g]) named=false;
    ck(named, "every region with areas names all eight of its badges");
  }
  ck(dexes, "every slot is a real species with a weight");
  ck(gates, "every gate is a badge count or the champion, and a slot's is past its area's");
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
        if (wildAreaPick((uint16_t)na,WILD_WHEN_DAY,all,r)==nd) day=true;
        if (wildAreaPick((uint16_t)na,WILD_WHEN_NITE,all,r)==nd) night=true;
      }
      printf("      %s: %s\n", WILD_AREAS[na].name, DEX_TBL[nd].name);
      ck(night && !day, "it is met at night and never by day");
    }
  }
  {
    // Platinum's grass swaps two slots by time of day too (v3.43): a Sinnoh
    // creature met in the morning or at night but never by day, found from
    // the data, so the Platinum loader is checked on its own and not by
    // Crystal's tables standing in for it.
    int sa=-1; int16_t sd=0;
    for (int i=WILD_REGION_AREAS[3][0]; i<WILD_REGION_AREAS[3][0]+WILD_REGION_AREAS[3][1] && sa<0; i++){
      const WildArea &a=WILD_AREAS[i];
      for (int k=0;k<a.count && sa<0;k++){
        const WildSlot &s=WILD_SLOTS[a.first+k];
        if (s.gate || (s.when & WILD_WHEN_DAY)) continue;
        bool byDay=false;
        for (int j=0;j<a.count;j++)
          if (WILD_SLOTS[a.first+j].dex==s.dex && (WILD_SLOTS[a.first+j].when & WILD_WHEN_DAY)) byDay=true;
        if (!byDay){ sa=i; sd=s.dex; }
      }
    }
    ck(sa>=0, "Sinnoh has a creature never met by day");
    if (sa>=0){
      bool day=false, other=false;
      for (uint32_t r=0;r<WILD_WEIGHT_TOTAL;r++){
        if (wildAreaPick((uint16_t)sa,WILD_WHEN_DAY,all,r)==sd) day=true;
        if (wildAreaPick((uint16_t)sa,WILD_WHEN_NITE,all,r)==sd ||
            wildAreaPick((uint16_t)sa,WILD_WHEN_MORN,all,r)==sd) other=true;
      }
      printf("      %s: %s\n", WILD_AREAS[sa].name, DEX_TBL[sd].name);
      ck(other && !day, "it comes in its own band and never by day");
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
      int16_t d=wildAreaPick((uint16_t)r22,WILD_WHEN_DAY,all,roll);
      int k=0; while(k<a.count && WILD_SLOTS[a.first+k].dex!=d) k++;
      if (k==a.count) { inArea=false; continue; }
      got[k]++;
    }
    // By species: one may hold an open slot AND a best-rod slot (Psyduck surfs
    // and bites a line), and a pick names the species, not the slot.
    bool exact=true;
    for (int k=0;k<a.count;k++){
      uint32_t want=0, have=0;
      for (int j=0;j<a.count;j++) if (WILD_SLOTS[a.first+j].dex==WILD_SLOTS[a.first+k].dex){
        want+=WILD_SLOTS[a.first+j].weight; have+=got[j];
      }
      if (have!=want) exact=false;
    }
    ck(inArea, "a pick only ever returns a species of that area");
    ck(exact, "and each species comes up exactly as often as its weight");
  }

  // A missing pack: Tanoby Ruins is Unown (Johto's) and little else.
  {
    bool johtoSeen=false;
    for (uint32_t roll=0;roll<WILD_WEIGHT_TOTAL;roll+=7){
      int16_t d=wildAreaPick((uint16_t)tanoby,WILD_WHEN_DAY,all,roll);
      if (d>=REGIONS[1].lo && d<=REGIONS[1].hi) johtoSeen=true;
    }
    ck(johtoSeen, "with every pack, a Kanto area can hold a Johto species (the case below is real)");
    gRegionArt=1u<<0;   // Kanto only
    bool leak=false, any=false;
    for (int i=WILD_REGION_AREAS[0][0]; i<WILD_REGION_AREAS[0][0]+WILD_REGION_AREAS[0][1]; i++)
      for (uint32_t roll=0;roll<WILD_WEIGHT_TOTAL;roll+=13){
        int16_t d=wildAreaPick((uint16_t)i,WILD_WHEN_DAY,all,roll);
        if (d) any=true;
        if (d && regionOfDex(d)!=0) leak=true;
      }
    ck(any && !leak, "with only Kanto's pack, no other region's species is ever met");
    gRegionArt=0;
    ck(!wildAreaHasLive((uint16_t)r1,WILD_WHEN_DAY,all) && wildAreaPick((uint16_t)r1,WILD_WHEN_DAY,all,1)==0,
       "with no pack at all, an area has nothing live and picks nothing");
    gRegionArt=0xFFFF;
    ck(wildAreaHasLive((uint16_t)r1,WILD_WHEN_DAY,all), "and comes back with the packs");
  }
  {
    bool art=true;
    for (int i=0;i<WILD_AREA_COUNT;i++)
      for (uint32_t roll=0;roll<WILD_WEIGHT_TOTAL;roll+=97){
        int16_t d=wildAreaPick((uint16_t)i,WILD_WHEN_DAY,all,roll);
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

  // ---- the best rod's hidden gate --------------------------------------------
  // The game's Super Rod catch waits for the badge the rod is handed out at,
  // through the same gate rule as an area. Checked in every area that has one,
  // from a Pet holding exactly the badges BEFORE the rod's gate, then the one
  // that opens it. Found from the data, not named.
  {
    int gatedAreas=0, rodOnly=0;
    bool neverEarly=true, metAfter=true, exactEarly=true, allLive=true;
    for (int i=0;i<WILD_AREA_COUNT;i++){
      const WildArea &a=WILD_AREAS[i];
      // Every area, opened with exactly its own gate, has something live in
      // every band: the rod gate is SECRET, so it must never read NEEDS PACK.
      Pet opened;
      for (uint8_t t=0;t<(a.gate==WILD_GATE_CHAMPION?TRAINER_COUNT:a.gate);t++) opened.winBadge(a.region,t,false);
      for (uint8_t b=WILD_WHEN_MORN;b<=WILD_WHEN_NITE;b<<=1)
        if (!wildAreaHasLive((uint16_t)i,b,opened)) allLive=false;
      uint8_t rg=0;
      for (int k=0;k<a.count;k++) if (WILD_SLOTS[a.first+k].gate) rg=WILD_SLOTS[a.first+k].gate;
      if (!rg) continue;
      gatedAreas++;
      Pet before, after;
      for (uint8_t t=0;t+1<rg;t++){ before.winBadge(a.region,t,false); after.winBadge(a.region,t,false); }
      after.winBadge(a.region,rg-1,true);   // on hard: either difficulty opens it
      for (uint8_t b=WILD_WHEN_MORN;b<=WILD_WHEN_NITE;b<<=1){
        // What each species should come up as before the rod: its open slots'
        // weights alone, renormalised over the open total. A species with no
        // art is never offered (Unova onward has some), so it counts for none.
        uint32_t open=0;
        for (int k=0;k<a.count;k++){
          const WildSlot &s=WILD_SLOTS[a.first+k];
          // These Pets have no clock, so a zone with weather uses its average.
          if ((s.when&b) && (s.wx&WILD_WX_AVG) && !s.gate && speciesHasArt(s.dex)) open+=s.weight;
        }
        for (int k=0;k<a.count;k++){
          const WildSlot &s=WILD_SLOTS[a.first+k];
          if (!(s.when&b) || !(s.wx&WILD_WX_AVG)) continue;
          uint32_t want=0; bool onlyRod=true;
          for (int j=0;j<a.count;j++){
            const WildSlot &o=WILD_SLOTS[a.first+j];
            if (o.dex!=s.dex || !(o.when&b) || !(o.wx&WILD_WX_AVG)) continue;
            if (!o.gate){ if (speciesHasArt(o.dex)) want+=o.weight; onlyRod=false; }
          }
          uint32_t got=0, late=0;
          for (uint32_t r=0;r<open;r++) if (wildAreaPick((uint16_t)i,b,before,r)==s.dex) got++;
          if (got!=want) exactEarly=false;
          // A gated creature with no art (Galar's Frillish) can never be met:
          // it counts for neither side.
          if (onlyRod && s.gate && speciesHasArt(s.dex)){
            rodOnly++;
            if (got) neverEarly=false;
            for (uint32_t r=0;r<WILD_WEIGHT_TOTAL;r++) if (wildAreaPick((uint16_t)i,b,after,r)==s.dex) late++;
            if (!late) metAfter=false;
          }
        }
      }
    }
    printf("      %d areas hold a best-rod catch back, %d rod-only slots\n", gatedAreas, rodOnly);
    ck(gatedAreas>0 && rodOnly>0, "some areas hold a fishing catch back (Route 22's Gyarados)");
    ck(neverEarly, "a creature only the best rod finds never appears before the rod's badge");
    ck(metAfter, "and every one of them appears once that badge is won, on either difficulty");
    ck(exactEarly, "before the rod, the rest keep the game's rates, renormalised exactly");
    ck(allLive, "and no area is ever left with nothing live by it (the gate is secret)");
  }

  // ---- NO VALID SPRITES, the catch-all (v3.45) -------------------------------
  // An area whose open creatures cannot be drawn greys out and says so, and
  // lights up by itself when the art arrives or a gate opens. The art check is
  // swapped through wildHasArt to stand in for art not ingested yet. Found from
  // the data: an area whose rod holds a creature its open slots do not.
  {
    int da=-1; uint8_t rg=0;
    for (int i=0;i<WILD_AREA_COUNT && da<0;i++){
      const WildArea &a=WILD_AREAS[i];
      for (int k=0;k<a.count && da<0;k++){
        const WildSlot &s=WILD_SLOTS[a.first+k];
        if (!s.gate || !speciesHasArt(s.dex) || !(s.when & WILD_WHEN_DAY)) continue;
        bool open=false;
        for (int j=0;j<a.count;j++) if (WILD_SLOTS[a.first+j].dex==s.dex && !WILD_SLOTS[a.first+j].gate) open=true;
        if (!open){ da=i; rg=s.gate; }
      }
    }
    ck(da>=0, "some area's rod holds a creature its open slots do not");
    if (da>=0){
      static int16_t openDex[64]; static int nOpen; nOpen=0;
      const WildArea &a=WILD_AREAS[da];
      for (int k=0;k<a.count;k++) if (!WILD_SLOTS[a.first+k].gate && nOpen<64) openDex[nOpen++]=WILD_SLOTS[a.first+k].dex;
      wildHasArt=[](int16_t d){ for(int i=0;i<nOpen;i++) if(openDex[i]==d) return false; return speciesHasArt(d); };
      Pet before, after;
      for (uint8_t t=0;t+1<rg;t++){ before.winBadge(a.region,t,false); after.winBadge(a.region,t,false); }
      after.winBadge(a.region,rg-1,false);
      printf("      %s: its open creatures' art withheld\n", a.name);
      ck(wildAreaState((uint16_t)da,WILD_WHEN_DAY,before)==WILD_AREA_NO_SPRITES &&
         !wildAreaHasLive((uint16_t)da,WILD_WHEN_DAY,before) &&
         wildAreaPick((uint16_t)da,WILD_WHEN_DAY,before,1)==0,
         "with no drawable creature behind open gates it reads NO VALID SPRITES and offers nothing");
      bool rodOnly=true;
      for (uint32_t r=0;r<WILD_WEIGHT_TOTAL;r++){
        int16_t d=wildAreaPick((uint16_t)da,WILD_WHEN_DAY,after,r);
        for (int i=0;i<nOpen;i++) if (openDex[i]==d) rodOnly=false;
        if (!d) rodOnly=false;
      }
      ck(wildAreaState((uint16_t)da,WILD_WHEN_DAY,after)==WILD_AREA_LIVE && rodOnly,
         "the rod's badge colours it up, offering only what can be drawn");
      gRegionArt=0;
      ck(wildAreaState((uint16_t)da,WILD_WHEN_DAY,after)==WILD_AREA_NEEDS_PACK,
         "and a missing pack still reads NEEDS PACK, not NO VALID SPRITES");
      gRegionArt=0xFFFF;
      wildHasArt=speciesHasArt;
      ck(wildAreaState((uint16_t)da,WILD_WHEN_DAY,before)==WILD_AREA_LIVE,
         "when the art arrives it is live again with nothing else changed");
    }
    bool same=true;
    for (int i=0;i<WILD_AREA_COUNT;i++)
      for (uint8_t b=WILD_WHEN_MORN;b<=WILD_WHEN_NITE;b<<=1)
        if (wildAreaHasLive((uint16_t)i,b,all)!=(wildAreaState((uint16_t)i,b,all)==WILD_AREA_LIVE)) same=false;
    ck(same, "wildAreaHasLive() and wildAreaState() never disagree");
  }


  // ---- Alola (v3.46, Sun/Moon from PokeAPI) ----------------------------------
  // Three things its data needs that no earlier region did, each asked of the
  // firmware's own picks.
  {
    int r2=areaIn(6,"ROUTE 2"), meadow=areaIn(6,"MELEMELE MEADOW"), brook=areaIn(6,"BROOKLET HILL");
    ck(r2>=0 && meadow>=0 && brook>=0, "Alola's Route 2, Melemele Meadow and Brooklet Hill are found");
    Pet none;
    // 'bubbling-spots' is both rustling grass and a fishing rock. Route 2's
    // Makuhita is met only in the grass, so it is open from the start; read
    // as fishing it would wait for Lana's rod.
    ck(wildAreaUnlocked((uint16_t)r2,none) && picks(r2,none,296)>0,
       "Route 2's rustling grass gives Makuhita before any trial: a phenomenon, not the rod");
    // The rod itself waits for Lana: Brooklet Hill's Magikarp is fished only.
    Pet ilima; ilima.winBadge(6,0,false);
    Pet lana=ilima; lana.winBadge(6,1,false);
    ck(wildAreaUnlocked((uint16_t)brook,ilima) && picks(brook,ilima,129)==0,
       "Brooklet Hill opens after Ilima, but its Magikarp waits for Lana's rod");
    ck(picks(brook,lana,129)>0, "and bites once her trial is passed");
    // The Ultra Beasts are rate-100 walk-ins in the data; they are homes, not
    // pool members: no roll of the meadow's pool names Buzzwole.
    ck(picks(meadow,all,794)==0, "Melemele Meadow's pool never holds Buzzwole -- it is a legendary home");
    ck(wildIsExploreLegend(794) && wildIsExploreLegend(800), "Buzzwole and Necrozma have homes");
    // PokeAPI starts every Sun/Moon SOS and fishing row at level 10, however
    // late the map. Poni Gauntlet's Dragonair is an SOS ally at 56-59 there;
    // read at 10, its floor would drop below its own evolution level.
    int gaunt=areaIn(6,"PONI GAUNTLET"), fl=-1;
    if (gaunt>=0) for (int k=0;k<WILD_AREAS[gaunt].count;k++)
      if (WILD_SLOTS[WILD_AREAS[gaunt].first+k].dex==148) fl=WILD_SLOTS[WILD_AREAS[gaunt].first+k].floor;
    ck(fl==DEX_TBL[147].evolveLevel, "Poni Gauntlet's Dragonair is floored at Dratini's evolution level, not 10");
  }


  // ---- Galar (v3.47, Sword/Shield from PokeAPI) -----------------------------
  // The Wild Area's lakes wait for the Rotom Bike's water mode (Gordie's
  // badge), hidden like the best rod. Lapras is only ever met on the water.
  {
    int west=areaIn(7,"W. LAKE AXEWELL"), rolling=areaIn(7,"ROLLING FIELDS");
    ck(west>=0 && rolling>=0, "Galar's West Lake Axewell and Rolling Fields are found");
    Pet five, six;
    for (uint8_t t=0;t<5;t++){ five.winBadge(7,t,false); six.winBadge(7,t,false); }
    six.winBadge(7,5,false);
    Pet none;
    ck(wildAreaUnlocked((uint16_t)west,none) && picks(west,five,131)==0,
       "West Lake Axewell is open from the start; its Lapras waits for the water mode");
    ck(picks(west,six,131)>0, "and is on the water once Gordie's badge is won");
    // PokeAPI labels Rolling Fields' ordinary tables with a Max Raid den
    // condition; read as written, the zone would hold only its post-game set.
    ck(wildAreaHasLive((uint16_t)rolling,WILD_WHEN_DAY,none) && picks(rolling,none,674)>0,
       "Rolling Fields holds its ordinary creatures from the start (Pancham)");
  }


  // ---- Galar's daily weather (v3.48) ------------------------------------------
  // Each Wild Area zone draws one of its game weathers a day from the board's
  // clock. Asked of the firmware's own wildAreaWeather() and picks.
  {
    int west=areaIn(7,"W. LAKE AXEWELL"), r22=areaNamed("ROUTE 22");
    const WildArea &a=WILD_AREAS[west];
    int nWx=0; for (int w=0;w<WILD_WX_COUNT;w++) nWx+=(a.wxMask>>w)&1;
    ck(nWx>1 && WILD_AREAS[r22].wxMask==0, "a Wild Area zone has several weathers; Kanto's Route 22 none");
    Pet clockless; Pet dated;
    ck(wildAreaWeather((uint16_t)west,clockless)==-1, "with no clock set there is no weather (the average holds)");
    // Every weather the zone has comes round, roughly evenly, and a day keeps one.
    int seen[WILD_WX_COUNT]={0}; bool inMask=true, steady=true;
    const uint32_t DAYS=900, base=20000u;   // days since 1970: 2024 onwards
    for (uint32_t d=0;d<DAYS;d++){
      dated.lastSeenEpoch=(base+d)*86400u+3600u;
      int8_t w=wildAreaWeather((uint16_t)west,dated);
      dated.lastSeenEpoch=(base+d)*86400u+86000u;
      if (wildAreaWeather((uint16_t)west,dated)!=w) steady=false;
      if (w<0 || !((a.wxMask>>w)&1)) { inMask=false; continue; }
      seen[w]++;
    }
    bool all=true, even=true;
    for (int w=0;w<WILD_WX_COUNT;w++) if ((a.wxMask>>w)&1){
      if (!seen[w]) all=false;
      if (seen[w] < (int)DAYS/nWx/2 || seen[w] > (int)DAYS*2/nWx) even=false;
    }
    ck(inMask, "a day's weather is always one the zone has in the game");
    ck(steady, "and it holds from morning to midnight");
    ck(all && even, "over 900 days every one of its weathers comes round, roughly evenly");
    int diffZone=0;
    int dusty=areaIn(7,"DUSTY BOWL");
    for (uint32_t d=0;d<60;d++){
      dated.lastSeenEpoch=(base+d)*86400u;
      if (wildAreaWeather((uint16_t)west,dated)!=wildAreaWeather((uint16_t)dusty,dated)) diffZone++;
    }
    ck(diffZone>10, "zones keep their own weather, not one shared sky");
    // The weather decides what is met: find a creature this zone holds in one
    // weather and not in another, then a day of each.
    int16_t only=0; int wIn=-1, wOut=-1;
    for (int k=0;k<a.count && !only;k++){
      const WildSlot &sl=WILD_SLOTS[a.first+k];
      if (sl.gate || !speciesHasArt(sl.dex)) continue;
      uint16_t present=0;   // its open slots only: a gated one cannot be met here
      for (int j=0;j<a.count;j++){
        const WildSlot &o=WILD_SLOTS[a.first+j];
        if (o.dex==sl.dex && !o.gate) present|=o.wx;
      }
      for (int w=0;w<WILD_WX_COUNT;w++) if (((a.wxMask>>w)&1) && !((present>>w)&1)){
        for (int v=0;v<WILD_WX_COUNT;v++) if ((present>>v)&1){ only=sl.dex; wIn=v; wOut=w; break; }
        if (only) break;
      }
    }
    ck(only!=0, "some creature of the zone comes in one weather and not another");
    Pet dIn, dOut;
    for (uint32_t d=0;d<DAYS && (!dIn.lastSeenEpoch || !dOut.lastSeenEpoch);d++){
      dated.lastSeenEpoch=(base+d)*86400u;
      int8_t w=wildAreaWeather((uint16_t)west,dated);
      if (w==wIn && !dIn.lastSeenEpoch) dIn.lastSeenEpoch=dated.lastSeenEpoch;
      if (w==wOut && !dOut.lastSeenEpoch) dOut.lastSeenEpoch=dated.lastSeenEpoch;
    }
    printf("      %s: %s in weather %d, absent in %d\n", a.name, DEX_TBL[only].name, wIn, wOut);
    ck(picks(west,dIn,only)>0 && picks(west,dOut,only)==0,
       "on its weather's day it can be met, on the other's never");
    // No clock: exactly the all-weather average the slots carry.
    uint32_t avgW=0, total=0;
    for (int k=0;k<a.count;k++){
      const WildSlot &sl=WILD_SLOTS[a.first+k];
      if (!(sl.wx&WILD_WX_AVG) || sl.gate || !speciesHasArt(sl.dex)) continue;
      total+=sl.weight; if (sl.dex==only) avgW+=sl.weight;
    }
    uint32_t got=0;
    for (uint32_t r=0;r<total;r++) if (wildAreaPick((uint16_t)west,WILD_WHEN_DAY,clockless,r)==only) got++;
    ck(total>0 && got==avgW, "with no clock set, its odds are exactly the all-weather average");
    // A zone with no weather is the same every day.
    bool same=true;
    for (uint32_t d=0;d<40;d++){
      dated.lastSeenEpoch=(base+d)*86400u;
      for (uint32_t r=0;r<WILD_WEIGHT_TOTAL;r+=7)
        if (wildAreaPick((uint16_t)r22,WILD_WHEN_DAY,dated,r)!=wildAreaPick((uint16_t)r22,WILD_WHEN_DAY,clockless,r)) same=false;
    }
    ck(same, "Kanto's Route 22, with no weather, is the same every day");
    bool agree=true;
    for (int i=0;i<WILD_AREA_COUNT;i++) if (WILD_AREAS[i].wxMask)
      if (wildAreaHasLive((uint16_t)i,WILD_WHEN_DAY,dIn)!=(wildAreaState((uint16_t)i,WILD_WHEN_DAY,dIn)==WILD_AREA_LIVE)) agree=false;
    ck(agree, "and on a weather day, wildAreaHasLive() and wildAreaState() still agree");
  }

  printf(bad?"FAILED %d\n":"OK\n", bad);
  return bad?1:0;
}
