#pragma once
#include <stdint.h>
#include <stddef.h>
#include "party.h"

// Trading one banked creature for another, between two boards over ESP-NOW.
//
// THE PROMISE: a trade between two honest boards never LOSES a creature --
// not to a dropped packet, not to a flat battery, not to the two boards
// wandering out of range at the worst possible moment -- as long as the two
// boards reconnect within TRADE_DECIDE_MS. Past that a PERSON decides, by
// looking at the other board's screen (see "When contact is lost" below).
//
// It is not anti-cheat and does not try to be. The owner's call: the firmware
// is open source, nobody pays, and spoofing a trade harms nobody, so every
// board is assumed honest. EXPORT/IMPORT restores an old save by design.
//
// WHY IT HAS A PENDING STAGE. No exchange of messages can guarantee both sides
// learn the outcome, because the last message can always be lost (the "two
// generals" problem). A plain "time out and revert" therefore loses creatures:
// A commits on B's receipt, A's own receipt is lost, B times out and reverts,
// and now B's creature exists twice and A's nowhere. So:
//
//   1. CHOOSING. Each side offers one party/box creature (never the live pet).
//      Nothing is written. Leaving, a timeout, or a changed offer costs nothing.
//   2. AGREED.   Each side ACCEPTs a FINGERPRINT: a hash of both exact records
//      and both sides' session nonces. Equal fingerprints prove both boards
//      hold byte-identical copies of the same two creatures, and a fresh
//      nonce per session means the same pair traded twice is two trades.
//   3. PENDING.  Once both ACCEPTs match, each side writes ONE journal record
//      ("trade fp with that board: mine goes, theirs comes") and says READY.
//      The outgoing creature is locked. From here a side NEVER undoes its half
//      on its own -- not on a timeout, not on a reboot.
//   4. COMMIT.   A side commits only after hearing the other is PENDING (its
//      READY) or has committed (its DONE). That is the receipt. It journals
//      the commit, swaps the creature in, and says DONE until the peer ACKs.
//
// If the link dies between 3 and 4 the trade simply stays pending, and the
// next time those two boards meet they settle it: either side committed ->
// both commit; neither did -> both still pending, so both commit; one side has
// no record at all (it never reached PENDING) -> it answers UNKNOWN and the
// other cancels, safely, because a side cannot commit without having been
// PENDING first. A committed side keeps the fingerprint OPEN in its log until
// the peer acknowledges, so "I have never heard of it" can never be mistaken
// for "I finished it and forgot".
//
// WHEN CONTACT IS LOST. A pending board keeps calling for TRADE_DECIDE_MS
// (5 minutes): long enough to walk back into range, reboot, or reopen the
// screen, and any reconnect in that window settles the trade by itself as
// above. After that the player is ASKED, and the choice is final:
//
//   decideRevert()  keep my creature, drop the hidden copy of theirs.
//                   Right when the other board still shows ITS original.
//   decideKeep()    take theirs, delete mine -- i.e. commit.
//                   Right when the other board shows MY creature.
//
// The screen must say it cannot be undone and to check the other board first.
// The board cannot verify that answer and does not try: the owner chose a
// person's eyes over an automatic check here, and nothing afterwards second-
// guesses the decision. A wrong answer duplicates or loses a creature, which
// is why the warning exists. (If the other board is STILL pending when the two
// meet again, it simply follows: keep -> it hears DONE and commits; revert ->
// it hears UNKNOWN and cancels. That is the decision propagating, not a check.)
//
// The board's storage is behind TradeBoard so the protocol can be tested: two
// boards in one process, packets dropped and duplicated on purpose, and a
// power cut at every single durable write. Only the radio is unverifiable here.

#define TRADE_PROTO 3   // 3, v3.83: PartyMon::reserved[0] carries Meltan's walk; 2 would refuse it
#define TRADE_NAME_LEN 12
#define TRADE_LOG_CAP 8
#define TRADE_RESEND_MS 400
#define TRADE_JITTER_MS 200           // see LINK_JITTER_MS: a fixed period can lock step with an interferer
#define TRADE_PEER_TIMEOUT_MS 15000   // nothing heard for this long: the peer is gone
#define TRADE_PAIR_TIMEOUT_MS 60000   // waiting for a peer that may not exist
#define TRADE_DECIDE_MS (5UL * 60UL * 1000UL)   // pending with no word from the peer: ask the player

enum TradeState : uint8_t {
  TRADE_OFF = 0,
  TRADE_LISTENING,   // announced, no peer yet
  TRADE_CHOOSING,    // peer known; offers going back and forth; nothing written
  TRADE_PENDING,     // journaled, waiting for the peer's receipt
  TRADE_DONE,        // committed here; the creature has been swapped in
  TRADE_CANCELLED,   // ended with nothing changed on this board
  TRADE_STUCK,       // pending, and the peer went away: settled when you meet again
  TRADE_REFUSED,     // incompatible build, same board, or busy with someone else
};

enum TradeRefusal : uint8_t {
  TREF_NONE = 0,
  TREF_VERSION,      // a different protocol or table layout
  TREF_SELF,         // the "peer" has this board's own MAC
  TREF_BUSY,         // (unused: a board pending with another now just ignores a stranger)
  TREF_LOG_FULL,     // too many trades whose peer has not yet confirmed
};

// Numbered clear of link.h's LinkMsg (1..8), so a trading board and a battling
// board on the same channel can never read each other's packets: the radio
// layer delivers only the range it was opened for (TM_FIRST..TM_LAST).
enum TradeMsg : uint8_t {
  TM_HELLO = 0x41,   // proto, build tag, MAC, session nonce, name
  TM_OFFER,       // seq + the creature I would give (dex 0 = nothing chosen)
  TM_ACCEPT,      // I accept the pair whose fingerprint is this
  TM_READY,       // I have journaled fp: I am PENDING
  TM_DONE,        // I have committed fp
  TM_ACK,         // I know you committed fp; you may stop telling me
  TM_UNKNOWN,     // I have no pending or committed record of fp
  TM_BYE,         // leaving on purpose
};
#define TM_FIRST TM_HELLO
#define TM_LAST TM_BYE
static_assert(TM_FIRST > 0x3F, "linknow.cpp gives the battle link 1..0x3F");

// What a board must provide. The firmware's implementation is over `party`
// and NVS; the tests' is over arrays they can cut the power on.
struct TradeBoard {
  virtual ~TradeBoard() {}
  virtual void mac(uint8_t out[6]) = 0;
  virtual uint32_t epoch() = 0;                 // seconds; stamped on a pending trade, for display
  // The party and the box ONLY -- never the live pet, which cannot be traded.
  virtual bool findMon(const CreatureUid &u, PartyMon *out) = 0;
  // Overwrite the record holding `out` with `in`, durably. False if it did not land.
  virtual bool replaceMon(const CreatureUid &out, const PartyMon &in) = 0;
  // Put `in` in a free party/box slot, durably. False if full or it did not land.
  virtual bool storeNew(const PartyMon &in) = 0;
  // The journal: one blob, written atomically. False if it did not land.
  virtual bool saveJournal(const uint8_t *buf, size_t n) = 0;
  virtual size_t loadJournal(uint8_t *buf, size_t cap) = 0;
  // Policy hook: can this board hold that creature at all (its art, its region
  // pack)? Asked before ACCEPTing, so a board never agrees to something it
  // would have to draw as a bare number.
  virtual bool canReceive(const PartyMon &) { return true; }
  // (No receive hook: marking a creature for trade evolution is done by the
  // protocol itself, in applyJournal(), so no board can forget it.)
};

// A committed trade the peer may not yet know about (open), or that both
// sides have confirmed (closed, and evictable).
struct TradeLogEntry {
  uint64_t fp;
  uint8_t mac[6];
  uint8_t open;
  uint8_t pad;
};

// The journal. Saved as ONE blob, so every transition is atomic.
struct TradeJournal {
  uint8_t active = 0;          // 0 none, TRADE_PENDING, or TRADE_DONE (= committed, maybe not yet applied)
  uint64_t fp = 0;
  uint8_t peer[6] = { 0 };
  uint32_t since = 0;          // epoch when it went pending
  CreatureUid outUid;          // what leaves this board
  PartyMon in;                 // what arrives
  uint8_t logN = 0;
  TradeLogEntry log[TRADE_LOG_CAP];
};

// What the trade screen shows, as THIS board understands it. Exists for the
// moment contact is lost: both players are stuck on the trade screen and
// cannot open their party to check, so each screen must say plainly what this
// board holds -- and that is exactly what the OTHER player needs to read to
// decide. "Complete" on one screen means choose KEEP on the other; anything
// else means choose REVERT.
enum TradeView : uint8_t {
  TV_NONE = 0,               // no trade to report
  TV_INCOMPLETE,             // not traded here: you still have `have`, your original
  TV_COMPLETE_UNCONFIRMED,   // traded here: you have `have`, the new one; the other board has not confirmed
  TV_COMPLETE,               // traded, and both boards know it
};
struct TradeStatus {
  uint8_t view = TV_NONE;
  bool connected = false;    // heard from the peer within TRADE_PEER_TIMEOUT_MS
  bool mustDecide = false;   // the player is being asked (canDecide())
  uint16_t secsLeft = 0;     // until they are asked; counts down only while disconnected and pending
  PartyMon have;             // the creature in play that THIS board holds right now
  PartyMon other;            // the one on the other side of the deal
};

struct Trade {
  uint8_t state = TRADE_OFF;
  uint8_t refusal = TREF_NONE;
  TradeBoard *board = nullptr;
  TradeJournal j;

  uint8_t myMac[6] = { 0 };
  uint8_t peerMac[6] = { 0 };
  bool havePeer = false;
  uint32_t myNonce = 0, peerNonce = 0;
  char myName[TRADE_NAME_LEN] = "";
  char peerName[TRADE_NAME_LEN] = "";

  // CHOOSING
  PartyMon mine;               // dex 0 = nothing offered yet
  uint8_t mineSeq = 0;
  PartyMon theirs;
  bool theirsOk = false;       // arrived AND passed validation AND canReceive()
  uint8_t theirsWhy = 0;       // why not, for the screen: 1 invalid, 2 cannot hold, 3 already here
  bool iAccepted = false;
  uint64_t theirAccept = 0;    // the fingerprint they last accepted, 0 = none

  uint32_t lastRx = 0, lastTx = 0, nowMs = 0;
  bool sawRx = false;
  uint8_t resendSeq = 0;

  void (*send)(void *ctx, const uint8_t *buf, uint8_t len) = nullptr;
  void *ctx = nullptr;

  // Loads the journal and finishes anything that was committed but not yet
  // applied when the power went. Call at boot even if no trade is started:
  // tradeRecover() below is the same thing without a session.
  void begin(TradeBoard *b, const char *name, uint32_t nonce);
  void start();                               // announce; LISTENING (STUCK if a trade is pending)
  void end();                                 // say BYE and stop
  bool offer(const CreatureUid &u);           // what I would give; false if not in party/box
  bool accept();                              // false if there is nothing to accept yet
  void unaccept();
  void onPacket(const uint8_t *buf, uint8_t len);
  void tick(uint32_t now);

  // A pending trade whose peer has been silent for TRADE_DECIDE_MS: the player
  // looks at the other board and decides. Both are final; see the header.
  bool canDecide() const;
  TradeStatus status() const;
  bool decideRevert();    // keep mine
  bool decideKeep();      // take theirs

  // Is this creature promised to a trade (offered, or journaled)? The UI asks
  // this before letting it be released, swapped in, or offered elsewhere.
  bool locks(const CreatureUid &u) const;
  uint64_t fingerprint() const;               // of the current pair, 0 if incomplete
  bool hasOpenEntries() const;

  // internal, public so the tests can poke at them
  bool pend(uint64_t fp);
  bool commit(uint64_t fp, bool peerCommitted);
  void cancelPending();
  bool applyCommitted();
  bool saveJ();
};

// Boot-time: finish a trade the power cut in the middle of applying. Safe to
// call with nothing to do. Returns false only if a journal exists and could
// not be settled (it will be retried next boot).
bool tradeRecover(TradeBoard *b);

// Everything an incoming creature must satisfy before it is believed. REJECTS
// rather than clamps: a clamped record would hash differently on the two
// boards and could never be agreed on, and anything this check would have to
// "fix" was not sent by an honest build anyway.
bool tradeMonValid(const PartyMon &m);

// What must match for two builds to read a trade packet the same way.
uint16_t tradeBuildTag();

// Journal (de)serialisation, exposed for the tests.
size_t tradeJournalWrite(const TradeJournal &j, uint8_t *buf, size_t cap);
bool tradeJournalRead(TradeJournal &j, const uint8_t *buf, size_t n);
#define TRADE_JOURNAL_MAX 512
