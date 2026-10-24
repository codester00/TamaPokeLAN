// The LAN battle fixes of v3.57, each through the real sketch: the five-option
// POKEMON screen, STRUGGLE for a creature that can choose nothing, a scene that
// stays put for the whole fight, and the replacement after a faint being
// decided by the host and shipped to the guest -- the bugs found on two boards
// on 2026-10-24. Most of it is not LAN-only: the switch screen, Struggle and
// the next-standing rule are every fight's.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "pp.h"
#include "link.h"
#include <cstdio>
uint32_t g_seed = 11; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void startBattle(int16_t dex, uint8_t lvl);
void startTrainerBattle(uint8_t idx, bool hard);
void startLinkBattle();
uint8_t btlSwitchOpts(uint8_t out[TRAINER_TEAM_MAX]);
void btlSwitchCell(uint8_t k, int *x, int *y);
int btlSwitchAt(int16_t x, int16_t y);
int btlCellIndexAt(int16_t x, int16_t y);
extern Pet pet;
extern Link lan;
extern bool battleOpen, btlLink, btlLinkHost, btlOver, btlWon, btlWild;
extern Combatant btlYou, btlFoe, btlSquad[], btlFoeSquad[];
extern uint8_t btlSquadN, btlSquadAt, btlFoeAt, btlFoeSquadN, btlMenu, btlMsgCount, btlMyAct;
extern uint8_t btlSwapDue, btlNextAt[2];
extern uint16_t btlNextHp[2];
extern int16_t btlSceneDex;
extern char btlMsg[8][40];
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// what the sketch sends, so a shipped LinkResult can be read back
static uint8_t lastPkt[256]; static uint8_t lastLen = 0;
static void capture(void *, const uint8_t *b, uint8_t n) {
  if (b[0] == LM_RESULT) { memcpy(lastPkt, b, n); lastLen = n; }   // the last RESULT, not an END after it
}
static bool lastResult(LinkResult &r) {
  if (lastLen < 3 + sizeof(LinkResult) || lastPkt[0] != LM_RESULT) return false;
  memcpy(&r, lastPkt + 3, sizeof(r));     // [type][len][turn] then the result
  return true;
}
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) battleTap(233, 300); }
static bool said(const char *w) {
  for (uint8_t i = 0; i < btlMsgCount; i++) if (strstr(btlMsg[i], w)) return true;
  return false;
}
static LinkMon mon(int16_t dex, uint8_t lvl, const char *nm) {
  Pet p; p.dbgHatchAs(dex, false);
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 25;
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  Combatant c; combatantFromPet(c, p);
  snprintf(c.name, sizeof(c.name), "%s", nm);
  LinkMon m; linkMonFrom(m, c); return m;
}
static void strongPet() {
  pet.dbgHatchAs(150, false);   // MEWTWO
  pet.ageMinutes = 99UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  strongPet();
  for (int i = 0; i < PARTY_SLOTS; i++) {
    PartyMon m; m.dex = 130 + i; m.level = 80;   // GYARADOS.. strong enough
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
    m.moves[0] = 1;
    party.replaceAt(i, m);
  }
  squadMask = 0xFFFF;

  // --- 1. the POKEMON screen offers all five others ---------------------------
  startBattle(10, 2);                   // a level 2 CATERPIE: nothing faints
  ck(btlSquadN == 6, "(a full squad of six)");
  uint8_t opt[TRAINER_TEAM_MAX];
  ck(btlSwitchOpts(opt) == 5, "five options: everyone but the one out");
  bool distinct = true;
  for (uint8_t k = 0; k < 5; k++) if (opt[k] == btlSquadAt) distinct = false;
  ck(distinct, "and the one out is not among them");
  int seen = 0;
  for (uint8_t k = 0; k < 5; k++) {
    int x, y; btlSwitchCell(k, &x, &y);
    if (btlSwitchAt(x + 80, y + 27) == (int)k) seen++;
  }
  ck(seen == 5, "each option answers a tap at its own centre");
  int x4, y4; btlSwitchCell(4, &x4, &y4);
  ck(y4 + 54 < 270 && x4 > 69 && x4 + 160 < 69 + 328, "the fifth sits centred above the grid");
  bool overlap = false;
  for (int16_t y = 190; y < 400; y += 2)
    for (int16_t x = 40; x < 430; x += 2) {
      int k = btlSwitchAt(x, y), c = btlCellIndexAt(x, y);
      if (k == 4 && c >= 0) overlap = true;        // no pixel answers twice
    }
  ck(!overlap, "and no pixel belongs to both it and a grid cell");
  // switch to each in turn, through the screen
  bool all = true;
  for (int pass = 0; pass < 5; pass++) {
    clearMsgs();
    btlMenu = 2;
    uint8_t n = btlSwitchOpts(opt);
    uint8_t want = opt[n - 1];          // the last option: the fifth cell
    int x, y; btlSwitchCell(n - 1, &x, &y);
    battleTap(x + 80, y + 27);
    if (btlSquadAt != want) all = false;
  }
  ck(all, "tapping the fifth option really switches to it");
  battleOpen = false;

  // --- 2. STRUGGLE for a creature that can choose nothing -------------------------
  {
    Combatant abra; memset(&abra, 0, sizeof(abra));
    ck(true, "(an ABRA knows no move in our list)");
    Pet p; p.dbgHatchAs(63, false); p.ageMinutes = 9UL * MINUTES_PER_LEVEL; p.relearnFromLevel();
    combatantFromPet(abra, p);
    bool none = true;
    for (int i = 0; i < MOVE_SLOTS; i++) if (abra.moves[i]) none = false;
    ck(none, "(and so has no move at all)");
    ck(battleMustStruggle(abra), "a creature with no move must STRUGGLE");
    ck(battleMoveForSlot(abra, 0) == MV_STRUGGLE && battleMoveForSlot(abra, 3) == MV_STRUGGLE,
       "every slot means STRUGGLE for it");

    startBattle(10, 30);
    btlYou = abra; btlSquad[btlSquadAt] = abra;
    clearMsgs();
    btlMenu = 1;
    uint16_t foeHp = btlFoe.hp;
    battleTap(69 + 80, 270 + 27);       // move cell 0
    ck(said("STRUGGLE") || btlFoe.hp < foeHp, "the move screen offers STRUGGLE and it is used");
    battleOpen = false;
  }

  // --- 3. the scene is fixed for the fight -------------------------------------------
  {
    strongPet();
    for (uint8_t i = 0; i < 13; i++) pet.winBadge(0, i, false);
    startTrainerBattle(0, false);       // Brock: GEODUDE then ONIX
    int16_t scene = btlSceneDex;
    ck(scene == btlFoe.dex, "a trainer fight's scene is its lead's");
    btlFoe.hp = 1;
    btlMenu = 1;
    battleTap(69 + 80, 270 + 27);
    clearMsgs();
    ck(btlFoeAt == 1, "(Brock sent his second)");
    ck(btlSceneDex == scene, "and the scene did not change with him");
    battleOpen = false;
  }

  // --- 4. the host decides who comes on, and ships it --------------------------------
  lan.begin(true, "HOST");
  snprintf(lan.peerName, sizeof(lan.peerName), "%s", "RIVAL");
  lan.addMon(mon(150, 70, "MEW2"));
  lan.addMon(mon(143, 70, "SNORE"));
  lan.theirs[0] = mon(10, 5, "CAT0");
  lan.theirs[1] = mon(11, 5, "CAT1");
  lan.theirs[2] = mon(12, 5, "CAT2");
  lan.theirsN = 3;
  lan.state = LINK_READY;
  lan.send = capture;
  startLinkBattle();
  ck(btlSceneDex == btlSquad[0].dex, "a LAN fight's scene is the host's lead, on the host");
  // the rival switched to slot 2 earlier, and slot 1 fainted before that
  btlFoeSquad[1].hp = 0;
  btlFoeSquad[btlFoeAt] = btlFoe;
  btlFoeAt = 2; btlFoe = btlFoeSquad[2];
  btlFoe.hp = 1;
  btlMenu = 1; btlMsgCount = 0; btlSwapDue = 0;
  battleTap(69 + 80, 270 + 27);          // our move, latched
  lan.pendingAct = LINK_ACT_MOVE(0);
  render();                              // resolves
  LinkResult r{};
  ck(lastResult(r), "(the host shipped the turn)");
  ck(r.guestHp == 0 && r.guestNext == 0,
     "the fainted rival's replacement is the next STANDING one (wrapping to 0), in the result");
  ck(r.hostNext == LINK_NO_NEXT, "and our side, standing, has no replacement");
  ck(btlFoeAt == 2 && (btlSwapDue & 2), "the host shows the faint first");
  clearMsgs();
  ck(btlFoeAt == 0 && btlFoe.dex == 10, "then brings on exactly what it shipped");

  // end-of-turn damage is in the shipped result
  clearMsgs();
  btlFoe.ailment = AIL_POISON;
  btlFoe.hp = 1;
  btlMyAct = 0; lan.pendingAct = 0;
  btlSquad[btlSquadAt] = btlYou;
  btlSwitchOpts(opt);
  // a turn where we only switch (no attack), so only the poison tick can hurt it
  btlMenu = 2;
  { int x, y; btlSwitchCell(0, &x, &y); battleTap(x + 80, y + 27); }
  lan.pendingAct = LINK_ACT_SWITCH_TO(0);   // they ask for the one already out: ignored
  // their action is a switch to the active slot, so they do nothing either
  render();
  ck(lastResult(r) && r.guestHp == 0, "a poison tick's faint is in the result the guest gets");
  ck(r.guestNext == LINK_NO_NEXT && btlOver && btlWon,
     "and with nobody left the host ends the fight");
  battleOpen = false;

  // --- 5. the guest brings on what the host said ----------------------------------------
  lan.begin(false, "GUEST");
  snprintf(lan.peerName, sizeof(lan.peerName), "%s", "HOST");
  lan.addMon(mon(10, 5, "CAT0"));
  lan.addMon(mon(11, 5, "CAT1"));
  lan.addMon(mon(12, 5, "CAT2"));
  lan.theirs[0] = mon(150, 70, "MEW2");
  lan.theirs[1] = mon(143, 70, "SNORE");
  lan.theirsN = 2;
  lan.state = LINK_READY;
  startLinkBattle();
  ck(btlSceneDex == btlFoeSquad[0].dex, "and the guest shows the same scene, the host's lead");
  memset(&r, 0, sizeof(r));
  r.hostHp = btlFoe.maxHp; r.guestHp = 0; r.hostMove = btlFoe.moves[0];
  r.hostIdx = 0; r.guestIdx = 0;
  r.hostNext = LINK_NO_NEXT; r.guestNext = 2; r.guestNextHp = 9; r.guestNextAil = AIL_PARA;
  memcpy(lan.result, &r, sizeof(r)); lan.resultN = sizeof(r); lan.resultNew = true;
  render();
  ck(btlYou.fainted() && (btlSwapDue & 1), "the guest shows its creature fainting");
  clearMsgs();
  ck(btlSquadAt == 2 && btlYou.hp == 9 && btlYou.ailment == AIL_PARA,
     "then brings on the host's choice, with the host's numbers");
  // the last one down, and the host's END lost on the way: the result alone ends it
  memset(&r, 0, sizeof(r));
  r.hostHp = btlFoe.maxHp; r.guestHp = 0; r.hostMove = btlFoe.moves[0];
  r.hostIdx = 0; r.guestIdx = btlSquadAt;
  r.hostNext = LINK_NO_NEXT; r.guestNext = LINK_NO_NEXT;
  btlSquad[0].hp = 0; btlSquad[1].hp = 0;
  lan.state = LINK_READY;
  memcpy(lan.result, &r, sizeof(r)); lan.resultN = sizeof(r); lan.resultNew = true;
  render();
  ck(btlOver && !btlWon && lan.state == LINK_DONE,
     "a result with nobody left ends the fight even if the END packet is lost");
  battleOpen = false;

  // --- 6. not LAN only: after a switch, the next standing one, not the next index -----
  {
    strongPet();
    startBattle(19, 30);                // a RATTATA: weak enough to switch through it
    btlMenu = 2;
    btlSwitchOpts(opt);
    uint8_t last = opt[4];              // slot 5
    int x, y; btlSwitchCell(4, &x, &y);
    battleTap(x + 80, y + 27);
    clearMsgs();
    if (btlSquadAt == last && !btlOver) {
      btlYou.hp = 1;
      btlMenu = 1;
      battleTap(69 + 80, 270 + 27);
      ck(!btlOver && (btlSwapDue & 1), "the LAST slot fainting is not the end while others stand");
      ck(btlNextAt[0] != last && !btlSquad[btlNextAt[0]].fainted(),
         "and the one coming on is standing");
    } else {
      ck(false, "(could not reach the last slot)");
    }
    battleOpen = false;
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
