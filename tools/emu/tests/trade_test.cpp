// The trade protocol (trade.h), two boards in one process.
//
// The property under test is CONSERVATION: whatever happens to the radio or the
// power, at the end every creature that went into a trade exists at least
// once, and -- unless a player chose to give up -- EXACTLY once. Each case
// below breaks something on purpose and then checks that, rather than checking
// the happy path's particular numbers:
//
//   - every packet dropped one in three, and duplicated, over 300 seeds;
//   - the power cut at EVERY durable write either board makes, then a reboot;
//   - the link cut after EVERY packet of a trade, both ways and one way;
//   - the one-sided give-up, from every one of those states.
//
// The boards are TestBoard below -- arrays standing in for the party, the box
// and the journal key -- so a "power cut" is exact: the Nth write fails, the
// board stops, and its RAM (the Trade object) is thrown away.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "trade.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

uint32_t g_seed = 0x7EADE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

static uint32_t rng = 1;
static uint32_t rnd() { rng = rng * 1103515245u + 12345u; return rng >> 8; }

// ---- a board ------------------------------------------------------------------
struct TestBoard : TradeBoard {
  uint8_t id[6];
  uint32_t base = 1800000000u;
  uint32_t *clock = nullptr;
  uint32_t extraSecs = 0;                 // for the give-up wait
  PartyMon party[PARTY_SLOTS], box[BOX_SLOTS];
  std::vector<uint8_t> journal;
  int writes = 0;
  int cutAt = -1;                         // the write that loses power, -1 = never
  bool dead = false;
  bool refuse = false;                    // canReceive() answer, for the policy case

  bool landWrite() {
    if (dead) return false;
    if (cutAt >= 0 && writes >= cutAt) { dead = true; return false; }
    writes++;
    return true;
  }
  PartyMon *where(const CreatureUid &u) {
    for (auto &m : party) if (!m.empty() && m.uid == u) return &m;
    for (auto &m : box) if (!m.empty() && m.uid == u) return &m;
    return nullptr;
  }
  void mac(uint8_t out[6]) override { memcpy(out, id, 6); }
  uint32_t epoch() override { return base + (clock ? *clock / 1000 : 0) + extraSecs; }
  bool findMon(const CreatureUid &u, PartyMon *out) override {
    PartyMon *m = where(u);
    if (m && out) *out = *m;
    return m != nullptr;
  }
  bool replaceMon(const CreatureUid &out, const PartyMon &in) override {
    PartyMon *m = where(out);
    if (!m || !landWrite()) return false;
    *m = in;
    return true;
  }
  bool storeNew(const PartyMon &in) override {
    PartyMon *slot = nullptr;
    for (auto &m : party) if (!slot && m.empty()) slot = &m;
    for (auto &m : box) if (!slot && m.empty()) slot = &m;
    if (!slot || !landWrite()) return false;
    *slot = in;
    return true;
  }
  bool saveJournal(const uint8_t *buf, size_t n) override {
    if (!landWrite()) return false;
    journal.assign(buf, buf + n);
    return true;
  }
  size_t loadJournal(uint8_t *buf, size_t cap) override {
    if (journal.size() > cap) return 0;
    memcpy(buf, journal.data(), journal.size());
    return journal.size();
  }
  bool canReceive(const PartyMon &) override { return !refuse; }
  int count(const CreatureUid &u) {
    int n = 0;
    for (auto &m : party) n += (!m.empty() && m.uid == u);
    for (auto &m : box) n += (!m.empty() && m.uid == u);
    return n;
  }
};

static PartyMon makeMon(const uint8_t mac[6], uint32_t serial, int16_t dex) {
  PartyMon m;
  m.dex = dex;
  m.level = 30;
  m.ivAtk = 20; m.ivDef = 21; m.ivSpe = 22; m.ivHp = 23;
  m.trAtk = 10;
  m.stateVersion = 1;
  m.ageMinutes = 29 * 60;
  memcpy(m.uid.mac, mac, 6);
  m.uid.salt = 0x1234;
  m.uid.serial = serial;
  return m;
}

// ---- the radio --------------------------------------------------------------
struct Pkt { int to; std::vector<uint8_t> b; };

struct Sim {
  TestBoard A, B;
  Trade ta, tb;
  uint32_t now = 0;
  std::deque<Pkt> air;
  int dropPct = 0, dupPct = 0;
  bool upAB = true, upBA = true;          // the link, per direction
  int cutAfter = -1;                      // drop everything after this many packets
  int cutDir = 0;                         // 0 both ways, 1 only B->A
  int sent = 0;
  uint32_t nonceSeq = 1;
  PartyMon a1, a2, b1, b2;

  struct Ep { Sim *s; int from; };
  Ep epA{ this, 0 }, epB{ this, 1 };

  static void tx(void *ctx, const uint8_t *buf, uint8_t len) {
    Ep *e = (Ep *)ctx;
    Sim *s = e->s;
    if ((e->from == 0 ? s->A : s->B).dead) return;
    s->sent++;
    if (s->cutAfter >= 0 && s->sent > s->cutAfter) {
      if (s->cutDir == 0) { s->upAB = s->upBA = false; }
      else s->upBA = false;
    }
    if (e->from == 0 ? !s->upAB : !s->upBA) return;
    if (s->dropPct && (int)(rnd() % 100) < s->dropPct) return;
    Pkt p{ 1 - e->from, std::vector<uint8_t>(buf, buf + len) };
    s->air.push_back(p);
    if (s->dupPct && (int)(rnd() % 100) < s->dupPct) s->air.push_back(p);
  }

  Sim() {
    const uint8_t ma[6] = { 0xA0, 1, 2, 3, 4, 5 }, mb[6] = { 0xB0, 1, 2, 3, 4, 5 };
    memcpy(A.id, ma, 6);
    memcpy(B.id, mb, 6);
    A.clock = B.clock = &now;
    a1 = makeMon(ma, 1, 64);    // KADABRA
    a2 = makeMon(ma, 2, 25);
    b1 = makeMon(mb, 1, 67);    // MACHOKE
    b2 = makeMon(mb, 2, 133);
    A.party[0] = a1; A.box[3] = a2;
    B.party[2] = b1; B.party[4] = b2;
  }
  void boot(int side) {
    TestBoard &bd = side == 0 ? A : B;
    Trade &t = side == 0 ? ta : tb;
    bd.dead = false;
    bd.cutAt = -1;
    t = Trade();
    t.send = tx;
    t.ctx = side == 0 ? (void *)&epA : (void *)&epB;
    t.begin(&bd, side == 0 ? "ASH" : "GARY", nonceSeq++ * 2654435761u);
  }
  void session() {   // both boards open the trade screen afresh
    air.clear();
    upAB = upBA = true;
    cutAfter = -1;
    boot(0); boot(1);
    ta.start(); tb.start();
  }
  void step(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 20) {
      now += 20;
      // deliver what is in the air right now; replies queue for the next step
      size_t n = air.size();
      for (size_t i = 0; i < n && !air.empty(); i++) {
        Pkt p = air.front();
        air.pop_front();
        TestBoard &bd = p.to == 0 ? A : B;
        if (bd.dead) continue;
        (p.to == 0 ? ta : tb).onPacket(p.b.data(), (uint8_t)p.b.size());
      }
      if (!A.dead) ta.tick(now);
      if (!B.dead) tb.tick(now);
    }
  }
  bool dealt() { return ta.state == TRADE_DONE && tb.state == TRADE_DONE; }
  // Offer, wait for both offers to land, accept on both sides.
  void offerAndAccept(const PartyMon &mineA, const PartyMon &mineB) {
    ta.offer(mineA.uid);
    tb.offer(mineB.uid);
    for (int i = 0; i < 300 && !(ta.theirsOk && tb.theirsOk); i++) step(20);
    ta.accept();
    tb.accept();
  }
  int countAll(const PartyMon &m) { return A.count(m.uid) + B.count(m.uid); }
  bool clean() {
    return !ta.j.active && !tb.j.active && !ta.hasOpenEntries() && !tb.hasOpenEntries();
  }
};

// After any disaster: reboot what died, let the two meet again (twice, as a
// player might reopen the screen), and report whether the books balance.
static void settle(Sim &s) {
  s.step(TRADE_PEER_TIMEOUT_MS + 2000);   // the survivor notices
  for (int round = 0; round < 2; round++) {
    s.session();
    s.step(6000);
  }
}

int main() {
  // --- 0. the journal survives a round trip, and refuses a damaged one
  {
    TradeJournal j;
    j.active = TRADE_PENDING;
    j.fp = 0x0123456789ABCDEFULL;
    j.since = 77;
    j.outUid.serial = 9;
    j.in.dex = 6;
    j.in.uid.serial = 4;
    j.logN = 1;
    j.log[0].fp = 42;
    j.log[0].open = 1;
    uint8_t buf[TRADE_JOURNAL_MAX];
    size_t n = tradeJournalWrite(j, buf, sizeof(buf));
    TradeJournal r;
    ck(n && tradeJournalRead(r, buf, n) && r.fp == j.fp && r.in.dex == 6 &&
       r.outUid.serial == 9 && r.log[0].fp == 42 && r.log[0].open, "the journal round-trips");
    buf[20] ^= 1;
    ck(!tradeJournalRead(r, buf, n), "a damaged journal is refused, not believed");
  }

  // --- 1. a clean trade
  {
    Sim s;
    s.session();
    s.step(500);
    ck(s.ta.state == TRADE_CHOOSING && s.tb.state == TRADE_CHOOSING, "the two boards pair");
    ck(!strcmp(s.ta.peerName, "GARY"), "and learn each other's names");
    s.offerAndAccept(s.a1, s.b1);
    ck(s.ta.locks(s.a1.uid), "an offered creature is locked");
    s.step(3000);
    ck(s.dealt(), "both sides finish");
    ck(s.A.party[0].uid == s.b1.uid && s.B.party[2].uid == s.a1.uid,
       "each creature took the other's slot");
    ck(s.countAll(s.a1) == 1 && s.countAll(s.b1) == 1, "and each exists exactly once");
    ck(s.clean(), "no journal left open on either side");
    ck(!s.ta.locks(s.a1.uid), "and nothing is locked afterwards");
    ck(s.A.count(s.a2.uid) == 1 && s.B.count(s.b2.uid) == 1, "bystanders untouched");
  }

  // --- 2. changing an offer voids the acceptance
  {
    Sim t;
    t.session();
    t.step(500);
    t.ta.offer(t.a1.uid);
    t.tb.offer(t.b1.uid);
    t.step(500);
    t.ta.accept();
    t.step(500);
    ck(t.ta.state == TRADE_CHOOSING, "(one side accepting is not a deal)");
    t.tb.offer(t.b2.uid);
    t.step(500);
    ck(!t.ta.iAccepted, "a changed offer clears the other side's acceptance");
    t.tb.accept();
    t.step(1000);
    ck(t.ta.state == TRADE_CHOOSING && t.A.count(t.a1.uid) == 1,
       "so the new pair is not traded on the old acceptance");
    t.ta.accept();
    t.step(2000);
    ck(t.dealt() && t.A.count(t.b2.uid) == 1, "until both accept the new pair");
  }

  // --- 3. what gets refused
  {
    Sim s;
    s.session();
    s.step(500);
    // A broken creature on the wire: forge an offer by hand.
    PartyMon bogus = s.b1;
    bogus.moves[0] = 250;
    uint8_t pkt[2 + 1 + 60];
    pkt[0] = TM_OFFER; pkt[1] = 61; pkt[2] = 9;
    memcpy(pkt + 3, &bogus, 60);
    s.ta.onPacket(pkt, sizeof(pkt));
    ck(!s.ta.theirsOk && s.ta.theirsWhy == 1, "a creature that fails validation is refused");
    ck(!s.ta.accept(), "and cannot be accepted");
    PartyMon dup = s.b1;
    dup.uid = s.a2.uid;      // an ID A already holds
    memcpy(pkt + 3, &dup, 60);
    pkt[2] = 10;
    s.ta.onPacket(pkt, sizeof(pkt));
    ck(s.ta.theirsWhy == 3, "a creature already on this board is refused");
    s.A.refuse = true;
    s.tb.offer(s.b2.uid);
    s.step(600);
    ck(s.ta.theirsWhy == 2, "a creature this board cannot hold is refused");

    Sim v;
    v.session();
    uint8_t hello[2 + 25] = {};
    hello[0] = TM_HELLO; hello[1] = 25; hello[2] = TRADE_PROTO; hello[3] = 0xEE; hello[4] = 0xEE;
    hello[5] = 0xC0;
    v.ta.onPacket(hello, sizeof(hello));
    ck(v.ta.state == TRADE_REFUSED && v.ta.refusal == TREF_VERSION, "a different build is refused");
    Sim w;
    memcpy(w.B.id, w.A.id, 6);
    w.session();
    w.step(500);
    ck(w.ta.state == TRADE_REFUSED && w.ta.refusal == TREF_SELF, "a board with the same MAC is refused");
    CreatureUid nobody; nobody.serial = 999;
    ck(!s.ta.offer(nobody), "the live pet or anything not in party/box cannot be offered");
  }

  // --- 4. a lossy, duplicating radio, 300 times over
  {
    int done = 0, conserved = 0, cleanN = 0;
    for (int seed = 1; seed <= 300; seed++) {
      rng = (uint32_t)seed * 7919u;
      Sim s;
      s.dropPct = 33;
      s.dupPct = 15;
      s.session();
      s.step(1000);
      s.offerAndAccept(s.a1, s.b1);
      s.step(10000);
      done += s.dealt();
      conserved += (s.countAll(s.a1) == 1 && s.countAll(s.b1) == 1);
      cleanN += s.clean();
    }
    printf("      300 lossy trades: %d finished, %d conserved, %d clean\n", done, conserved, cleanN);
    ck(conserved == 300, "a lossy radio never loses or copies a creature");
    ck(done == 300, "and every trade still finishes");
    ck(cleanN == 300, "with nothing left open");
  }

  // --- 5. the power cut at EVERY durable write, on either board
  {
    Sim probe;
    probe.session();
    probe.step(500);
    probe.offerAndAccept(probe.a1, probe.b1);
    probe.step(3000);
    int wa = probe.A.writes, wb = probe.B.writes;
    printf("      a clean trade makes %d writes on A, %d on B\n", wa, wb);
    int cases = 0, ok = 0, traded = 0, kept = 0;
    for (int side = 0; side < 2; side++)
      for (int k = 0; k <= (side ? wb : wa); k++) {
        Sim s;
        s.session();
        s.step(500);
        (side ? s.B : s.A).cutAt = k;
        s.offerAndAccept(s.a1, s.b1);
        s.step(3000);
        settle(s);
        cases++;
        bool once = s.countAll(s.a1) == 1 && s.countAll(s.b1) == 1;
        bool fin = s.clean();
        if (once && fin) ok++;
        else printf("      cut %c@%d: a1 x%d b1 x%d clean=%d\n", side ? 'B' : 'A', k,
                    s.countAll(s.a1), s.countAll(s.b1), fin);
        if (s.A.count(s.b1.uid)) traded++; else kept++;
      }
    printf("      %d power cuts: %d settled as traded, %d as not traded\n", cases, traded, kept);
    ck(ok == cases, "a power cut at any write never loses or copies a creature, and settles");
  }

  // --- 6. the link cut after EVERY packet, both ways and one way
  {
    Sim probe;
    probe.session();
    probe.step(500);
    int before = probe.sent;
    probe.offerAndAccept(probe.a1, probe.b1);
    probe.step(3000);
    int total = probe.sent;
    int cases = 0, ok = 0, giveups = 0, giveOk = 0;
    for (int dir = 0; dir < 2; dir++)
      for (int n = before; n <= total; n++) {
        for (int give = 0; give < 2; give++) {
          Sim s;
          s.session();
          s.step(500);
          s.cutAfter = s.sent + (n - before);
          s.cutDir = dir;
          s.offerAndAccept(s.a1, s.b1);
          s.step(TRADE_PEER_TIMEOUT_MS + 2000);
          if (give) {
            // A player who never sees the other board again: a week later.
            s.A.extraSecs = s.B.extraSecs = TRADE_GIVEUP_SECS;
            bool any = false;
            if (s.ta.canGiveUp()) any |= s.ta.giveUp();
            if (s.tb.canGiveUp()) any |= s.tb.giveUp();
            if (!any) continue;
            giveups++;
            settle(s);
            // A give-up may duplicate. It may never lose.
            bool none = s.countAll(s.a1) >= 1 && s.countAll(s.b1) >= 1;
            if (none && s.clean()) giveOk++;
            else printf("      give-up after cut %d/%d: a1 x%d b1 x%d clean=%d\n", dir, n,
                        s.countAll(s.a1), s.countAll(s.b1), s.clean());
            continue;
          }
          settle(s);
          cases++;
          bool once = s.countAll(s.a1) == 1 && s.countAll(s.b1) == 1;
          if (once && s.clean()) ok++;
          else printf("      cut %d after %d: a1 x%d b1 x%d clean=%d\n", dir, n,
                      s.countAll(s.a1), s.countAll(s.b1), s.clean());
        }
      }
    printf("      %d link cuts settled, %d gave up\n", cases, giveups);
    ck(ok == cases, "a link cut at any packet never loses or copies a creature, and settles");
    ck(giveups > 0, "(some cut leaves a side pending long enough to give up)");
    ck(giveOk == giveups, "giving up never loses a creature");
  }

  // --- 7. give-up waits a week, and a pending trade locks its creature
  {
    Sim s;
    s.session();
    s.step(500);
    s.ta.offer(s.a1.uid);
    s.tb.offer(s.b1.uid);
    s.step(500);
    s.upAB = false;          // B never hears A's acceptance, so only A goes pending
    s.ta.accept();
    s.tb.accept();
    // B hears nothing from A and gives up the session first; A keeps hearing B
    // until then, so A's own clock only starts when B goes quiet.
    s.step(2 * TRADE_PEER_TIMEOUT_MS + 2000);
    ck(s.ta.j.active == TRADE_PENDING && s.ta.state == TRADE_STUCK, "(A is stuck pending)");
    ck(s.tb.state == TRADE_CANCELLED && !s.tb.j.active, "(B never went pending)");
    ck(s.ta.locks(s.a1.uid), "a pending trade keeps its creature locked");
    ck(!s.ta.canGiveUp() && !s.ta.giveUp(), "giving up is not offered before a week");
    // Meeting again settles it without anyone giving up: B answers UNKNOWN.
    settle(s);
    ck(s.clean() && s.A.count(s.a1.uid) == 1 && s.B.count(s.b1.uid) == 1,
       "meeting again cancels it cleanly: each keeps their own");
  }

  // --- 8. busy with one board, refused by another
  {
    Sim s;
    s.session();
    s.step(500);
    s.ta.offer(s.a1.uid);
    s.tb.offer(s.b1.uid);
    s.step(500);
    s.upAB = false;
    s.ta.accept();
    s.tb.accept();
    s.step(TRADE_PEER_TIMEOUT_MS + 2000);
    // A third board turns up; give it B's identity slot with a new MAC.
    s.B.id[0] = 0xC0;
    s.session();
    s.step(1000);
    ck(s.ta.state == TRADE_REFUSED && s.ta.refusal == TREF_BUSY,
       "a board pending with someone else refuses a new trade");
  }

  // --- 9. a full log of unconfirmed trades refuses a new one rather than
  //        forgetting one -- an open entry is the only proof a stuck peer has
  {
    Sim s;
    TradeJournal j;
    j.logN = TRADE_LOG_CAP;
    for (int i = 0; i < TRADE_LOG_CAP; i++) {
      j.log[i].fp = 1000 + i;
      j.log[i].mac[0] = (uint8_t)(0x10 + i);
      j.log[i].open = 1;
    }
    uint8_t buf[TRADE_JOURNAL_MAX];
    size_t n = tradeJournalWrite(j, buf, sizeof(buf));
    s.A.journal.assign(buf, buf + n);
    s.session();
    s.step(500);
    s.offerAndAccept(s.a1, s.b1);
    s.step(3000);
    ck(s.ta.state == TRADE_REFUSED && s.ta.refusal == TREF_LOG_FULL,
       "eight unconfirmed trades refuse a ninth");
    bool kept = true;
    for (int i = 0; i < TRADE_LOG_CAP; i++) kept &= s.ta.j.log[i].fp == (uint64_t)(1000 + i) && s.ta.j.log[i].open;
    ck(kept, "and none of the eight is forgotten");
    ck(s.countAll(s.a1) == 1 && s.countAll(s.b1) == 1, "and nothing moved");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
