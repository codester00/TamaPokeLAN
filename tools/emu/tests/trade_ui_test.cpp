// Trading as the sketch actually wires it: the LAN screen's TRADE button, the
// picker, ACCEPT, the stuck screen, the REVERT/KEEP decision, the locks, the
// reboot that lands back on a pending trade, and the board under it all
// (party + box + the "trade" NVS key). trade_test proves the protocol between
// two test boards; this proves the FIRMWARE is one of them.
//
// Board A is the real sketch. Board B is a Trade over a test board, and the
// two talk through the emulator's stand-in radio (emuRadioAttach/Deliver).
// Every tap lands on a point asked of the firmware's own hit tests -- never on
// coordinates retyped here (CLAUDE.md § 3, "a test that keeps its own copy of
// a LAYOUT").
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "trade.h"
#include "save.h"
#include "dex.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>
#include <thread>
#include <chrono>

uint32_t g_seed = 0x7AD3;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void loop();
void render();
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
void focusSwap(uint8_t slot);
bool monSheetConfirmTap(int16_t x, int16_t y, bool fromBox);
void cordPickTap(int16_t x, int16_t y);
void partyTap(int16_t x, int16_t y);
extern uint8_t partyDetail;
extern bool releaseConfirm, cordPick, partyPick;
uint8_t uiCurrentScreen();
void tradeBoot();
bool tradeLocked(const PartyMon &m);
bool tradePending();
bool lanTradeHit(int16_t x, int16_t y);
bool tradeGiveHit(int16_t x, int16_t y);
bool tradeAcceptHit(int16_t x, int16_t y);
bool tradeRevertHit(int16_t x, int16_t y);
bool tradeKeepHit(int16_t x, int16_t y);
bool tradeCloseHit(int16_t x, int16_t y);
bool tradeSureHit(int16_t x, int16_t y);
bool tradeBackHit(int16_t x, int16_t y);
bool tradeNotSureHit(int16_t x, int16_t y);
bool partyHubAt(int16_t x, int16_t y);
void partySlotPos(int i, int *cx, int *cy);
void emuRadioAttach(void (*air)(const uint8_t *, uint8_t));
void emuRadioDeliver(const uint8_t *b, uint8_t n);
void emuSetTimeScale(uint32_t s);
void linkNowEnd();
extern Pet pet;
extern Trade trade;
extern bool tradeOpen, tradePicking, lanOpen, tradeNoRadio;
extern uint8_t tradeAsk, tradePickSrc;
extern const char *const SCREEN_NAME[];

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

// ---- board B -----------------------------------------------------------------
struct BBoard : TradeBoard {
  PartyMon slots[6];
  std::vector<uint8_t> journal;
  uint32_t t = 0;
  void mac(uint8_t out[6]) override { const uint8_t m[6] = { 0xB0, 1, 2, 3, 4, 5 }; memcpy(out, m, 6); }
  uint32_t epoch() override { return 1700000000u + t; }
  int where(const CreatureUid &u) {
    for (int i = 0; i < 6; i++) if (!slots[i].empty() && slots[i].uid == u) return i;
    return -1;
  }
  bool findMon(const CreatureUid &u, PartyMon *out) override {
    int i = where(u);
    if (i < 0) return false;
    if (out) *out = slots[i];
    return true;
  }
  bool replaceMon(const CreatureUid &o, const PartyMon &in) override {
    int i = where(o);
    if (i < 0) return false;
    slots[i] = in;
    return true;
  }
  bool storeNew(const PartyMon &in) override {
    for (auto &s : slots) if (s.empty()) { s = in; return true; }
    return false;
  }
  bool saveJournal(const uint8_t *b, size_t n) override { journal.assign(b, b + n); return true; }
  size_t loadJournal(uint8_t *b, size_t cap) override {
    if (journal.empty() || journal.size() > cap) return 0;
    memcpy(b, journal.data(), journal.size());
    return journal.size();
  }
};
static BBoard B;
static Trade tb;
static bool bAlive = true, linkAB = true, linkBA = true;
static std::deque<std::vector<uint8_t>> toA, toB;

static void airFromA(const uint8_t *b, uint8_t n) { if (linkAB) toB.emplace_back(b, b + n); }
static void airFromB(void *, const uint8_t *b, uint8_t n) { if (linkBA && bAlive) toA.emplace_back(b, b + n); }

// One beat of both boards. The sketch's own loop() pumps board A -- that is
// what puts tradePoll() on the main loop -- and B is ticked on the same clock.
static void pump(int beats) {
  for (int i = 0; i < beats; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    while (!toB.empty()) {
      auto p = toB.front();
      toB.pop_front();
      if (bAlive) tb.onPacket(p.data(), (uint8_t)p.size());
    }
    while (!toA.empty()) {
      auto p = toA.front();
      toA.pop_front();
      emuRadioDeliver(p.data(), (uint8_t)p.size());
    }
    loop();
    if (bAlive) tb.tick(millis());
  }
}
template <class F> static bool pumpUntil(F f, int beats = 3000) {
  for (int i = 0; i < beats; i++) {
    if (f()) return true;
    pump(1);
  }
  return f();
}

// The middle of whatever region a firmware hit test answers true for.
static bool centre(bool (*hit)(int16_t, int16_t), int *x, int *y) {
  long sx = 0, sy = 0, n = 0;
  for (int yy = 0; yy < 466; yy += 2)
    for (int xx = 0; xx < 466; xx += 2)
      if (hit((int16_t)xx, (int16_t)yy)) { sx += xx; sy += yy; n++; }
  if (!n) return false;
  *x = (int)(sx / n);
  *y = (int)(sy / n);
  return true;
}
static bool tapHit(bool (*hit)(int16_t, int16_t)) {
  int x, y;
  if (!centre(hit, &x, &y)) return false;
  onTap((int16_t)x, (int16_t)y);
  render();
  return true;
}
static bool hubHit(int16_t x, int16_t y) { return partyHubAt(x, y); }
static void tapSlot(int i) {
  int x, y;
  partySlotPos(i, &x, &y);
  onTap((int16_t)x, (int16_t)y);
  render();
}

static PartyMon mkMon(int16_t dex, uint8_t lvl) {
  PartyMon m;
  m.dex = dex;
  m.level = lvl;
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
  m.stateVersion = 1;
  m.fullness = m.joy = m.energy = 90;
  m.hygiene = 100;
  m.weight = 25;
  m.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  return m;
}

static void bootB() {
  toA.clear();
  toB.clear();
  tb = Trade();
  tb.send = airFromB;
  tb.begin(&B, "GARY", (uint32_t)random(1, 0x7FFFFFFF));
  tb.start();
}
static void openTradeFromLan() {
  lanOpen = true;
  render();
  tapHit(lanTradeHit);
}
static bool inParty(const CreatureUid &u) {
  for (auto &s : party.slots) if (!s.empty() && s.uid == u) return true;
  for (auto &s : party.box) if (!s.empty() && s.uid == u) return true;
  return false;
}

// A trade pending on A (the firmware) and on nobody else: A hears B's ACCEPT,
// B never hears A's, then B goes away. Exactly the stuck case the owner's
// screen is for.
static void makePending(const PartyMon &mine, const PartyMon &theirs) {
  bAlive = linkAB = linkBA = true;
  bootB();
  openTradeFromLan();
  pumpUntil([] { return trade.state == TRADE_CHOOSING && tb.state == TRADE_CHOOSING; });
  trade.offer(mine.uid);
  tb.offer(theirs.uid);
  pumpUntil([] { return trade.theirsOk && tb.theirsOk; });
  linkAB = false;               // B will never hear A again
  tb.accept();
  pumpUntil([] { return trade.theirAccept != 0; });
  tapHit(tradeAcceptHit);
  pumpUntil([] { return tradePending(); }, 200);
  bAlive = false;               // and B is switched off
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  emuSetTimeScale(20);

  // A's creatures: a Kadabra in the party, a Pikachu in box page 2, a Rattata.
  PartyMon kad = mkMon(64, 30);   kad.uid = pet.mintUid();
  PartyMon pik = mkMon(25, 20);   pik.uid = pet.mintUid();
  PartyMon rat = mkMon(19, 12);   rat.uid = pet.mintUid();
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = PartyMon();
  for (int i = 0; i < BOX_SLOTS; i++) party.box[i] = PartyMon();
  party.slots[0] = kad;
  party.slots[1] = rat;
  party.box[7] = pik;           // box page 2, ring slot 1
  party.save();
  party.boxSave();
  // B's: a Machoke and an Oddish, with B's own IDs.
  PartyMon mac = mkMon(67, 31), odd = mkMon(43, 15), eev = mkMon(133, 10);
  const uint8_t bm[6] = { 0xB0, 1, 2, 3, 4, 5 };
  memcpy(mac.uid.mac, bm, 6); mac.uid.serial = 1;
  memcpy(odd.uid.mac, bm, 6); odd.uid.serial = 2;
  memcpy(eev.uid.mac, bm, 6); eev.uid.serial = 3;
  B.slots[0] = mac;
  B.slots[1] = odd;
  B.slots[2] = eev;

  // --- 0. the journal is in the backup
  bool inBackup = false;
  for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++) inBackup |= !strcmp(SAVE_FIELDS[i].key, "trade");
  ck(inBackup, "the trade journal is one of the keys EXPORT carries");

  // --- 1. no radio: the screen says so instead of hanging
  emuRadioAttach(nullptr);
  openTradeFromLan();
  ck(tradeOpen && !lanOpen && !strcmp(SCREEN_NAME[uiCurrentScreen()], "trade"),
     "TRADE on the LAN screen opens the trade screen");
  ck(tradeNoRadio, "with no radio it knows it has none");
  onSwipeV(+1);
  ck(!tradeOpen, "and a swipe down leaves");

  // --- 2. a whole trade, through the screens
  emuRadioAttach(airFromA);
  bootB();
  openTradeFromLan();
  ck(trade.state == TRADE_LISTENING && !tradeNoRadio, "with a radio it is looking for a trainer");
  ck(pumpUntil([] { return trade.state == TRADE_CHOOSING && tb.state == TRADE_CHOOSING; }),
     "the two boards find each other");
  ck(!strcmp(trade.peerName, "GARY"), "and it knows who the other trainer is");
  render();
  ck(tapHit(tradeGiveHit) && tradePicking, "tapping YOU GIVE opens the picker");
  tapHit(hubHit);
  tapHit(hubHit);
  ck(tradePickSrc == 2, "the hub pages party -> box 1 -> box 2");
  tapSlot(1);
  ck(!tradePicking && trade.mine.uid == pik.uid, "a box creature can be offered");
  tapHit(tradeGiveHit);
  tapHit(hubHit); tapHit(hubHit); tapHit(hubHit); tapHit(hubHit);
  ck(tradePickSrc == 0, "and round again to the party");
  tapSlot(0);
  ck(trade.mine.uid == kad.uid, "the offer can be changed before accepting");
  tb.offer(mac.uid);
  ck(pumpUntil([] { return trade.theirsOk; }), "their Machoke arrives on A's screen");
  tapHit(tradeAcceptHit);
  ck(trade.iAccepted, "ACCEPT accepts");
  tapHit(tradeAcceptHit);
  ck(!trade.iAccepted, "and tapping it again takes it back");
  tapHit(tradeAcceptHit);
  tb.accept();
  ck(pumpUntil([] { return trade.state == TRADE_DONE && tb.state == TRADE_DONE; }), "both accept: traded");
  ck(party.slots[0].uid == mac.uid && !inParty(kad.uid), "A now holds the Machoke where the Kadabra was");
  ck(B.where(kad.uid) >= 0 && B.where(mac.uid) < 0, "and B the Kadabra");
  ck(party.slots[0].flags & PMON_TRADE_EVO, "the Machoke arrived marked to evolve");
  Party reread;
  reread.begin();
  ck(reread.slots[0].uid == mac.uid, "the party write landed in NVS, not just in RAM");
  pump(60);
  ck(!trade.hasOpenEntries() && !tb.hasOpenEntries(), "both boards confirmed it");
  render();
  ck(tapHit(tradeCloseHit) && !tradeOpen, "CLOSE leaves the finished trade");

  // --- 3. the art gate: a creature this board could only draw as a number
  {
    bootB();
    openTradeFromLan();
    pumpUntil([] { return trade.state == TRADE_CHOOSING && tb.state == TRADE_CHOOSING; });
    PartyMon noArt = mkMon(616, 20);   // SHELMET has no sprite upstream (noart.h)
    memcpy(noArt.uid.mac, bm, 6); noArt.uid.serial = 9;
    B.slots[3] = noArt;
    tb.offer(noArt.uid);
    pumpUntil([] { return trade.theirs.dex == 616; });
    ck(!trade.theirsOk && trade.theirsWhy == 2, "a creature with no art is refused before anyone accepts");
    B.slots[3] = PartyMon();
    onSwipeV(+1);
    ck(!tradeOpen, "(left)");
  }

  // --- 4. stuck: cannot leave, the creature is locked, a reboot comes back
  //        here, and after five minutes REVERT puts everything back
  makePending(rat, odd);
  ck(tradePending() && tradeOpen, "(A is pending and B has gone)");
  render();
  tapHit(tradeBackHit);
  onSwipeV(+1);
  ck(tradeOpen, "a pending trade cannot be left, by tap or swipe");
  ck(tradeLocked(rat), "the offered creature is locked");
  const int16_t live = pet.speciesId;
  focusSwap(1);
  ck(pet.speciesId == live && party.slots[1].uid == rat.uid, "and cannot be raised out from under the trade");
  {
    // Released from its sheet: the YES of the release confirm.
    partyDetail = 2;
    int x, y;
    centre(tradeSureHit, &x, &y);   // the same confirm rectangle
    monSheetConfirmTap((int16_t)x, (int16_t)y, false);
    ck(party.slots[1].uid == rat.uid, "nor released");
    partyDetail = 0;
    releaseConfirm = false;
    // The Linking Cord's partner pick, and a farewell replacing a full party's slot.
    int sx, sy;
    partySlotPos(1, &sx, &sy);
    cordPick = true;
    cordPickTap((int16_t)sx, (int16_t)sy);
    ck(cordPick && party.slots[1].uid == rat.uid, "nor linked by a Linking Cord");
    cordPick = false;
    partyPick = true;
    pet.endedMon = mkMon(10, 5);
    partyTap((int16_t)sx, (int16_t)sy);
    ck(party.slots[1].uid == rat.uid, "nor overwritten by a creature looking for a slot");
    partyPick = false;
    pet.endedMon = PartyMon();
  }
  ck(trade.status().connected, "right after the other board goes quiet it still counts as connected");
  ck(pumpUntil([] { return !trade.status().connected; }, 20000), "until its silence passes the peer timeout");
  render();
  TradeStatus st = trade.status();
  ck(st.view == TV_INCOMPLETE && st.have.uid == rat.uid && st.secsLeft > 0 && !st.connected,
     "the stuck screen: incomplete, you have the Rattata, counting down");
  // A reboot: the screen is gone, the radio off -- and boot puts it back.
  tradeOpen = false;
  linkNowEnd();
  tradeBoot();
  ck(tradeOpen && tradePending(), "a reboot with a trade pending lands straight back on it");
  ck(!tapHit(tradeRevertHit) || !tradeAsk, "nothing to decide inside the five minutes");
  emuSetTimeScale(4000);
  ck(pumpUntil([] { return trade.canDecide(); }, 20000), "five minutes later the player is asked");
  emuSetTimeScale(20);
  ck(trade.state != TRADE_CANCELLED && trade.status().mustDecide, "(still calling, not given up, when asked)");
  render();
  tapHit(tradeRevertHit);
  ck(tradeAsk == 1, "REVERT asks first: it cannot be undone");
  tapHit(tradeNotSureHit);
  ck(tradeAsk == 0 && tradePending(), "NO backs out of the warning with nothing done");
  tapHit(tradeKeepHit);
  ck(tradeAsk == 2, "KEEP asks too");
  onSwipeV(+1);
  ck(tradeAsk == 0 && tradeOpen, "and a swipe down backs out of the warning, not the trade");
  tapHit(tradeRevertHit);
  tapHit(tradeSureHit);
  ck(!tradePending() && party.slots[1].uid == rat.uid && !inParty(odd.uid),
     "REVERT: the Rattata stays, the Oddish never arrives");
  ck(!tradeLocked(rat), "and it is unlocked");
  ck(tapHit(tradeCloseHit) && !tradeOpen, "and now the screen can be left");

  // --- 5. KEEP, with the party write failing: RAM must not claim what NVS
  //        does not hold, or the retry stops and a reboot loses the trade
  bAlive = true;
  makePending(rat, odd);
  emuSetTimeScale(4000);
  pumpUntil([] { return trade.canDecide(); }, 20000);
  emuSetTimeScale(20);
  ck(trade.canDecide(), "(asked again)");
  nvsFailWritesAfter(1);          // the journal's commit lands; the party write does not
  ck(trade.decideKeep(), "KEEP commits");
  ck(party.slots[1].uid == rat.uid, "a party write that failed leaves the slot as NVS has it");
  nvsResumeWrites();
  pump(20);
  ck(party.slots[1].uid == odd.uid && !inParty(rat.uid), "and the retry lands it");
  Party again;
  again.begin();
  ck(again.slots[1].uid == odd.uid, "in NVS too");
  ck(!trade.j.active, "the journal is clear");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
