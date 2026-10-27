#pragma once
#include <stdint.h>
#include "battle.h"
#include "trainers.h"   // TRAINER_TEAM_MAX: a link team is the same size

// Peer-to-peer battles.
//
// ONE DEVICE IS AUTHORITATIVE. That is forced, not chosen: battle.cpp makes 11
// random() calls a turn -- crit, damage spread, accuracy, ailment procs,
// multi-hit count, confusion, thaw, wake, speed ties -- so two devices
// resolving independently desync inside a single turn. A shared seed only holds
// until the two builds differ by one call. The host runs battleAct(); the guest
// sends an action and renders the result it is handed, and therefore needs no
// game logic at all.
//
// THE RADIO DROPS FRAMES. ESP-NOW is best effort, so nothing here may assume a
// packet arrived. Three rules cover it:
//
//   1. Every exchange is stamped with a turn number. A repeat of a turn already
//      handled is ignored, so a resend can never be mistaken for a new choice.
//   2. Whatever we last said is resent until something supersedes it. That is
//      what makes a lost frame a delay rather than a permanent stall.
//   3. A LinkResult carries ABSOLUTE state, never deltas. A guest that misses a
//      turn entirely still lands on the right numbers from the next one -- the
//      loss costs narration, not correctness.
//
// And every wait has a deadline: a peer that goes out of range or powers off
// ends as LINK_LOST with a message, never as a hang. But liveness is judged
// from RECEIVED traffic alone, and a side with nothing queued to resend sends
// nothing while it waits -- so whichever side is currently just thinking about
// a move, not transmitting, was starving the OTHER side's clock. A human
// deciding a move routinely takes longer than the deadline, so this fired on
// ordinary play, not just on a bad radio. LM_PING is tick()'s answer: a
// payload-less idle heartbeat sent on the same jittered cadence whenever
// nothing else is pending, so liveness no longer depends on anyone acting.
//
// The transport is a function pointer rather than a direct ESP-NOW call so the
// protocol can be tested: two Links cross-wired in one process exercise the
// whole handshake without a radio, and a deliberately lossy transport exercises
// all of the above. Only the radio itself is unverifiable here.

#define LINK_PROTO 9        // bump on ANY wire change; a mismatch is refused
                            // (8, v3.64: LinkResult carries the turn's beats;
                            // 9, v3.69: SKETCH in the beats, the guest's moves)
                            // (6, v3.58: the stats a LinkMon carries scale
                            // training with level; 7, v3.60: the trainer marks
                            // in the hello, the creature mark in LinkMon::flags)
#define LINK_MARK_RULES  1  // the hidden settings were used (Pet::rulesUsed)
#define LINK_MARK_CHEATS 2  // cheats were used (Pet::cheatsUsed)
#define LINK_MAX_PAYLOAD 200
#define LINK_NAME_LEN 12

#define LINK_RESEND_MS 400
// Resends are jittered, and that is not decoration. A fixed interval can lock
// step with a periodic interferer -- a beacon, a microwave -- so that the same
// packet in the cycle is lost every single time and the resends never help.
// The lossy test reproduces exactly that: with one frame in three dropped and a
// fixed interval, one squad packet was destroyed on all ten attempts.
#define LINK_JITTER_MS 200
#define LINK_PAIR_TIMEOUT_MS 60000    // waiting for a peer that may not exist
#define LINK_BATTLE_TIMEOUT_MS 60000  // mid-fight, where one is known to be there

// Idle at LINK_READY, nothing queued to resend: the heartbeat only needs to be
// RECEIVED, not delivered fast, so it rides its own slower cadence rather than
// LINK_RESEND_MS -- that constant stays fast for traffic someone is actually
// waiting on (a move, a result, a squad packet during pairing).
#define LINK_IDLE_PING_MS 1000

// A minute with nothing REAL happening -- as opposed to LINK_BATTLE_TIMEOUT_MS,
// which the heartbeat above keeps from ever elapsing on its own. Different
// question ("has anyone done anything") from "is the peer still there at all",
// so it gets its own name even though it is the same number today.
#define LINK_IDLE_MS 60000

// An action is a move slot, or a switch with the high bit set. One message
// rather than two, so turn matching and resend have a single path -- which is
// the place a second message type would most easily drift out of step.
//
// A move is stored as slot+1 so that 0 can keep meaning "nothing chosen yet".
// Storing the slot raw would make move slot 0 -- the first move every creature
// has -- indistinguishable from silence.
#define LINK_ACT_SWITCH 0x80
#define LINK_ACT_MOVE(i) ((uint8_t)((i) + 1))
#define LINK_ACT_SWITCH_TO(i) ((uint8_t)(LINK_ACT_SWITCH | ((i) & 0x0F)))
#define LINK_ACT_IS_SWITCH(a) (((a) & LINK_ACT_SWITCH) != 0)
#define LINK_ACT_SLOT(a) \
  ((uint8_t)(LINK_ACT_IS_SWITCH(a) ? ((a) & 0x0F) : ((a) - 1)))

enum LinkState : uint8_t {
  LINK_OFF = 0,
  LINK_LISTENING,    // waiting for a peer
  LINK_HANDSHAKE,    // hello sent, waiting for theirs
  LINK_SQUADS,       // teams being exchanged
  LINK_READY,        // both known; an action may be chosen
  LINK_WAITING,      // acted, waiting on the other side
  LINK_DONE,         // somebody won
  LINK_REFUSED,      // incompatible -- loudly, never a silent desync
  LINK_LOST,         // peer went quiet, or said goodbye
};

enum LinkMsg : uint8_t {
  LM_HELLO = 1,
  LM_SQUAD,
  LM_ACT,        // guest -> host: a move slot or a switch, stamped with a turn
  LM_RESULT,     // host -> guest: what happened, stamped with the same turn
  LM_END,
  LM_BYE,        // leaving on purpose, so the peer need not wait for a timeout
  LM_REMATCH,    // go again with the same squads
  LM_PING,       // idle heartbeat: liveness only, no payload, not `keep`d --
                 // tick() sends a fresh one each idle beat rather than
                 // resending one until "superseded" by nothing
};

// A creature on the wire. Deliberately not `Combatant` itself: that carries
// live battle state (stages, ailments, timers) which only the host owns.
struct LinkMon {
  int16_t dex;
  uint8_t level;
  uint16_t maxHp;
  uint16_t base[SI_COUNT];
  uint8_t moves[MOVE_SLOTS];
  uint8_t shiny;
  char name[LINK_NAME_LEN];
  uint8_t flags;   // PMON_CHEATED (v3.60); sat in the struct's tail padding, so the size held
};

// What the host tells the guest happened. Everything the guest needs to render
// a turn, and nothing it could use to resolve one -- it has no say in the
// outcome, which is the whole point of one side being authoritative.
//
// Every field is ABSOLUTE, not a delta. That is what lets a guest which missed
// a turn recover from the next one instead of drifting.
// One narrated beat of a turn (v3.64): an action or an end-of-turn tick, as the
// host's engine resolved it, with both sides' HP right after. The guest replays
// them through the SAME narration the host used, so the two boards read the
// same lines -- it used to see moves and faints only -- and each line's HP is
// the HP the bars drain to while it is shown.
#define LINK_BEATS 5
#define LB_MISSED 0x01
#define LB_CRIT 0x02
#define LB_IMMUNE 0x04
#define LB_SKIPPED 0x08
#define LB_HURTSELF 0x10
#define LB_CHARGED 0x20
#define LB_FAINTED 0x40
#define LB_STAGE_USER 0x80
struct LinkBeat {
  uint8_t who;          // 0 the host's creature acted (or ticked), 1 the guest's
  uint8_t kind;         // 0 an action, 1 an end-of-turn tick
  uint8_t move;
  uint8_t flags;        // LB_*
  uint8_t eff;          // 0 no damage, 1 super effective, 2 not very, 3 neutral damage
  uint8_t inflicted;
  uint8_t stageMask;
  int8_t stageDelta;
  uint16_t hostHp, guestHp;
  uint8_t sketch;       // v3.69: the move SKETCH copied, LB_SKETCH_FAILED, or 0
};
#define LB_SKETCH_FAILED 0xFF

struct LinkResult {
  uint16_t hostHp, guestHp;
  uint8_t hostAil, guestAil;
  uint8_t hostMove, guestMove;
  uint16_t hostDmg, guestDmg;
  uint8_t hostIdx, guestIdx;   // which squad member is out on each side
  uint8_t flags;               // bit0 crit, bit1 super, bit2 a faint happened
  // v3.55: the PP the guest's creature out now has left, ABSOLUTE like the
  // rest (rule 3). Only the host spends PP; the guest shows these.
  uint8_t guestPp[MOVE_SLOTS];
  // v3.57 (LINK_PROTO 5): who comes on when a side's creature fainted this turn,
  // decided by the HOST at resolution (battleNextStanding()) and shipped with
  // the turn, never worked out by the guest. Before this the host chose the
  // replacement locally after its own faint message was dismissed and nothing
  // told the guest, so the guest picked its next move from a creature that was
  // no longer out. LINK_NO_NEXT = no faint, or nobody left. The incoming one's
  // HP and ailment travel too, being absolute state (rule 3): a creature that
  // was hurt before it was switched out comes back hurt on both boards.
  uint8_t hostNext, guestNext;
  uint16_t hostNextHp, guestNextHp;
  uint8_t hostNextAil, guestNextAil;
  uint8_t beatN;               // v3.64
  LinkBeat beats[LINK_BEATS];
  // v3.69 (LINK_PROTO 9): the guest's creature out now's moves, ABSOLUTE like
  // its PP. A SKETCH the host resolved for it shows up here, and the guest
  // keeps it -- but only over a slot it really holds SKETCH in, so a host can
  // never rewrite any other move.
  uint8_t guestMoves[MOVE_SLOTS];
};
#define LINK_NO_NEXT 0xFF

struct Link {
  uint8_t state = LINK_OFF;
  bool isHost = false;
  uint8_t protoTheirs = 0;
  uint16_t buildTheirs = 0;
  uint16_t id = 0;             // breaks the tie when both sides offer to host
  uint16_t peerId = 0;
  char peerName[LINK_NAME_LEN] = "";   // write: onPacket() only. read: display only.
  char myName[LINK_NAME_LEN] = "";     // write: begin() only. read: sendHello() only.
  // The trainers' marks (LINK_MARK_*), shown on the pairing screen. Ours is set
  // by the caller after begin(); theirs arrives in the hello's flags byte.
  uint8_t myMarks = 0;
  uint8_t peerMarks = 0;

  LinkMon mine[TRAINER_TEAM_MAX];
  uint8_t mineN = 0;
  LinkMon theirs[TRAINER_TEAM_MAX];
  uint8_t theirsN = 0;
  uint8_t theirsExpected = 0;  // from their hello, so a lost SQUAD is detectable
  uint16_t theirsGot = 0;      // which slots have actually arrived

  uint8_t turn = 0;            // the exchange both sides are on
  uint8_t pendingAct = 0;      // host: the guest's action for `turn`, 0 = none
  bool youWon = false;

  // guest: the last turn the host resolved. `resultNew` is cleared by whoever
  // renders it, so a dropped or duplicated frame cannot replay a turn twice.
  uint8_t result[LINK_MAX_PAYLOAD];
  uint8_t resultN = 0;
  bool resultNew = false;

  // what we last said, kept so it can be said again until superseded
  uint8_t tx[2 + LINK_MAX_PAYLOAD];
  uint8_t txN = 0;
  bool txLive = false;
  uint32_t txAt = 0;
  uint32_t lastRx = 0;
  uint8_t resendSeq = 0;       // varies the resend interval, see LINK_JITTER_MS
  bool sawRx = false;          // onPacket has no clock; tick() stamps it
  bool armed = false;

  // Same idea as lastRx/sawRx, but LM_PING never counts -- this is "did
  // anything REAL happen", which lastRx alone can no longer answer once a
  // heartbeat exists to keep it fresh forever on its own.
  uint32_t lastActivity = 0;
  bool sawActivity = false;

  // set by whoever owns the radio; ctx lets a test route two Links to each other
  void (*send)(void *ctx, const uint8_t *buf, uint8_t len) = nullptr;
  void *ctx = nullptr;

  void begin(bool host, const char *name);
  void addMon(const LinkMon &m);
  void start();                       // announce: hello, then squad
  void onPacket(const uint8_t *buf, uint8_t len);
  void tick(uint32_t now);            // resend + timeout; call once a frame

  void sendAct(uint8_t act);          // guest -> host: a move slot or a switch
  void sendResult(const uint8_t *blob, uint8_t len);   // host -> guest
  void sendEnd(bool hostWon);
  void sendBye();                     // leaving deliberately
  void sendRematch();
  void rearm();                       // same squads, fresh fight

  bool ready() const { return state == LINK_READY; }
  bool live() const {
    return state != LINK_OFF && state != LINK_REFUSED && state != LINK_LOST;
  }
  // the host's own action is its own business; this is the guest's, and it
  // only counts once it has actually arrived
  bool hasPeerAct() const { return isHost && pendingAct != 0; }
};

// Everything that must match for two builds to narrate the same fight. NOT the
// firmware version: a build differing only in, say, a colour is perfectly
// compatible and refusing it would be noise. These are the things that would
// actually make one device read a packet differently from the other.
uint16_t linkBuildTag();

// Builds the wire form of a combatant, so both ends agree on what a creature is.
// linkMonTo VALIDATES: everything it produces is safe to index a table with,
// whatever actually arrived over the air.
void linkMonFrom(LinkMon &out, const Combatant &c);
void linkMonTo(Combatant &out, const LinkMon &m);
uint8_t linkSafeMove(uint8_t m);      // 0 unless it is a real move
