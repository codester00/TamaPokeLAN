// Choosing who comes on after a faint in a LAN fight (v4.02, LINK_PROTO 12).
// The host used to decide both sides' replacements at resolution. Now a side
// with more than one that could follow is marked LINK_PICK in the result and
// the next turn is a pick turn: each board chooses (or says READY with nothing
// to choose), the guest's choice rides its ordinary action message, and the
// host's result for that turn carries no beats, only who is out. With one left
// the host still brings it straight on. Driven through the real sketch, one
// board at a time, with the other board's packets written by hand.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "link.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 12; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void startLinkBattle();
uint8_t btlSwitchOpts(uint8_t out[TRAINER_TEAM_MAX]);
void btlSwitchCell(uint8_t k, int *x, int *y);
void btlCellCenter(int i, int *x, int *y);
uint8_t btlMenuNow();
extern Pet pet;
extern Link lan;
extern bool battleOpen, btlOver, btlLinkPick;
extern Combatant btlYou, btlFoe, btlSquad[], btlFoeSquad[];
extern uint8_t btlSquadN, btlSquadAt, btlFoeAt, btlFoeSquadN, btlMenu, btlMsgCount, btlMyAct, btlSwapDue;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint8_t lastRes[256], lastResN = 0, lastAct = 0;
static void capture(void *, const uint8_t *b, uint8_t n) {
  if (b[0] == LM_RESULT) { memcpy(lastRes, b, n); lastResN = n; }
  if (b[0] == LM_ACT && n >= 4) lastAct = b[3];     // [type][len][turn][act]
}
static bool lastResult(LinkResult &r) {
  if (lastResN < 3 + sizeof(LinkResult)) return false;
  memcpy(&r, lastRes + 3, sizeof(r));
  return true;
}
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) battleTap(233, 300); }
static void tapOpt(uint8_t k) { int x, y; btlSwitchCell(k, &x, &y); battleTap(x + 80, y + 27); }
static LinkMon mon(int16_t dex, uint8_t lvl, const char *nm) {
  Pet p; p.dbgHatchAs(dex, false);
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 25;
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  Combatant c; combatantFromPet(c, p);
  snprintf(c.name, sizeof(c.name), "%s", nm);
  LinkMon m; linkMonFrom(m, c); return m;
}
static void hostFight(bool strongMine) {
  lan.begin(true, "HOST");
  snprintf(lan.peerName, sizeof(lan.peerName), "%s", "RIVAL");
  lan.addMon(mon(strongMine ? 150 : 10, strongMine ? 70 : 3, "A0"));
  lan.addMon(mon(143, 50, "A1"));
  lan.addMon(mon(9, 50, "A2"));
  lan.theirs[0] = mon(strongMine ? 10 : 150, strongMine ? 3 : 70, "B0");
  lan.theirs[1] = mon(11, 30, "B1");
  lan.theirs[2] = mon(12, 30, "B2");
  lan.theirsN = 3;
  lan.state = LINK_READY;
  lan.send = capture;
  startLinkBattle();
}
static void guestFight() {
  lan.begin(false, "GUEST");
  snprintf(lan.peerName, sizeof(lan.peerName), "%s", "HOST");
  lan.addMon(mon(10, 5, "G0"));
  lan.addMon(mon(11, 5, "G1"));
  lan.addMon(mon(12, 5, "G2"));
  lan.theirs[0] = mon(150, 70, "H0");
  lan.theirs[1] = mon(143, 70, "H1");
  lan.theirs[2] = mon(9, 70, "H2");
  lan.theirsN = 3;
  lan.state = LINK_READY;
  lan.send = capture;
  startLinkBattle();
}
static void deliver(const LinkResult &r) {
  memcpy(lan.result, &r, sizeof(r)); lan.resultN = sizeof(r); lan.resultNew = true;
  lan.state = LINK_READY;
  render();
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  LinkResult r{};

  // --- 1. host: the rival's creature faints with two behind it ---------------------
  hostFight(true);
  btlFoe.hp = 1;
  btlMenu = 1;
  { int x, y; btlCellCenter(0, &x, &y); battleTap(x, y); }   // our move, latched
  lan.pendingAct = LINK_ACT_MOVE(0);
  render();
  ck(lastResult(r) && r.guestHp == 0, "(the rival's lead fainted, and the turn was shipped)");
  ck(r.guestNext == LINK_PICK, "with two to follow, the result says the RIVAL picks");
  ck(r.hostNext == LINK_NO_NEXT, "(ours is standing)");
  clearMsgs();
  ck(btlLinkPick && btlMyAct == LINK_ACT_READY, "the host, with nothing to choose, says READY");
  ck(btlFoeAt == 0, "and nothing comes on before the rival has chosen");
  lastResN = 0;
  lan.pendingAct = LINK_ACT_SWITCH_TO(2);   // the rival picks B2, not the next in line
  render();
  ck(btlFoeAt == 2 && btlFoe.dex == 12, "the rival's pick comes on, not the next standing one");
  ck(lastResult(r) && r.guestIdx == 2 && r.beatN == 0 && r.guestHp == btlFoe.hp,
     "and the pick turn's result says who is out, and nothing else happened");
  ck(!btlLinkPick && btlMenuNow() == 0, "then the fight goes on");
  battleOpen = false;

  // a pick the rival may not make (the fainted one) falls back to the next standing
  hostFight(true);
  btlFoe.hp = 1;
  btlMenu = 1;
  { int x, y; btlCellCenter(0, &x, &y); battleTap(x, y); }
  lan.pendingAct = LINK_ACT_MOVE(0);
  render();
  clearMsgs();
  lan.pendingAct = LINK_ACT_SWITCH_TO(0);   // the one that just fainted
  render();
  ck(btlFoeAt == 1, "a pick of a fainted creature brings on the next standing one");
  battleOpen = false;

  // --- 2. host: our own faints with two behind it -----------------------------------
  hostFight(false);
  btlYou.hp = 1;
  for (int t = 0; t < 30 && !btlYou.fainted(); t++) {
    btlYou.hp = 1;
    btlMenu = 1;
    { int x, y; btlCellCenter(0, &x, &y); battleTap(x, y); }
    lan.pendingAct = LINK_ACT_MOVE(0);
    render();
    if (!btlYou.fainted()) clearMsgs();
  }
  ck(lastResult(r) && r.hostNext == LINK_PICK, "ours fainted: the result says WE pick");
  clearMsgs();
  ck(btlMenuNow() == 4, "the host gets the choice");
  tapOpt(1);                                // A2
  ck(btlMyAct == LINK_ACT_SWITCH_TO(2) && btlSquadAt == 0, "the pick is latched, not yet brought on");
  lan.pendingAct = LINK_ACT_READY;          // the rival has nothing to choose
  render();
  ck(btlSquadAt == 2 && lastResult(r) && r.hostIdx == 2, "both in: our pick comes on and is shipped");
  battleOpen = false;

  // --- 3. guest: its own faints, with two behind it ---------------------------------
  guestFight();
  memset(&r, 0, sizeof(r));
  r.hostHp = btlFoe.maxHp; r.guestHp = 0; r.hostMove = btlFoe.moves[0];
  r.hostIdx = 0; r.guestIdx = 0;
  r.hostNext = LINK_NO_NEXT; r.guestNext = LINK_PICK;
  deliver(r);
  ck(btlYou.fainted() && (btlSwapDue & 1), "the guest shows its creature fainting");
  clearMsgs();
  ck(btlMenuNow() == 4, "and then gets the choice");
  lastAct = 0;
  tapOpt(1);                                // G2
  ck(lastAct == LINK_ACT_SWITCH_TO(2) && lan.state == LINK_WAITING,
     "its pick goes to the host as an action, and it waits");
  memset(&r, 0, sizeof(r));
  r.hostHp = btlFoe.hp; r.guestHp = btlSquad[2].hp; r.hostIdx = 0; r.guestIdx = 2;
  r.hostNext = r.guestNext = LINK_NO_NEXT;
  deliver(r);
  clearMsgs();
  ck(btlSquadAt == 2 && btlYou.dex == 12 && !btlSwapDue && !btlLinkPick,
     "the host's pick-turn result brings it on");
  battleOpen = false;

  // --- 4. guest: the host's faints; the guest has nothing to choose -----------------
  guestFight();
  memset(&r, 0, sizeof(r));
  r.hostHp = 0; r.guestHp = btlYou.maxHp; r.hostIdx = 0; r.guestIdx = 0;
  r.hostNext = LINK_PICK; r.guestNext = LINK_NO_NEXT;
  deliver(r);
  lastAct = 0;
  clearMsgs();
  ck(lastAct == LINK_ACT_READY, "the guest says READY by itself");
  memset(&r, 0, sizeof(r));
  r.hostHp = btlFoeSquad[1].maxHp; r.guestHp = btlYou.hp; r.hostIdx = 1; r.guestIdx = 0;
  r.hostNext = r.guestNext = LINK_NO_NEXT;
  deliver(r);
  ck(btlFoeAt == 1, "and shows the host's pick when it lands");
  battleOpen = false;

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
