// Every screen must flush, or the panel freezes on the previous frame.
// Headless screenshots CANNOT catch this: shotMode reads gfx->buffer()
// directly and never looks at frameReady, which is exactly how the training
// submenu shipped frozen.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=7; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
// wasPressed lives in the sketch and millis() in clock.cpp; both are linked in
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render();
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Arduino_Canvas *gfx;
extern Pet pet;
extern bool cardOpen, galleryOpen, clockOpen, kbOpen, menuOpen, partyOpen, partyPick;
extern bool trainOpen, movePickOpen, battleOpen, gymOpen, playerOpen;
extern bool exploreOpen, trainOpen, catchOpen;
extern uint8_t trainPage;
void startCatch();
void startRock(); void startBlock(); void startTilt();
void startPush(); void startSimon(); void startSeek();
extern uint8_t miniGame; extern uint32_t mgOverUntil;
void emuSetTilt(float, float);
extern uint8_t cardPage;
extern uint8_t partyDetail, boxDetail;
extern bool boxOpen, releaseConfirm;
extern bool bagOpen;
extern Party party;
void startBattle(int16_t dex, uint8_t lvl);
#include "trade.h"
extern Trade trade;
extern bool tradeOpen, tradePicking;
extern uint8_t tradeAsk;
void tradeOpenScreen();

static int bad = 0;
static void clearAll(){
  cardOpen=galleryOpen=clockOpen=kbOpen=menuOpen=partyOpen=partyPick=false;
  trainOpen=movePickOpen=battleOpen=gymOpen=playerOpen=false;
  exploreOpen=false;
  trainOpen=false; trainPage=0; catchOpen=false; miniGame=0;
  boxOpen=releaseConfirm=false;
  bagOpen=false;
  partyDetail=boxDetail=0;
  tradeOpen=tradePicking=false; tradeAsk=0;
}
// The trade screen draws a different layout for every state of the protocol,
// and each one is a separate chance to forget the flush. Opened for real (no
// radio here, so it starts and finds nobody), then put in each state.
static void tradeIn(uint8_t state, bool pending, bool decide){
  clearAll();
  tradeOpenScreen();
  trade.state = state;
  trade.j.active = pending ? TRADE_PENDING : 0;
  if (pending) { trade.lastRx = 0; trade.nowMs = decide ? TRADE_DECIDE_MS + 1 : 1000; }
}
static void check(const char *name){
  gfx->frameReady = false;
  render();
  if (!gfx->frameReady) { printf("FAIL  %-12s never flushed -- the panel would freeze\n", name); bad++; }
  else printf("PASS  %-12s flushes\n", name);
}

// The crash breadcrumb has to name the screen that is ACTUALLY drawn, or a
// crash report points at the wrong one -- which is worse than no report, since
// it sends the next person hunting in the wrong file. Checked here because
// this test already knows how to put each screen up.
static void crumbIs(const char *want){
  render();
  const char *got = SCREEN_NAME[uiCurrentScreen()];
  if (strcmp(got, want)) {
    printf("FAIL  crumb says '%s' while '%s' is on the panel\n", got, want); bad++;
  } else printf("PASS  crumb   %-12s named correctly\n", want);
}
int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6,false);
  pet.ageMinutes = 60UL*60;
  while (pet.hasLearnOffer()) pet.declineLearn();

  clearAll(); check("main");
  clearAll(); trainOpen=true;    check("train");
  clearAll(); trainOpen=true; trainPage=1; check("train p2");
  clearAll(); trainOpen=true; trainPage=2; check("train p3");
  clearAll(); startCatch();      check("berrycatch");
  clearAll(); startRock();       check("rocksmash");
  clearAll(); startRock(); mgOverUntil=millis()+3000; check("rocksmash result");
  clearAll(); startBlock();      check("block");
  clearAll(); startTilt();       check("tilt, no sensor");
  emuSetTilt(0, 0);
  clearAll(); startTilt();       check("tiltmaze");
  clearAll(); trainOpen=true; trainPage=3; check("train p4");
  clearAll(); startPush();       check("boulderpush");
  clearAll(); startSimon();      check("memory");
  clearAll(); startSimon(); mgOverUntil=millis()+3000; check("memory result");
  clearAll(); startSeek();       check("hideandseek");
  clearAll(); movePickOpen=true; check("movepick");
  clearAll(); exploreOpen=true;  check("explore");
  clearAll(); gymOpen=true;      check("gyms");
  clearAll(); playerOpen=true;   check("player");
  clearAll(); menuOpen=true;     check("menu");
  clearAll(); partyOpen=true;    check("party");
  clearAll(); bagOpen=true;      check("bag");
  clearAll(); clockOpen=true;    check("clock");
  for (uint8_t p=0;p<4;p++){ clearAll(); cardOpen=true; cardPage=p;
    char n[16]; snprintf(n,sizeof(n),"card%u",p); check(n); }
  clearAll(); startBattle(9,50); check("battle");
  // The detail sheets and the release confirm drawn over them. A creature has to
  // be IN the slot: an empty one falls through to the grid, which flushes for a
  // different reason and would pass without ever drawing a sheet.
  {
    PartyMon m; m.dex=25; m.level=30; m.ivAtk=m.ivDef=m.ivSpe=m.ivHp=20;
    snprintf(m.nick,sizeof(m.nick),"PIKA");
    party.slots[0]=m; party.box[0]=m; party.save(); party.boxSave();
  }
  clearAll(); partyOpen=true; partyDetail=1;                      check("partysheet");
  clearAll(); partyOpen=true; partyDetail=1; releaseConfirm=true;  check("partyconfirm");
  clearAll(); partyOpen=true; boxOpen=true; boxDetail=1;           check("boxsheet");
  clearAll(); partyOpen=true; boxOpen=true; boxDetail=1; releaseConfirm=true; check("boxconfirm");
  tradeIn(TRADE_LISTENING, false, false); check("tradelook");
  tradeIn(TRADE_CHOOSING, false, false);  trade.mine=party.slots[0]; trade.theirs=party.box[0];
                                          trade.theirsOk=true; check("tradechoose");
  tradeIn(TRADE_CHOOSING, false, false);  tradePicking=true; check("tradepick");
  tradeIn(TRADE_PENDING, true, false);    check("tradebusy");
  tradeIn(TRADE_STUCK, true, false);      check("tradestuck");
  tradeIn(TRADE_STUCK, true, true);       check("tradedecide");
  tradeIn(TRADE_STUCK, true, true);       tradeAsk=2; check("tradesure");
  tradeIn(TRADE_DONE, false, false);      check("tradedone");
  tradeIn(TRADE_CANCELLED, false, false); check("tradecancel");
  tradeIn(TRADE_REFUSED, false, false);   check("traderefused");

  clearAll();                       crumbIs("main");
  clearAll(); trainOpen=true;       crumbIs("train");
  clearAll(); menuOpen=true;        crumbIs("menu");
  clearAll(); partyOpen=true;       crumbIs("party");
  clearAll(); exploreOpen=true;     crumbIs("explore");
  clearAll(); gymOpen=true;         crumbIs("gym");
  clearAll(); playerOpen=true;      crumbIs("player");
  clearAll(); movePickOpen=true;    crumbIs("movepick");
  clearAll(); clockOpen=true;       crumbIs("clock");
  clearAll(); cardOpen=true;        crumbIs("card");
  clearAll(); bagOpen=true;         crumbIs("bag");
  clearAll(); startBattle(9,50);    crumbIs("battle");
  tradeIn(TRADE_LISTENING,false,false); crumbIs("trade");
  trade.j.active = 0;
  clearAll();

  printf("%s\n", bad ? "FAILURES" : "every screen flushes");
  return bad?1:0;
}
