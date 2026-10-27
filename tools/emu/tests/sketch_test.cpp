// SKETCH (v3.69): Smeargle's whole movepool. It turns its own slot into the
// foe's last move for good, so this checks the copy rule in the engine, that
// the copy is SAVED on the creature through a real fight (the first time a
// fight changes a creature at all), the extra slots by level and in the move
// picker, PP kept per slot, the LAN path both ways -- the host resolving the
// guest's SKETCH and the guest keeping it, and only over a SKETCH -- and the
// random moveset a wild Smeargle meets you with.
#include <cstring>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "pp.h"
#include "link.h"
#include <cstdio>
uint32_t g_seed = 0x5E7C; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void onTap(int16_t x, int16_t y);
void startBattle(int16_t dex, uint8_t lvl);
void startLinkBattle();
bool sketchKeep(const CreatureUid &uid, uint8_t slot, uint8_t to);
int movePickRowY(int i);
uint8_t learnableList(uint8_t *out, uint8_t max);
extern Pet pet;
extern Link lan;
extern bool battleOpen, btlOver, movePickOpen, cardOpen;
extern Combatant btlYou, btlFoe, btlSquad[], btlFoeSquad[];
extern uint8_t btlMenu, btlMsgCount, btlMyAct, btlSwapDue, btlSquadAt;
extern uint8_t movePickSlot, movePickParty, movePickPage;
extern char btlMsg[8][40];
extern uint16_t squadMask;
extern CreatureUid lanMineUid[];

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint8_t lastPkt[256]; static uint8_t lastLen = 0;
static void capture(void *, const uint8_t *b, uint8_t n) {
  if (b[0] == LM_RESULT) { memcpy(lastPkt, b, n); lastLen = n; }
}
static bool lastResult(LinkResult &r) {
  if (lastLen < 3 + sizeof(LinkResult) || lastPkt[0] != LM_RESULT) return false;
  memcpy(&r, lastPkt + 3, sizeof(r));
  return true;
}
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) battleTap(233, 300); }
static bool said(const char *w) {
  for (uint8_t i = 0; i < btlMsgCount; i++) if (strstr(btlMsg[i], w)) return true;
  return false;
}
static LinkMon monOf(const Pet &p, const char *nm) {
  Combatant c; combatantFromPet(c, p);
  snprintf(c.name, sizeof(c.name), "%s", nm);
  LinkMon m; linkMonFrom(m, c); return m;
}
// the live pet as a level-`lvl` SMEARGLE holding `moves`
static void smeargle(uint8_t lvl, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  pet.dbgHatchAs(235, false);
  pet.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  pet.lastLearnLevel = lvl;
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.moves[0] = a; pet.moves[1] = b; pet.moves[2] = c; pet.moves[3] = d;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  if (!pet.uid.valid()) pet.uid = pet.mintUid();
  pet.saveNow();
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the copy, in the engine -------------------------------------------------
  {
    Combatant a, b;
    a.dex = 235; a.level = 30; a.maxHp = a.hp = 80;
    b.dex = 19;  b.level = 30; b.maxHp = b.hp = 80;
    a.moves[0] = MV_SKETCH; a.moves[1] = MV_SKETCH; a.moves[2] = MV_TACKLE;
    b.moves[0] = MV_FLAMETHROWER;
    battleFullPp(a); battleFullPp(b);
    TurnLog lg;
    battleAct(a, b, MV_SKETCH, lg);
    ck(lg.failed && !lg.sketched && a.moves[0] == MV_SKETCH && a.moves[1] == MV_SKETCH,
       "a SKETCH before the foe has moved fails");
    ck(a.pp[0] == 0 && a.pp[1] == 1, "and spends that one slot's PP");
    battleAct(b, a, MV_FLAMETHROWER, lg);
    ck(b.lastMove == MV_FLAMETHROWER, "a creature remembers the move it used");
    battleAct(a, b, MV_SKETCH, lg);
    ck(lg.sketched == MV_FLAMETHROWER && lg.sketchSlot == 1 && a.moves[1] == MV_FLAMETHROWER,
       "the next SKETCH -- from the slot with PP left -- becomes the foe's last move");
    ck(a.pp[1] == ppMax(MV_FLAMETHROWER), "with that move's full PP");
    a.pp[0] = 1;
    battleAct(a, b, MV_SKETCH, lg);
    ck(lg.failed && a.moves[0] == MV_SKETCH, "a move it already knows cannot be copied again");
    b.lastMove = MV_STRUGGLE;
    ck(!battleSketchTarget(a, b), "STRUGGLE cannot be copied");
    b.lastMove = MV_SKETCH;
    ck(!battleSketchTarget(a, b), "nor SKETCH itself");
    b.moves[0] = MV_TACKLE; b.pp[0] = 1;
    b.lastMove = 0;
    battleAct(b, a, MV_TACKLE, lg);
    ck(b.lastMove == MV_TACKLE, "a move that misses or hits is remembered all the same");

    // the AI does not waste a turn on a SKETCH with nothing to copy
    Combatant s; s.dex = 235; s.level = 30; s.maxHp = s.hp = 80;
    s.moves[0] = MV_SKETCH; s.moves[1] = MV_TACKLE; battleFullPp(s);
    Combatant f = b; f.lastMove = 0;
    int sk = 0;
    for (int i = 0; i < 50; i++) if (aiChooseMove(s, f, true) == MV_SKETCH) sk++;
    ck(sk == 0, "the smart AI never SKETCHes when the foe has not moved");
  }

  // --- 2. learning SKETCH by level, as the games do ----------------------------------
  // A SKETCH row at 1, 11 ... 91. At each: nothing if it still knows SKETCH (no
  // creature learns a move it knows), into an empty slot if there is one, and
  // otherwise the "forget a move?" offer -- the same rule as every level-up move.
  ck(learnsSketch(235) && !learnsSketch(19), "only Smeargle learns it");
  {
    uint8_t rows = 0;
    for (uint8_t i = 0; i < learnCount(235); i++)
      if (learnMove(235, i) == MV_SKETCH && learnLevel(235, i) == 1 + 10 * rows) rows++;
    ck(rows == 10, "Smeargle's learnset has SKETCH at 1, 11, 21 ... 91");
  }
  auto toLevel = [](uint8_t lvl) {
    pet.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
    pet.checkLearnGates();
  };
  smeargle(10, MV_SKETCH, 0, 0, 0);
  toLevel(11);
  ck(pet.moves[0] == MV_SKETCH && !pet.moves[1] && !pet.hasLearnOffer(),
     "at 11, still knowing SKETCH, it learns nothing (one SKETCH at a time)");
  smeargle(20, MV_FLAMETHROWER, 0, 0, 0);
  toLevel(21);
  ck(pet.moves[1] == MV_SKETCH && !pet.hasLearnOffer(),
     "at 21, its SKETCH used, a new one goes into an empty slot by itself");
  smeargle(30, MV_FLAMETHROWER, MV_SURF, MV_THUNDERBOLT, MV_ICE_BEAM);
  toLevel(31);
  ck(pet.hasLearnOffer() && pet.learnOffer() == MV_SKETCH,
     "at 31 with all four slots full, the trainer is offered SKETCH");
  pet.acceptLearn(2);
  ck(pet.moves[2] == MV_SKETCH && pet.moves[0] == MV_FLAMETHROWER, "and forgets the move chosen for it");
  smeargle(40, MV_FLAMETHROWER, MV_SURF, MV_THUNDERBOLT, MV_ICE_BEAM);
  toLevel(41);
  pet.declineLearn();
  ck(!pet.hasLearnOffer() && pet.moves[3] == MV_ICE_BEAM, "or declines and keeps all four");
  // a Smeargle from before v3.69: nothing, past every gate
  smeargle(35, 0, 0, 0, 0);
  pet.checkLearnGates();
  ck(pet.moves[0] == MV_SKETCH && !pet.moves[1], "a moveless level 35 Smeargle gets its one SKETCH");
  // the same rule for every creature: Charmander's own first two gates
  {
    uint8_t g[2] = { 0, 0 }, mv[2] = { 0, 0 }, k = 0;
    for (uint8_t i = 0; i < learnCount(4) && k < 2; i++)
      if (learnLevel(4, i) > 1) { g[k] = learnLevel(4, i); mv[k++] = learnMove(4, i); }
    pet.dbgHatchAs(4, false);
    pet.ageMinutes = (uint32_t)(g[0] - 2) * MINUTES_PER_LEVEL;
    pet.lastLearnLevel = (uint8_t)(g[0] - 1);
    while (pet.hasLearnOffer()) pet.declineLearn();   // what hatching at the old age queued
    for (int i = 0; i < MOVE_SLOTS; i++) pet.moves[i] = 0;
    pet.moves[0] = MV_SURF;
    toLevel(g[0]);
    ck(pet.moves[1] == mv[0] && !pet.hasLearnOffer(), "any creature: a new move fills an empty slot by itself");
    pet.moves[2] = MV_THUNDERBOLT; pet.moves[3] = MV_ICE_BEAM;
    toLevel(g[1]);
    ck(pet.hasLearnOffer() && pet.learnOffer() == mv[1], "and with no slot free, the trainer is asked");
    pet.declineLearn();
  }

  // --- 3. PP is kept per slot ---------------------------------------------------------
  {
    uint8_t mv[MOVE_SLOTS] = { MV_SKETCH, MV_SKETCH, MV_SKETCH, MV_SKETCH };
    uint8_t left[MOVE_SLOTS] = { 0, 1, 1, 1 };
    ppRecord(pet.uid, mv, left);
    ck(ppLeftSlot(pet.uid, 0, MV_SKETCH) == 0 && ppLeftSlot(pet.uid, 1, MV_SKETCH) == 1,
       "one spent SKETCH does not empty the other three");
    uint8_t full[MOVE_SLOTS] = { 1, 1, 1, 1 };
    ppRecord(pet.uid, mv, full);
  }

  // --- 4. a real fight: the copy is kept on the live pet, and saved ----------------------
  smeargle(35, MV_SKETCH, MV_SKETCH, MV_SKETCH, MV_SKETCH);
  squadMask = 1;
  startBattle(10, 2);                   // a level 2 CATERPIE: slower, and harmless
  btlFoe.lastMove = MV_FLAMETHROWER;    // what it "used" last turn
  btlMenu = 1; btlMsgCount = 0;
  battleTap(69 + 80, 270 + 27);         // the first cell: SKETCH
  ck(btlYou.moves[0] == MV_FLAMETHROWER, "in a fight, SKETCH becomes the foe's last move");
  ck(said("sketched"), "and the fight says so");
  ck(pet.moves[0] == MV_FLAMETHROWER && pet.moves[1] == MV_SKETCH,
     "the live pet keeps it, in that slot only");
  { Pet r; r.begin(); ck(r.moves[0] == MV_FLAMETHROWER, "and it is saved: it survives a reload"); }
  clearMsgs();
  battleOpen = false;

  // --- 5. a party member keeps it too; nothing else can be changed --------------------
  {
    PartyMon m; m.dex = 235; m.level = 40;
    m.moves[0] = MV_SKETCH; m.moves[1] = MV_SURF;
    m.uid = pet.mintUid();
    party.replaceAt(0, m);
    ck(sketchKeep(m.uid, 0, MV_THUNDERBOLT) && party.slots[0].moves[0] == MV_THUNDERBOLT,
       "a party member's SKETCH is kept by its ID");
    ck(!sketchKeep(m.uid, 1, MV_THUNDERBOLT) && party.slots[0].moves[1] == MV_SURF,
       "but never over a slot that is not SKETCH");
    ck(!sketchKeep(m.uid, 0, MV_ICE_BEAM), "nor twice over the same slot");
    CreatureUid nobody;
    ck(!sketchKeep(nobody, 0, MV_SURF), "and a creature with no ID (a foe) changes nothing");
    Party pr; pr.begin();
    ck(pr.slots[0].moves[0] == MV_THUNDERBOLT, "it is saved with the party");
    party.replaceAt(0, PartyMon());
  }

  // --- 6. the picker: SKETCH is one move like any other -------------------------------
  {
    smeargle(35, MV_FLAMETHROWER, MV_SKETCH, 0, 0);
    uint8_t all[64]; uint8_t n = learnableList(all, sizeof(all));
    int at = -1;
    for (uint8_t i = 0; i < n; i++) if (all[i] == MV_SKETCH) at = i;
    ck(at >= 0, "(SKETCH is in a Smeargle's picker)");
    cardOpen = true; movePickOpen = true; movePickParty = 0; movePickSlot = 0;
    movePickPage = (uint8_t)(at / 5);
    onTap(233, movePickRowY(at % 5) + 25);
    ck(pet.moves[0] == MV_SKETCH && pet.moves[1] == MV_FLAMETHROWER,
       "picking SKETCH where it already sits elsewhere swaps the two, never two SKETCH");
    cardOpen = false; movePickOpen = false;
  }

  // --- 7. LAN, the host: the guest's SKETCH is resolved and shipped --------------------
  {
    Pet host; host.dbgHatchAs(143, false); host.ageMinutes = 49UL * MINUTES_PER_LEVEL;
    host.relearnFromLevel();
    Pet guest; guest.dbgHatchAs(235, false); guest.ageMinutes = 49UL * MINUTES_PER_LEVEL;
    guest.moves[0] = MV_SKETCH; guest.moves[1] = MV_SKETCH; guest.moves[2] = 0; guest.moves[3] = 0;
    lan.begin(true, "HOST");
    snprintf(lan.peerName, sizeof(lan.peerName), "%s", "GUEST");
    lan.addMon(monOf(host, "SNORE"));
    lan.theirs[0] = monOf(guest, "SMEAR");
    lan.theirsN = 1;
    lan.state = LINK_READY;
    lan.send = capture;
    startLinkBattle();
    btlYou.lastMove = MV_BODY_SLAM;
    btlMenu = 1; btlMsgCount = 0; btlSwapDue = 0; btlMyAct = 0;
    battleTap(69 + 80, 270 + 27);          // ours, latched
    lan.pendingAct = LINK_ACT_MOVE(0);     // theirs: SKETCH
    render();
    LinkResult r{};
    ck(lastResult(r), "(the host shipped the turn)");
    uint8_t got = r.guestMoves[0];
    bool copied = got == MV_BODY_SLAM;     // the faster Smeargle copies last turn's move
    ck(copied && r.guestMoves[1] == MV_SKETCH, "the result carries the guest's new move, in that slot only");
    bool beat = false;
    for (uint8_t k = 0; k < r.beatN; k++) if (r.beats[k].who == 1 && r.beats[k].sketch == got) beat = true;
    ck(copied && beat, "and a beat that says it was sketched");
    ck(lan.theirs[0].moves[0] == got, "the host's copy of the guest's squad learns it (for a rematch)");
    ck(said("sketched"), "the host narrates it");
    battleOpen = false;

    // --- the guest: keeps it on its own creature, and only over SKETCH
    smeargle(50, MV_SKETCH, MV_SKETCH, 0, 0);
    lan.begin(false, "GUEST");
    snprintf(lan.peerName, sizeof(lan.peerName), "%s", "HOST");
    lan.addMon(monOf(pet, "SMEAR"));
    lanMineUid[0] = pet.uid;
    lan.theirs[0] = monOf(host, "SNORE");
    lan.theirsN = 1;
    lan.state = LINK_READY;
    startLinkBattle();
    LinkResult g = r;
    g.guestIdx = 0; g.hostIdx = 0;
    g.guestHp = btlYou.maxHp; g.hostHp = btlFoe.maxHp;
    g.guestNext = g.hostNext = LINK_NO_NEXT;
    g.guestMoves[0] = got; g.guestMoves[1] = MV_SKETCH;
    g.guestMoves[2] = MV_HYPER_BEAM;       // a host trying to rewrite an empty slot
    btlMsgCount = 0;
    memcpy(lan.result, &g, sizeof(g)); lan.resultN = sizeof(g); lan.resultNew = true;
    render();
    ck(btlYou.moves[0] == got && pet.moves[0] == got, "the guest keeps the move the host says it sketched");
    { Pet re; re.begin(); ck(re.moves[0] == got, "saved on the guest board"); }
    ck(pet.moves[2] == 0 && btlYou.moves[2] == 0, "a slot that was not SKETCH is never rewritten");
    ck(lan.mine[0].moves[0] == got, "and its own squad copy learns it (for a rematch)");
    ck(said("sketched"), "the guest reads the same line");
    battleOpen = false;
  }

  // --- 8. a wild Smeargle meets you with a random set ----------------------------------
  {
    bool varied = false, ok = true;
    uint8_t first[MOVE_SLOTS] = { 0 };
    for (int t = 0; t < 30; t++) {
      Pet w; w.dbgHatchAs(235, false); w.ageMinutes = 24UL * MINUTES_PER_LEVEL;
      w.rollWildMoves();
      bool attack = false;
      for (int i = 0; i < MOVE_SLOTS; i++) {
        uint8_t m = w.moves[i];
        if (!m || m == MV_SKETCH || m == MV_STRUGGLE || m >= MOVE_COUNT) ok = false;
        for (int j = 0; j < i; j++) if (w.moves[j] == m) ok = false;
        if (m && m < MOVE_COUNT && MOVE_TBL[m].cat != MC_STATUS) attack = true;
      }
      if (!attack) ok = false;
      if (!t) memcpy(first, w.moves, sizeof(first));
      else if (memcmp(first, w.moves, sizeof(first))) varied = true;
    }
    ck(ok, "four distinct real moves, at least one attack, never SKETCH or STRUGGLE");
    ck(varied, "and not the same four every time");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
