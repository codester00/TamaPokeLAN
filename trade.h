#pragma once
#include <stdint.h>
#include <stddef.h>
#include "party.h"

// Trading one banked creature for another, between two boards over ESP-NOW.
//
// THE PROMISE: a trade between two honest boards never LOSES a creature --
// not to a dropped packet, not to a flat battery, not to the two boards
// wandering out of range at the worst possible moment. The only way a trade
// can end with a creature in two places is the player-chosen give-up below,
// and even that never ends with one in NO place.
//
// It is not anti-cheat and cannot be: EXPORT/IMPORT restores an old save by
// design and the firmware is open. Nothing here is a credential.
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
// The one escape hatch is giveUp(), offered after TRADE_GIVEUP_SECS: the
// player KEEPS their creature AND takes the incoming one into a free slot. If
// the peer had in fact committed that duplicates one creature; if it had not,
// it duplicates the other. Either way nothing is lost, which is why it keeps
// both rather than "reverting" -- a revert after the peer committed is exactly
// the loss this protocol exists to prevent.
//
// The board's storage is behind TradeBoard so the protocol can be tested: two
// boards in one process, packets dropped and duplicated on purpose, and a
// power cut at every single durable write. Only the radio is unverifiable here.

#define TRADE_PROTO 1
#define TRADE_NAME_LEN 12
#define TRADE_LOG_CAP 8
#define TRADE_RESEND_MS 400
#define TRADE_JITTER_MS 200           // see LINK_JITTER_MS: a fixed period can lock step with an interferer
#define TRADE_PEER_TIMEOUT_MS 15000   // nothing heard for this long: the peer is gone
#define TRADE_PAIR_TIMEOUT_MS 60000   // waiting for a peer that may not exist
#define TRADE_GIVEUP_SECS (7UL * 86400UL)

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
  TREF_BUSY,         // a trade with ANOTHER board is still pending here
  TREF_LOG_FULL,     // too many trades whose peer has not yet confirmed
};

enum TradeMsg : uint8_t {
  TM_HELLO = 1,   // proto, build tag, MAC, session nonce, name
  TM_OFFER,       // seq + the creature I would give (dex 0 = nothing chosen)
  TM_ACCEPT,      // I accept the pair whose fingerprint is this
  TM_READY,       // I have journaled fp: I am PENDING
  TM_DONE,        // I have committed fp
  TM_ACK,         // I know you committed fp; you may stop telling me
  TM_UNKNOWN,     // I have no pending or committed record of fp
  TM_BYE,         // leaving on purpose
};

// What a board must provide. The firmware's implementation is over `party`
// and NVS; the tests' is over arrays they can cut the power on.
struct TradeBoard {
  virtual ~TradeBoard() {}
  virtual void mac(uint8_t out[6]) = 0;
  virtual uint32_t epoch() = 0;                 // seconds, for the give-up wait
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
  // Called on the incoming record just before it is stored: the hook for
  // trade evolution, which marks the creature rather than evolving it here.
  virtual void onReceive(PartyMon &) {}
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

  uint32_t lastRx = 0, lastTx = 0;
  bool sawRx = false;
  uint8_t resendSeq = 0;

  void (*send)(void *ctx, const uint8_t *buf, uint8_t len) = nullptr;
  void *ctx = nullptr;

  // Loads the journal and finishes anything that was committed but not yet
  // applied when the power went. Call at boot even if no trade is started:
  // tradeRecover() below is the same thing without a session.
  void begin(TradeBoard *b, const char *name, uint32_t nonce);
  void start();                               // announce; LISTENING
  void end();                                 // say BYE and stop
  bool offer(const CreatureUid &u);           // what I would give; false if not in party/box
  bool accept();                              // false if there is nothing to accept yet
  void unaccept();
  void onPacket(const uint8_t *buf, uint8_t len);
  void tick(uint32_t now);

  // The player's escape hatch from a pending trade whose peer never came back.
  // Keeps the outgoing creature AND stores the incoming one; see the header.
  bool canGiveUp() const;
  bool giveUp();

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
