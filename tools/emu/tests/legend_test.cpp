// Explore's legendaries (v3.40): a guaranteed first meeting at home, then a rare
// repeat until caught; roamers; gates of their own; the save; and the egg pool
// leaving story legendaries out (docs/explore-plan.md § 7-8).
//
// The legendaries are found from WILD_LEGENDS by dex rather than by position,
// the gates are opened with real badges, and the save is checked by reloading a
// fresh Pet -- through the checkpoint, and again through the legacy key with
// the checkpoint deleted. Every "never" has a companion proving it could.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "wild.h"
#include "noart.h"
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t g_seed=11; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static int legendOf(int16_t dex){
  for (uint8_t i=0;i<wildLegendCount();i++) if (wildLegendDex(i)==dex) return i;
  return -1;
}
static int areaIn(uint8_t region, const char *name){
  for (uint8_t k=0;k<wildAreaCount(region);k++){
    uint16_t a=wildAreaFirst(region)+k;
    if (!strcmp(wildAreaName(a),name)) return a;
  }
  return -1;
}
static void badges(Pet &p, uint8_t region, uint8_t n){
  for (uint8_t i=0;i<n;i++) p.winBadge(region,i,false);
}
// Over rolls 0..9999: how many times legendary `want` comes up in `area`.
static int count(uint16_t area, const Pet &p, int want){
  int n=0;
  for (uint32_t r=0;r<10000;r++) if (wildLegendFor(area,p,r)==want) n++;
  return n;
}

int main(){
  gRegionArt=0xFFFF;
  int art=legendOf(144), zap=legendOf(145), mew2=legendOf(150);
  int rai=legendOf(243), ent=legendOf(244), sui=legendOf(245), lug=legendOf(249);
  int seafoam=areaIn(0,"SEAFOAM ISLANDS"), plant=areaIn(0,"POWER PLANT"), r1=areaIn(0,"ROUTE 1");
  int burned=areaIn(1,"BURNED TOWER"), r29=areaIn(1,"ROUTE 29"), whirl=areaIn(1,"WHIRL ISLANDS");
  ck(art>=0 && zap>=0 && mew2>=0 && rai>=0 && ent>=0 && sui>=0 && lug>=0,
     "the legendaries this test uses have homes");
  ck(seafoam>=0 && plant>=0 && r1>=0 && burned>=0 && r29>=0 && whirl>=0,
     "and so do the areas");
  ck(wildLegendHome(art)==(uint16_t)seafoam && wildLegendHome(lug)==(uint16_t)whirl,
     "Articuno lives in Seafoam Islands and Lugia in the Whirl Islands");
  ck(legendOf(151)<0 && legendOf(251)<0, "Mew and Celebi have no home: they are mythical");

  // ---- the gate -------------------------------------------------------------
  {
    Pet p;
    badges(p,0,4);   // one short of Seafoam's Soul Badge gate
    ck(!wildLegendLive(art,p) && !wildAreaHasLegend(seafoam,p),
       "a legendary behind a locked gate cannot appear and stars nothing");
    p.winBadge(0,4,false);
    ck(wildLegendLive(art,p) && wildAreaHasLegend(seafoam,p),
       "the Soul Badge opens it, and its home is starred");
  }

  // ---- the guaranteed first meeting, then the rare repeat ------------------
  {
    Pet p; badges(p,0,5);
    ck(count(seafoam,p,art)==10000, "until it is met, every Explore at home meets it");
    ck(count(r1,p,art)==0, "and nowhere else -- Articuno does not roam");
    ck(count(plant,p,zap)==10000 && count(plant,p,art)==0,
       "next door, the Power Plant's own legendary comes first, not Articuno");
    p.setLegendMet(art);
    int n=count(seafoam,p,art);
    ck(n==WILD_LEGEND_PCT*100, "once met, it comes back in exactly WILD_LEGEND_PCT of encounters");
    ck(count(seafoam,p,-1)==10000-n, "and the rest are ordinary encounters");
    p.setLegendCaught(art);
    ck(count(seafoam,p,art)==0 && !wildAreaHasLegend(seafoam,p),
       "caught, it never appears again and the star goes");
    ck(wildLegendLive(zap,p), "while the others are untouched");
  }

  // ---- a gate of its own, after its home area's -----------------------------
  {
    Pet p; badges(p,1,3);   // Johto, three badges: the Burned Tower is open
    ck(count(burned,p,rai)==10000, "the Burned Tower's first meeting is Raikou");
    p.setLegendMet(rai);
    ck(count(burned,p,ent)==10000, "then Entei");
    p.setLegendMet(ent);
    ck(count(burned,p,sui)==0 && !wildLegendLive(sui,p),
       "Suicune waits for its own gate, seven badges, though its home is open");
    badges(p,1,7);
    ck(count(burned,p,sui)==10000, "and comes with the seventh badge");
  }

  // ---- roamers ---------------------------------------------------------------
  {
    Pet p; badges(p,1,3);
    ck(count(r29,p,rai)==0 && count(r29,p,ent)==0,
       "a roamer not yet met never turns up away from home");
    p.setLegendMet(rai); p.setLegendMet(ent);
    int nr=count(r29,p,rai), ne=count(r29,p,ent);
    ck(nr>0 && ne>0, "once met, both roam: each turns up on Route 29");
    ck(nr+ne==WILD_LEGEND_PCT*100, "sharing the rare chance between them");
    badges(p,0,8);
    ck(count(r1,p,rai)==0 && count(r1,p,ent)==0, "but never outside Johto");
    p.setLegendCaught(rai);
    ck(count(r29,p,rai)==0 && count(r29,p,ent)==WILD_LEGEND_PCT*100,
       "caught, Raikou stops roaming and Entei has the chance to itself");
  }

  // ---- the champion gate and the sprite pack ------------------------------
  {
    Pet p; badges(p,1,8);
    ck(!wildLegendLive(lug,p), "Lugia waits for the champion");
    p.winBadge(1,TRAINER_COUNT-1,false);
    ck(wildLegendLive(lug,p), "and is there after");
    gRegionArt=1u<<0;   // Kanto's pack only
    ck(!wildLegendLive(lug,p), "a legendary whose pack is missing cannot appear");
    gRegionArt=0xFFFF;
  }

  // ---- Hoenn (v3.42): a gate of its own, and roamers after the champion ----
  {
    int regice=legendOf(378), latias=legendOf(380), latios=legendOf(381), ray=legendOf(384);
    int sea=areaIn(2,"ROUTES 105-109"), r101=areaIn(2,"ROUTE 101"), pillar=areaIn(2,"SKY PILLAR");
    ck(regice>=0 && latias>=0 && latios>=0 && ray>=0 && sea>=0 && r101>=0 && pillar>=0,
       "Hoenn's legendaries and areas are found");
    Pet p; badges(p,2,6);   // Surf and more: Regice's sea is open
    ck(wildAreaUnlocked((uint16_t)sea,p) && count(sea,p,regice)==0,
       "Regice's sea is open with six badges but Regice waits for Dive");
    p.winBadge(2,6,false);
    ck(count(sea,p,regice)==10000, "and comes with the Mind Badge");
    badges(p,2,8);
    ck(wildAreaUnlocked((uint16_t)pillar,p) && count(pillar,p,ray)==0,
       "Rayquaza's Sky Pillar is open before the champion; Rayquaza is not");
    p.winBadge(2,TRAINER_COUNT-1,false);
    ck(count(pillar,p,ray)==10000, "after the champion it is");
    p.setLegendMet(latias); p.setLegendMet(latios);
    int a=count(r101,p,latias), o=count(r101,p,latios);
    ck(a>0 && o>0 && a+o==WILD_LEGEND_PCT*100, "met, Latias and Latios roam Hoenn, as far as Route 101");
    badges(p,0,8);
    int r1=areaIn(0,"ROUTE 1");
    ck(count(r1,p,latias)==0 && count(r1,p,latios)==0, "and never leave it");
  }

  // ---- Sinnoh (v3.43): the lake trio after Candice, a roamer, Giratina -----
  {
    int uxie=legendOf(480), mesprit=legendOf(481), gira=legendOf(487), gigas=legendOf(486);
    int acuity=areaIn(3,"LAKE ACUITY"), verity=areaIn(3,"LAKE VERITY"), r201=areaIn(3,"ROUTE 201");
    int turnback=areaIn(3,"TURNBACK CAVE");
    ck(uxie>=0 && mesprit>=0 && gira>=0 && gigas>=0 && acuity>=0 && verity>=0 && r201>=0 &&
       turnback>=0, "Sinnoh's legendaries and areas are found");
    ck(wildLegendLevel(gigas)==70, "Regigigas is level 70, the owner's override of Platinum's 1");
    Pet p; badges(p,3,6);
    ck(wildAreaUnlocked((uint16_t)verity,p) && count(verity,p,mesprit)==0,
       "Lake Verity is open from the start; Mesprit waits for Candice's badge");
    p.winBadge(3,6,false);
    ck(count(acuity,p,uxie)==10000 && count(verity,p,mesprit)==10000 &&
       count(turnback,p,gira)==10000, "with it, Uxie, Mesprit and Giratina each meet you once at home");
    ck(count(r201,p,mesprit)==0, "Mesprit not yet met does not roam");
    p.setLegendMet(mesprit);
    ck(count(r201,p,mesprit)==WILD_LEGEND_PCT*100, "once met it roams Sinnoh, as far as Route 201");
  }

  // ---- Unova (v3.44, from PokeAPI): the Swords, the dragons, Kyurem -------
  {
    int cob=legendOf(638), resh=legendOf(643), zek=legendOf(644), kyu=legendOf(646);
    int r13=areaIn(4,"ROUTE 13"), spiral=areaIn(4,"DRAGONSPIRAL"), r19=areaIn(4,"ROUTE 19");
    ck(cob>=0 && resh>=0 && zek>=0 && kyu>=0 && r13>=0 && spiral>=0 && r19>=0,
       "Unova's legendaries and areas are found");
    ck(legendOf(641)<0 && legendOf(642)<0 && legendOf(645)<0,
       "the Forces of Nature have no home: B2W2 never offers them");
    Pet p; badges(p,4,5);
    ck(count(r13,p,cob)==0 && !wildAreaUnlocked((uint16_t)r13,p), "Route 13 waits for the Jet Badge");
    p.winBadge(4,5,false);
    ck(count(r13,p,cob)==10000, "and with it Cobalion meets you there once");
    badges(p,4,8);
    ck(count(spiral,p,resh)==0 && count(spiral,p,zek)==0, "the dragons wait for the champion");
    p.winBadge(4,TRAINER_COUNT-1,false);
    ck(count(spiral,p,resh)==10000, "then Reshiram comes first at Dragonspiral Tower");
    p.setLegendMet(resh);
    ck(count(spiral,p,zek)==10000, "and Zekrom next, both versions' dragons in one home");
    ck(wildLegendLevel(kyu)==70 && wildLegendLevel(cob)==45, "levels are PokeAPI's B2W2 statics");
    ck(count(r19,p,cob)==0, "the Swords do not roam");
  }

  // ---- Kalos (v3.45, from PokeAPI): Xerneas and Yveltal, Zygarde ----------
  {
    int xer=legendOf(716), yve=legendOf(717), zyg=legendOf(718);
    int r13=areaIn(5,"ROUTE 13"), term=areaIn(5,"TERMINUS CAVE");
    ck(xer>=0 && yve>=0 && zyg>=0 && r13>=0 && term>=0, "Kalos's legendaries and areas are found");
    Pet p; badges(p,5,6);
    ck(wildAreaUnlocked((uint16_t)r13,p) && count(r13,p,xer)==0 && count(r13,p,yve)==0,
       "the Lumiose Badlands are open; Xerneas and Yveltal wait for Olympia's badge");
    p.winBadge(5,6,false);
    ck(count(r13,p,xer)==10000, "then Xerneas meets you once");
    p.setLegendMet(xer);
    ck(count(r13,p,yve)==10000, "then Yveltal -- both versions' in one home");
    ck(wildLegendLevel(xer)==50 && wildLegendLevel(zyg)==70, "at PokeAPI's X/Y levels");
    badges(p,5,8);
    ck(wildAreaUnlocked((uint16_t)term,p) && count(term,p,zyg)==0, "Zygarde waits for the champion");
    p.winBadge(5,TRAINER_COUNT-1,false);
    ck(count(term,p,zyg)==10000, "and is there after");
  }

  // ---- Alola (v3.46, from PokeAPI): Solgaleo and Lunala, the Tapus, the
  // Ultra Beasts and Necrozma -------------------------------------------------
  {
    int sol=legendOf(791), lun=legendOf(792), koko=legendOf(785), buzz=legendOf(794);
    int guzz=legendOf(799), necro=legendOf(800);
    int canyon=areaIn(6,"VAST PONI CANYON"), r1a=areaIn(6,"ROUTE 1"), meadow=areaIn(6,"MELEMELE MEADOW");
    ck(sol>=0 && lun>=0 && koko>=0 && buzz>=0 && guzz>=0 && necro>=0 && canyon>=0 && r1a>=0 &&
       meadow>=0, "Alola's legendaries and areas are found");
    ck(legendOf(789)<0 && legendOf(772)<0, "Cosmog and Type: Null are gifts, never fought: no home");
    Pet p; badges(p,6,6);
    ck(!wildAreaUnlocked((uint16_t)canyon,p) && count(canyon,p,sol)==0,
       "Vast Poni Canyon waits for Nanu's grand trial, and Solgaleo with it");
    p.winBadge(6,6,false);
    ck(count(canyon,p,sol)==10000, "then Solgaleo meets you once at the altar");
    p.setLegendMet(sol);
    ck(count(canyon,p,lun)==10000, "then Lunala -- both versions' in one home");
    ck(count(r1a,p,koko)==0 && count(meadow,p,buzz)==0,
       "Route 1 and Melemele Meadow are open, but Tapu Koko and Buzzwole wait for the champion");
    p.winBadge(6,TRAINER_COUNT-1,false);
    ck(count(r1a,p,koko)==10000 && count(meadow,p,buzz)==10000,
       "and after the Hall of Fame they are there");
    ck(wildLegendLevel(sol)==55 && wildLegendLevel(koko)==60 && wildLegendLevel(guzz)==70 &&
       wildLegendLevel(necro)==75, "at PokeAPI's Sun/Moon levels, walk-ins included");
    // Necrozma waits until every Ultra Beast is CAUGHT (owner's call), and
    // its home wears no star until then. Six of seven is not enough.
    int ten=areaIn(6,"TEN CARAT HILL");
    ck(ten>=0, "Ten Carat Hill is found");
    for (int16_t d=793; d<=798; d++) p.setLegendCaught(legendOf(d));
    ck(count(ten,p,necro)==0 && !wildAreaHasLegend((uint16_t)ten,p),
       "with six Ultra Beasts caught, Necrozma is not there and Ten Carat Hill has no star");
    p.setLegendMet(guzz);
    ck(count(ten,p,necro)==0, "meeting the seventh is not catching it");
    p.setLegendCaught(guzz);
    ck(count(ten,p,necro)==10000 && wildAreaHasLegend((uint16_t)ten,p),
       "the seventh caught, Necrozma meets you once and its home wears the star");
  }

  // ---- Galar (v3.47, from PokeAPI): Zacian and Zamazenta, Eternatus ---------
  {
    int zac=legendOf(888), zam=legendOf(889), eter=legendOf(890);
    int depths=areaIn(7,"WEALD DEPTHS"), hills=areaIn(7,"HAMMERLOCKE HLS.");
    ck(zac>=0 && zam>=0 && eter>=0 && depths>=0 && hills>=0, "Galar's legendaries and areas are found");
    Pet p; badges(p,7,8);
    ck(wildAreaUnlocked((uint16_t)hills,p) && count(hills,p,eter)==0 && !wildAreaUnlocked((uint16_t)depths,p),
       "with eight badges the hills are open, but Eternatus and the Weald's depths wait for the champion");
    p.winBadge(7,TRAINER_COUNT-1,false);
    ck(count(hills,p,eter)==10000 && count(depths,p,zac)==10000, "then Eternatus and Zacian meet you once each");
    p.setLegendMet(zac);
    ck(count(depths,p,zam)==10000, "then Zamazenta -- both versions' in one home");
    ck(wildLegendLevel(zac)==70 && wildLegendLevel(eter)==60, "at PokeAPI's Sword/Shield levels");
  }

  // ---- the save --------------------------------------------------------------
  {
    Pet a; a.begin();
    a.setLegendMet(art); a.setLegendCaught(zap); a.setLegendMet(rai);
    ck(a.legendMet(zap), "caught implies met");
    a.saveNow();
    ck(a.saveHealthy() && (nvs().count("plyA") || nvs().count("plyB")) && nvs().count("lgnd"),
       "the save lands, checkpoint and legacy key both");
    Pet b; b.begin();
    ck(b.legendMet(art) && !b.legendCaught(art) && b.legendCaught(zap) && b.legendMet(rai) &&
       !b.legendMet(ent), "a reload keeps exactly the met and caught bits");
    // That reload could pass on either copy alone, so prove EACH carries them:
    // first the checkpoint, with the legacy key gone (the begin() above wrote
    // nothing new, so the checkpoint is still the one a.saveNow() made).
    std::vector<uint8_t> legacy = nvs()["lgnd"];
    nvs().erase("lgnd");
    Pet ck1; ck1.begin();
    ck(ck1.legendMet(art) && ck1.legendCaught(zap) && ck1.legendMet(rai) && !ck1.legendMet(ent),
       "the checkpoint alone carries them, with the legacy key deleted");
    nvs()["lgnd"] = legacy;
    // The checkpoint gone, the legacy key alone must carry them.
    nvs().erase("plyA"); nvs().erase("plyB");
    Pet c; c.begin();
    ck(c.legendMet(art) && c.legendCaught(zap) && c.legendMet(rai) && !c.legendMet(ent),
       "and so does the legacy key, with the checkpoint deleted");
    // A save from before legendaries: neither the tail nor the key.
    nvs().erase("lgnd");
    Pet d; d.begin();
    ck(!d.legendMet(art) && !d.legendCaught(zap), "and an older save reads as none met");
  }

  // ---- eggs: caught, never hatched -------------------------------------------
  {
    Pet p;
    bool homed=false, mew=false, celebi=false;
    for (int i=0;i<600;i++){
      int16_t k=p.rollInRegion(0,R_LEGENDARIO), j=p.rollInRegion(1,R_LEGENDARIO);
      if (wildIsExploreLegend(k) || wildIsExploreLegend(j)) homed=true;
      if (k==151) mew=true;
      if (j==251) celebi=true;
    }
    ck(!homed, "no egg hatches a legendary that has a home in Explore");
    ck(mew && celebi, "while Mew and Celebi still hatch -- the legendary tier is not empty");
    // Hoenn's through Galar's have homes (v3.42-v3.47); Paldea's do not yet.
    // Found by asking wildIsExploreLegend(), not by a typed range.
    bool homed2=false, paldeaLegend=false, forces=false, ub=false, gifts=false, dlc=false;
    for (int i=0;i<900;i++){
      for (uint8_t r=2;r<=7;r++) if (wildIsExploreLegend(p.rollInRegion(r,R_LEGENDARIO))) homed2=true;
      int16_t k=p.rollInRegion(8,R_LEGENDARIO);
      if (k>=1001 && k<=1024 && !wildIsExploreLegend(k)) paldeaLegend=true;   // the treasures .. Terapagos
      int16_t gl=p.rollInRegion(7,R_LEGENDARIO);
      if (gl>=891 && gl<=898) dlc=true;   // Kubfu .. Calyrex: the expansions, not in Sword/Shield's data
      int16_t u=p.rollInRegion(4,R_LEGENDARIO);
      if (u==641 || u==642 || u==645) forces=true;
      // The Ultra Beasts were RARE-tier eggs until they got homes.
      if (wildIsExploreLegend(p.rollInRegion(6,R_RARO))) ub=true;
      int16_t a=p.rollInRegion(6,R_LEGENDARIO);
      if (a==772 || a==789) gifts=true;
    }
    ck(!homed2, "Hoenn's through Galar's homed legendaries left their eggs with their areas");
    ck(!ub, "and so did the Ultra Beasts, from the rare tier");
    ck(gifts, "while Type: Null and Cosmog, gifts in the game, still hatch");
    ck(dlc, "Galar's expansion legendaries, met in no table here, still hatch");
    ck(paldeaLegend, "and a region whose legendaries have no homes yet still hatches them");
    ck(forces, "Tornadus, Thundurus and Landorus, never met in B2W2, still hatch");
  }

  printf(bad?"FAILED %d\n":"OK\n", bad);
  return bad?1:0;
}
