// Kanto ladder win rates, v3.57 rules ("before") against v3.58 ("after"):
// a solo perfect-IV Charizard, untrained and fully trained, easy and hard,
// your level capped to the leader's best as in the game. Not a test -- a
// measurement. Build it like a core test (see tests/run.sh CORE) and run it.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include <cstdio>
uint32_t g_seed=777; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
// before = the v3.57 formula: training added flat, leaders untrained
static void unscale(Combatant &c,uint8_t tr){
  int add=tr-(int)tr*c.level/100;
  c.base[SI_ATK]+=add; c.base[SI_DEF]+=add; c.base[SI_SPA]+=add; c.base[SI_SPD]+=add; c.base[SI_SPE]+=add;
}
static void mk(Combatant &c,int16_t dex,uint8_t lvl,uint8_t iv,uint8_t tr,bool before){
  Pet p; p.dbgHatchAs(dex,false); p.ivAtk=p.ivDef=p.ivSpe=p.ivHp=iv;
  p.trAtk=p.trDef=p.trSpe=tr; p.ageMinutes=(uint32_t)(lvl-1)*MINUTES_PER_LEVEL;
  p.relearnFromLevel(); combatantFromPet(c,p); if(before) unscale(c,tr);
}
static bool fight(const Trainer &t,uint8_t L,uint8_t tr,bool hard,bool before){
  uint8_t cap=0; for(int i=0;i<t.count;i++) if(t.team[i].level>cap) cap=t.team[i].level;
  uint8_t lv=L<cap?L:cap; Combatant A; mk(A,6,lv,31,tr,before);
  for(int k=0;k<t.count;k++){
    Combatant B; uint8_t iv=hard?HARD_IV:EASY_IV; uint8_t ftr=(hard&&!before)?Pet::trMaxFor(iv):0;
    mk(B,t.team[k].dex,t.team[k].level,iv,ftr,before);
    TurnLog lg; int n=0;
    while(!A.fainted()&&!B.fainted()&&n++<300){
      uint8_t ma=aiChooseMove(A,B,true),mb=aiChooseMove(B,A,hard);
      Combatant *f=&A,*s=&B; uint8_t mf=ma,ms=mb;
      if(!battleMovesFirst(A,ma,B,mb)){f=&B;s=&A;mf=mb;ms=ma;}
      battleAct(*f,*s,mf,lg); if(!s->fainted()) battleAct(*s,*f,ms,lg);
      battleEndTurn(A,lg); battleEndTurn(B,lg);
    }
    if(A.fainted()) return false;
  }
  return true;
}
int main(){
  const Trainer *T=TRAINER_SETS[0].list; const int R=30;
  const uint8_t Ls[]={14,40,73,100};
  for(int hard=0;hard<2;hard++) for(int trd=0;trd<2;trd++){
    printf("\n%s, Charizard %s\n",hard?"HARD":"EASY",trd?"fully trained":"untrained");
    printf("lvl | ");for(int i=0;i<13;i++)printf("%-9.9s ",T[i].name);printf("\n");
    for(uint8_t L:Ls){ printf("%3u | ",L);
      for(int i=0;i<13;i++){int b=0,a=0;for(int r=0;r<R;r++){b+=fight(T[i],L,trd?100:0,hard,true);a+=fight(T[i],L,trd?100:0,hard,false);}
        printf("%3d>%-3d   ",b*100/R,a*100/R);} printf("\n"); }
  }
}
