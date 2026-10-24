#include "trade.h"
#include "pet.h"
#include "moves.h"
#include "dex.h"
#include <string.h>

// Packets are [type][len][payload], as in link.cpp.
#define HDR 2
#define MAX_PAYLOAD 200

// ---- the creature on the wire ----------------------------------------------
// A record is hashed and journaled as a CANONICAL 60-byte image, not as the
// struct's raw bytes. PartyMon has two padding bytes before ageMinutes, and a
// struct copy is free to leave padding as whatever was there -- so two boards
// holding the "same" creature could hash different bytes and never agree.
// Packing zeroes the padding; both sides pack before hashing, so they hash the
// same thing whatever their compilers did.
#define MON_BYTES 64
#define MON_PAD_AT 42
static_assert(sizeof(PartyMon) == MON_BYTES, "the trade wire format is a PartyMon");
static_assert(offsetof(PartyMon, ageMinutes) == MON_PAD_AT + 2, "padding moved");

static void monPack(const PartyMon &m, uint8_t out[MON_BYTES]) {
  memcpy(out, &m, MON_BYTES);
  out[MON_PAD_AT] = out[MON_PAD_AT + 1] = 0;
}
static void monUnpack(PartyMon &m, const uint8_t in[MON_BYTES]) {
  memcpy(&m, in, MON_BYTES);
}
static bool monSame(const PartyMon &a, const PartyMon &b) {
  uint8_t x[MON_BYTES], y[MON_BYTES];
  monPack(a, x);
  monPack(b, y);
  return !memcmp(x, y, MON_BYTES);
}

bool tradeMonValid(const PartyMon &m) {
  if (m.dex < 1 || m.dex > DEX_COUNT) return false;
  if (m.level < 1 || m.level > MAX_LEVEL) return false;
  const uint8_t iv[4] = { m.ivAtk, m.ivDef, m.ivSpe, m.ivHp };
  for (uint8_t v : iv) if (v > 31) return false;
  if (m.trAtk > Pet::trMaxFor(m.ivAtk) || m.trDef > Pet::trMaxFor(m.ivDef) ||
      m.trSpe > Pet::trMaxFor(m.ivSpe)) return false;
  if (m.shiny > 1 || m.berryKnown > 1 || m.stateVersion > 1) return false;
  // The nickname must END inside its field, and be text the bitmap font has:
  // it is drawn on the receiving panel and read as a C string.
  bool ended = false;
  for (size_t i = 0; i < sizeof(m.nick); i++) {
    if (!m.nick[i]) { ended = true; break; }
    if (m.nick[i] < 0x20 || m.nick[i] > 0x7E) return false;
  }
  if (!ended) return false;
  for (int i = 0; i < MOVE_SLOTS; i++) if (m.moves[i] >= MOVE_COUNT) return false;
  if (m.fullness > 100 || m.joy > 100 || m.energy > 100 || m.hygiene > 100 ||
      m.weight > 100 || m.bond > 100) return false;
  if (!m.uid.valid()) return false;
  if (m.flags & ~(PMON_TRADE_EVO | PMON_CHEATED)) return false;   // a bit no build defines
  for (uint8_t r : m.reserved) if (r) return false;
  return true;
}

uint16_t tradeBuildTag() {
  uint32_t h = 2166136261u;
  const uint16_t bits[] = {
    (uint16_t)TRADE_PROTO, (uint16_t)sizeof(PartyMon), (uint16_t)DEX_COUNT,
    (uint16_t)MOVE_COUNT, (uint16_t)MOVE_SLOTS,
  };
  for (uint16_t b : bits) { h ^= b; h *= 16777619u; }
  return (uint16_t)(h ^ (h >> 16));
}

// FNV-1a, 64 bit. Not a secret and not a signature -- the firmware is open, so
// any "key" would ship to everybody. It only has to make "we are holding the
// same two records" checkable in 8 bytes.
static uint64_t fnv64(uint64_t h, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

// ---- the journal ------------------------------------------------------------
static const uint8_t J_MAGIC[4] = { 'T', 'R', 'J', '1' };
#define J_VERSION 1
#define J_ENTRY 15        // fp 8, mac 6, open 1
#define J_FIXED (4 + 4 + 8 + 6 + 4 + 12 + MON_BYTES)
#define J_BYTES (J_FIXED + TRADE_LOG_CAP * J_ENTRY + 2)
static_assert(J_BYTES <= TRADE_JOURNAL_MAX, "raise TRADE_JOURNAL_MAX");

static uint16_t crc16(const uint8_t *p, size_t n) {   // CRC-16/CCITT-FALSE
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)p[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}
static void wr32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void wr64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint64_t rd64(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
  return v;
}
static void wrUid(uint8_t *p, const CreatureUid &u) {
  memcpy(p, u.mac, 6);
  p[6] = (uint8_t)u.salt;
  p[7] = (uint8_t)(u.salt >> 8);
  wr32(p + 8, u.serial);
}
static void rdUid(CreatureUid &u, const uint8_t *p) {
  memcpy(u.mac, p, 6);
  u.salt = (uint16_t)(p[6] | p[7] << 8);
  u.serial = rd32(p + 8);
}

size_t tradeJournalWrite(const TradeJournal &j, uint8_t *buf, size_t cap) {
  if (cap < J_BYTES) return 0;
  memset(buf, 0, J_BYTES);
  memcpy(buf, J_MAGIC, 4);
  buf[4] = J_VERSION;
  buf[5] = j.active;
  buf[6] = j.logN;
  size_t at = 8;
  wr64(buf + at, j.fp); at += 8;
  memcpy(buf + at, j.peer, 6); at += 6;
  wr32(buf + at, j.since); at += 4;
  wrUid(buf + at, j.outUid); at += 12;
  monPack(j.in, buf + at); at += MON_BYTES;
  for (int i = 0; i < TRADE_LOG_CAP; i++, at += J_ENTRY) {
    wr64(buf + at, j.log[i].fp);
    memcpy(buf + at + 8, j.log[i].mac, 6);
    buf[at + 14] = j.log[i].open;
  }
  uint16_t c = crc16(buf, at);
  buf[at] = (uint8_t)c;
  buf[at + 1] = (uint8_t)(c >> 8);
  return at + 2;
}

bool tradeJournalRead(TradeJournal &j, const uint8_t *buf, size_t n) {
  j = TradeJournal();
  if (n != J_BYTES || memcmp(buf, J_MAGIC, 4) || buf[4] != J_VERSION) return false;
  uint16_t c = (uint16_t)(buf[n - 2] | buf[n - 1] << 8);
  if (crc16(buf, n - 2) != c) return false;
  if (buf[6] > TRADE_LOG_CAP) return false;
  if (buf[5] != 0 && buf[5] != TRADE_PENDING && buf[5] != TRADE_DONE) return false;
  j.active = buf[5];
  j.logN = buf[6];
  size_t at = 8;
  j.fp = rd64(buf + at); at += 8;
  memcpy(j.peer, buf + at, 6); at += 6;
  j.since = rd32(buf + at); at += 4;
  rdUid(j.outUid, buf + at); at += 12;
  monUnpack(j.in, buf + at); at += MON_BYTES;
  for (int i = 0; i < TRADE_LOG_CAP; i++, at += J_ENTRY) {
    j.log[i].fp = rd64(buf + at);
    memcpy(j.log[i].mac, buf + at + 8, 6);
    j.log[i].open = buf[at + 14] ? 1 : 0;
  }
  return true;
}

static bool loadJ(TradeBoard *b, TradeJournal &j) {
  uint8_t buf[TRADE_JOURNAL_MAX];
  size_t n = b->loadJournal(buf, sizeof(buf));
  if (!n) { j = TradeJournal(); return true; }   // never traded: an empty journal
  // A journal that fails its CRC cannot be believed, and inventing one would be
  // worse. Treat it as empty and say so: the creature it described stays where
  // it is, so the worst case is a trade that never finishes on this side.
  if (!tradeJournalRead(j, buf, n)) {
    j = TradeJournal();
    return false;
  }
  return true;
}

static bool saveJournalOf(TradeBoard *b, const TradeJournal &j) {
  uint8_t buf[TRADE_JOURNAL_MAX];
  size_t n = tradeJournalWrite(j, buf, sizeof(buf));
  return n && b->saveJournal(buf, n);
}

// Puts the committed incoming creature where the outgoing one was. IDEMPOTENT,
// because a power cut can land between swapping it in and clearing the
// journal, and the next boot runs it again: if the incoming creature is
// already here, the job is done.
static bool applyJournal(TradeBoard *b, TradeJournal &j) {
  if (j.active != TRADE_DONE) return true;
  PartyMon in = j.in;
  PartyMon gave;
  const bool outHere = b->findMon(j.outUid, &gave);
  // Trade evolution: marked here, by the protocol, on every board alike. It
  // needs what was given in exchange (KARRABLAST only for a SHELMET), which is
  // the outgoing creature still in its slot at this moment.
  Pet::markTraded(in, outHere ? gave.dex : 0);
  bool landed;
  if (b->findMon(in.uid, nullptr)) landed = true;
  else if (outHere) landed = b->replaceMon(j.outUid, in);
  // The outgoing creature is gone, which the lock should have made impossible.
  // Still never drop what arrived: give it a free slot.
  else landed = b->storeNew(in);
  if (!landed) return false;
  TradeJournal next = j;
  next.active = 0;
  if (!saveJournalOf(b, next)) return false;   // harmless: re-applying finds it in place
  j = next;
  return true;
}

bool tradeRecover(TradeBoard *b) {
  TradeJournal j;
  loadJ(b, j);
  return applyJournal(b, j);
}

// ---- the session -------------------------------------------------------------
static void rawPut(Trade &t, uint8_t type, const uint8_t *body, uint8_t n) {
  if (!t.send || n > MAX_PAYLOAD) return;
  uint8_t pkt[HDR + MAX_PAYLOAD];
  pkt[0] = type;
  pkt[1] = n;
  if (n) memcpy(pkt + HDR, body, n);
  t.send(t.ctx, pkt, (uint8_t)(HDR + n));
}
static void putFp(Trade &t, uint8_t type, uint64_t fp) {
  uint8_t b[8];
  wr64(b, fp);
  rawPut(t, type, b, 8);
}

#define HELLO_LEN (1 + 2 + 6 + 4 + TRADE_NAME_LEN)
static void sendHello(Trade &t) {
  uint8_t b[HELLO_LEN] = {};
  b[0] = TRADE_PROTO;
  uint16_t tag = tradeBuildTag();
  b[1] = (uint8_t)tag;
  b[2] = (uint8_t)(tag >> 8);
  memcpy(b + 3, t.myMac, 6);
  wr32(b + 9, t.myNonce);
  memcpy(b + 13, t.myName, TRADE_NAME_LEN);
  rawPut(t, TM_HELLO, b, HELLO_LEN);
}
static void sendOffer(Trade &t) {
  uint8_t b[1 + MON_BYTES];
  b[0] = t.mineSeq;
  monPack(t.mine, b + 1);
  rawPut(t, TM_OFFER, b, sizeof(b));
}

static bool macEq(const uint8_t *a, const uint8_t *b) { return !memcmp(a, b, 6); }

static int logFind(const TradeJournal &j, uint64_t fp) {
  for (int i = 0; i < j.logN; i++) if (j.log[i].fp == fp) return i;
  return -1;
}
// Room for one more committed entry: a free slot or a CLOSED one to evict. An
// open entry is never evicted -- it is the evidence a stuck peer will ask for.
static bool logHasRoom(const TradeJournal &j) {
  if (j.logN < TRADE_LOG_CAP) return true;
  for (int i = 0; i < j.logN; i++) if (!j.log[i].open) return true;
  return false;
}
static void logAdd(TradeJournal &j, uint64_t fp, const uint8_t *mac, bool open) {
  int at = -1;
  if (j.logN < TRADE_LOG_CAP) at = j.logN++;
  else for (int i = 0; i < j.logN && at < 0; i++) if (!j.log[i].open) at = i;
  if (at < 0) return;   // logHasRoom() was checked before going pending
  j.log[at].fp = fp;
  memcpy(j.log[at].mac, mac, 6);
  j.log[at].open = open ? 1 : 0;
  j.log[at].pad = 0;
}

bool Trade::saveJ() { return board && saveJournalOf(board, j); }

void Trade::begin(TradeBoard *b, const char *name, uint32_t nonce) {
  board = b;
  state = TRADE_OFF;
  refusal = TREF_NONE;
  havePeer = false;
  memset(peerMac, 0, 6);
  peerNonce = 0;
  peerName[0] = 0;
  myNonce = nonce ? nonce : 1;
  strncpy(myName, name ? name : "", sizeof(myName) - 1);
  myName[sizeof(myName) - 1] = 0;
  mine = PartyMon();
  theirs = PartyMon();
  mineSeq = 0;
  theirsOk = false;
  theirsWhy = 0;
  iAccepted = false;
  theirAccept = 0;
  lastRx = lastTx = 0;
  sawRx = false;
  resendSeq = 0;
  if (!b) return;
  b->mac(myMac);
  loadJ(b, j);
  applyJournal(b, j);   // finish a commit the power interrupted
}

void Trade::start() {
  if (!board) return;
  // A trade left pending (a reboot, a reopened screen) starts STUCK, not
  // LISTENING: LISTENING gives up after TRADE_PAIR_TIMEOUT_MS and goes quiet,
  // and a pending board must keep calling its peer for the whole window.
  // State BEFORE sending: a synchronous transport re-enters.
  state = j.active == TRADE_PENDING ? TRADE_STUCK : TRADE_LISTENING;
  sawRx = true;              // so the first tick() starts the clock from now
  sendHello(*this);
}

void Trade::end() {
  if (state != TRADE_OFF) rawPut(*this, TM_BYE, nullptr, 0);
  if (state == TRADE_CHOOSING || state == TRADE_LISTENING) state = TRADE_CANCELLED;
  else if (state == TRADE_PENDING) state = TRADE_STUCK;
}

bool Trade::locks(const CreatureUid &u) const {
  if (!u.valid()) return false;
  if (j.active && j.outUid == u) return true;
  return (state == TRADE_CHOOSING || state == TRADE_LISTENING) && mine.dex > 0 && mine.uid == u;
}

bool Trade::hasOpenEntries() const {
  for (int i = 0; i < j.logN; i++) if (j.log[i].open) return true;
  return false;
}

uint64_t Trade::fingerprint() const {
  if (!havePeer || mine.dex < 1 || !theirsOk) return 0;
  uint8_t a[MON_BYTES], b[MON_BYTES];
  bool meFirst = memcmp(myMac, peerMac, 6) < 0;
  monPack(meFirst ? mine : theirs, a);
  monPack(meFirst ? theirs : mine, b);
  uint8_t n[8];
  wr32(n, meFirst ? myNonce : peerNonce);
  wr32(n + 4, meFirst ? peerNonce : myNonce);
  uint64_t h = 14695981039346656037ULL;
  h = fnv64(h, a, MON_BYTES);
  h = fnv64(h, b, MON_BYTES);
  h = fnv64(h, n, 8);
  return h ? h : 1;   // 0 means "none"
}

bool Trade::offer(const CreatureUid &u) {
  if (state != TRADE_LISTENING && state != TRADE_CHOOSING) return false;
  if (j.active) return false;
  PartyMon m;
  if (!u.valid() || !board->findMon(u, &m) || !tradeMonValid(m)) return false;
  mine = m;
  mineSeq++;
  iAccepted = false;          // a different offer is a different deal
  if (havePeer) sendOffer(*this);
  return true;
}

static void checkAgreement(Trade &t);

bool Trade::accept() {
  if (state != TRADE_CHOOSING || mine.dex < 1 || !theirsOk) return false;
  iAccepted = true;
  putFp(*this, TM_ACCEPT, fingerprint());
  checkAgreement(*this);
  return true;
}

void Trade::unaccept() {
  if (state == TRADE_CHOOSING) iAccepted = false;
}

bool Trade::pend(uint64_t fp) {
  if (j.active || !fp) return false;
  if (!logHasRoom(j)) { state = TRADE_REFUSED; refusal = TREF_LOG_FULL; return false; }
  // What we offered must still be exactly what we hold: banked creatures are
  // frozen, but a swap or release in another screen would make this a promise
  // about a creature that is no longer here.
  PartyMon now;
  if (!board->findMon(mine.uid, &now) || !monSame(now, mine)) {
    iAccepted = false;
    return false;
  }
  TradeJournal next = j;
  next.active = TRADE_PENDING;
  next.fp = fp;
  memcpy(next.peer, peerMac, 6);
  next.since = board->epoch();
  next.outUid = mine.uid;
  next.in = theirs;
  if (!saveJournalOf(board, next)) return false;   // nothing durable happened: still choosing
  j = next;
  state = TRADE_PENDING;
  putFp(*this, TM_READY, fp);
  return true;
}

bool Trade::commit(uint64_t fp, bool peerCommitted) {
  if (j.active != TRADE_PENDING || j.fp != fp) return false;
  TradeJournal next = j;
  next.active = TRADE_DONE;
  // OPEN unless the peer has already committed: then it is the one waiting to
  // hear from us, and we owe it an ACK rather than it owing us one.
  logAdd(next, fp, j.peer, !peerCommitted);
  if (!saveJournalOf(board, next)) return false;   // still pending; the next receipt retries
  j = next;
  state = TRADE_DONE;
  applyJournal(board, j);   // if this fails, tick() and the next boot retry it
  return true;
}

void Trade::cancelPending() {
  if (j.active != TRADE_PENDING) return;
  TradeJournal next = j;
  next.active = 0;
  if (!saveJournalOf(board, next)) return;   // still pending; the next UNKNOWN retries
  j = next;
  state = TRADE_CANCELLED;
}

bool Trade::applyCommitted() { return applyJournal(board, j); }

// "Silent" is measured from the last thing heard, and a session that starts
// with a journal already pending starts the clock at its first tick -- so after
// a reboot the boards still get their five minutes to find each other.
bool Trade::canDecide() const {
  return board && j.active == TRADE_PENDING && state != TRADE_OFF &&
         nowMs - lastRx >= TRADE_DECIDE_MS;
}

TradeStatus Trade::status() const {
  TradeStatus s;
  if (!board) return s;
  uint32_t silent = nowMs - lastRx;
  s.connected = havePeer && silent < TRADE_PEER_TIMEOUT_MS;
  s.mustDecide = canDecide();
  int li = logFind(j, j.fp);
  if (j.active == TRADE_PENDING) {
    // Not traded here: the original is still in its slot (locked), and the
    // incoming one exists only as the journal's hidden copy.
    s.view = TV_INCOMPLETE;
    board->findMon(j.outUid, &s.have);
    s.other = j.in;
    if (!s.connected && silent < TRADE_DECIDE_MS)
      s.secsLeft = (uint16_t)((TRADE_DECIDE_MS - silent + 999) / 1000);
  } else if (j.fp && (j.active == TRADE_DONE || state == TRADE_DONE || (li >= 0 && j.log[li].open))) {
    // Traded here. Still "unconfirmed" while the peer has not acknowledged --
    // and that survives a reboot, because the open entry is in the journal.
    s.view = (li >= 0 && j.log[li].open) ? TV_COMPLETE_UNCONFIRMED : TV_COMPLETE;
    s.have = j.in;
    s.other = mine;   // only known in the session that traded it
  } else if ((state == TRADE_CANCELLED || state == TRADE_STUCK) && mine.dex > 0) {
    s.view = TV_INCOMPLETE;   // this session offered something and nothing moved
    s.have = mine;
    s.other = theirs;
  }
  return s;
}

bool Trade::decideRevert() {
  if (!canDecide()) return false;
  cancelPending();
  return j.active == 0;
}

// Exactly what the peer's receipt would have done, so the journal, the open
// log entry and the power-cut handling are the ones every test already covers.
bool Trade::decideKeep() {
  if (!canDecide()) return false;
  return commit(j.fp, false);
}

static void checkAgreement(Trade &t) {
  if (t.state != TRADE_CHOOSING || !t.iAccepted || !t.theirAccept) return;
  uint64_t fp = t.fingerprint();
  if (fp && fp == t.theirAccept) t.pend(fp);
}

// The periodic set: everything currently true, said again. A lost packet is a
// delay, never a stall, and there is no per-message bookkeeping to get wrong.
static void sendState(Trade &t) {
  if (t.state == TRADE_OFF || t.state == TRADE_REFUSED) return;
  bool live = t.state == TRADE_LISTENING || t.state == TRADE_CHOOSING ||
              t.state == TRADE_PENDING || t.state == TRADE_STUCK ||
              (t.state == TRADE_DONE && t.hasOpenEntries());
  if (live) sendHello(t);
  if (!t.havePeer) return;
  if (t.state == TRADE_CHOOSING) {
    if (t.mine.dex > 0) sendOffer(t);
    if (t.iAccepted) putFp(t, TM_ACCEPT, t.fingerprint());
  }
  if (t.j.active == TRADE_PENDING && macEq(t.j.peer, t.peerMac)) putFp(t, TM_READY, t.j.fp);
  for (int i = 0; i < t.j.logN; i++)
    if (t.j.log[i].open && macEq(t.j.log[i].mac, t.peerMac)) putFp(t, TM_DONE, t.j.log[i].fp);
}

void Trade::tick(uint32_t now) {
  if (state == TRADE_OFF || !board) return;
  if (sawRx) { lastRx = now; sawRx = false; }
  // A commit whose swap did not land (a failed write) keeps trying.
  if (j.active == TRADE_DONE) applyJournal(board, j);

  nowMs = now;
  if (now - lastRx > (havePeer ? TRADE_PEER_TIMEOUT_MS : TRADE_PAIR_TIMEOUT_MS)) {
    if (state == TRADE_CHOOSING || state == TRADE_LISTENING) { state = TRADE_CANCELLED; return; }
    if (state == TRADE_PENDING) state = TRADE_STUCK;
    // STUCK and DONE keep calling: a board walking back into range inside
    // TRADE_DECIDE_MS must find this one still asking, or the window is moot.
  }
  uint32_t gap = TRADE_RESEND_MS + (uint32_t)((resendSeq * 7919u) % TRADE_JITTER_MS);
  if (now - lastTx >= gap) {
    lastTx = now;
    resendSeq++;
    sendState(*this);
  }
}

void Trade::onPacket(const uint8_t *buf, uint8_t len) {
  if (state == TRADE_OFF || !board || len < HDR || buf[1] != len - HDR) return;
  const uint8_t type = buf[0];
  const uint8_t *p = buf + HDR;
  const uint8_t n = buf[1];

  if (type == TM_HELLO) {
    if (n != HELLO_LEN) return;
    if (p[0] != TRADE_PROTO || (uint16_t)(p[1] | p[2] << 8) != tradeBuildTag()) {
      state = TRADE_REFUSED; refusal = TREF_VERSION; return;
    }
    const uint8_t *mac = p + 3;
    if (macEq(mac, myMac)) { state = TRADE_REFUSED; refusal = TREF_SELF; return; }
    if (havePeer && !macEq(mac, peerMac)) return;   // somebody else's trade
    // Pending with another board: this one is a stranger. Ignore it rather than
    // refuse -- a refused session stops calling, and the board this one is
    // pending with could walk back into range at any moment.
    if (j.active == TRADE_PENDING && !macEq(mac, j.peer)) return;
    uint32_t nonce = rd32(p + 9);
    sawRx = true;
    if (havePeer && nonce == peerNonce) return;     // a resend: nothing new
    // A new peer, or the same peer in a new session (it rebooted): whatever was
    // being chosen is void -- the fingerprint covers the nonce, so an old ACCEPT
    // can never match the new session anyway.
    memcpy(peerMac, mac, 6);
    peerNonce = nonce;
    memcpy(peerName, p + 13, TRADE_NAME_LEN);
    peerName[TRADE_NAME_LEN - 1] = 0;
    havePeer = true;
    theirs = PartyMon();
    theirsOk = false;
    theirsWhy = 0;
    theirAccept = 0;
    iAccepted = false;
    if (state == TRADE_REFUSED) return;
    if (j.active == TRADE_PENDING) {
      state = TRADE_PENDING;   // with THIS board (a stranger was ignored above): settle it
    } else if (state == TRADE_LISTENING || state == TRADE_STUCK || state == TRADE_CANCELLED) {
      state = TRADE_CHOOSING;
    }
    sendState(*this);
    return;
  }

  if (!havePeer) return;
  sawRx = true;
  if (state == TRADE_STUCK) state = TRADE_PENDING;   // the peer is back

  if (type == TM_OFFER) {
    if (n != 1 + MON_BYTES || state != TRADE_CHOOSING) return;
    PartyMon m;
    monUnpack(m, p + 1);
    if (theirs.dex > 0 && monSame(m, theirs)) return;   // a resend
    theirs = m;
    theirsWhy = 0;
    if (!tradeMonValid(m)) theirsWhy = 1;
    else if (!board->canReceive(m)) theirsWhy = 2;
    // Already on this board: a second copy of one ID on one board would make
    // "swap out the creature with this ID" ambiguous, so it is refused.
    else if (board->findMon(m.uid, nullptr) || (mine.dex > 0 && mine.uid == m.uid)) theirsWhy = 3;
    theirsOk = theirsWhy == 0 && m.dex > 0;
    iAccepted = false;   // the deal I accepted is not this one
    theirAccept = 0;
    return;
  }
  if (type == TM_BYE) {
    if (state == TRADE_CHOOSING) state = TRADE_CANCELLED;
    else if (state == TRADE_PENDING) state = TRADE_STUCK;
    return;
  }
  if (n != 8) return;
  const uint64_t fp = rd64(p);
  const int li = logFind(j, fp);
  const bool mineActive = j.active == TRADE_PENDING && j.fp == fp;

  switch (type) {
  case TM_ACCEPT:
    if (state == TRADE_CHOOSING) {
      theirAccept = fp;
      checkAgreement(*this);
    }
    break;

  case TM_READY:   // the peer is PENDING on fp
    if (mineActive) {
      if (commit(fp, false)) putFp(*this, TM_DONE, fp);
    } else if (li >= 0) {
      putFp(*this, TM_DONE, fp);   // we finished it; tell them again
    } else if (state == TRADE_CHOOSING && iAccepted && fp == fingerprint()) {
      // Their READY overtook their ACCEPT. It proves they accepted this exact
      // pair, so it is our agreement as well: journal, then commit at once.
      if (pend(fp) && commit(fp, false)) putFp(*this, TM_DONE, fp);
    } else {
      putFp(*this, TM_UNKNOWN, fp);   // never pending on it: they may cancel
    }
    break;

  case TM_DONE:    // the peer has COMMITTED fp
    if (mineActive) {
      if (commit(fp, true)) putFp(*this, TM_ACK, fp);
    } else if (li >= 0) {
      if (j.log[li].open) {
        TradeJournal next = j;
        next.log[li].open = 0;
        if (saveJournalOf(board, next)) j = next;
      }
      putFp(*this, TM_ACK, fp);
    } else {
      // Only reachable after this player decided to revert: they need not keep asking.
      putFp(*this, TM_UNKNOWN, fp);
    }
    break;

  case TM_ACK:
  case TM_UNKNOWN:
    if (type == TM_UNKNOWN && mineActive) {
      // They hold no record: they never went pending, so they never committed,
      // so cancelling cannot lose anything (see the header).
      cancelPending();
    } else if (li >= 0 && j.log[li].open) {
      TradeJournal next = j;
      next.log[li].open = 0;
      if (saveJournalOf(board, next)) j = next;
    }
    break;
  }
}
