#pragma once
// PP (v3.52): how many times each of a creature's moves can still be used.
//
// The owner's rules (2026-10-22, docs/next-phase-plan.md section 3):
//   - every move has its real PP (MoveEntry::pp, from PokeAPI);
//   - PP carries over from battle to battle and across restarts, and the WHOLE
//     team's comes back when the live pet's energy fills up while it sleeps
//     (Pet::drainMinute(), the one place energy rises);
//   - a newly learned move starts full; opponents always start full; a LAN
//     battle starts everyone full and never touches what is saved here.
//
// THE BOOK stores what has been USED, per creature and per MOVE (not per slot):
//   - a creature with nothing used has no entry at all, so every creature that
//     existed before PP, and every one received in a trade, reads as full;
//   - rearranging moves cannot refill them, and a move the creature did not
//     have before simply has no count -- full, as the games start a new move;
//   - it is keyed by CreatureUid, so PP follows a creature through the party,
//     the box and swaps, and parking a tired creature in the box refills
//     nothing.
//
// One NVS key, "pp" (in SAVE_FIELDS). PP_BOOK_CAP covers the live pet, the
// party and the box with room to spare; when it is full the entry used least
// recently is dropped, which only ever HANDS BACK PP -- the generous direction.
#include <stdint.h>
#include "party.h"
#include "moves.h"

#define PP_BOOK_CAP 28

struct PpEntry {
  CreatureUid uid;
  uint8_t move[MOVE_SLOTS];   // which move each count belongs to
  uint8_t used[MOVE_SLOTS];   // how many of its PP are gone
};
static_assert(sizeof(PpEntry) == 20, "PpEntry is on-disk format");

// Full PP for a move: its table value, never 0 for a real move.
uint8_t ppMax(uint8_t move);

// PP Ups (v3.92), the games' rule: each adds a fifth of the move's base PP,
// rounded down, up to 3 (35 PP -> 42, 49, 56). A creature's Ups are packed two
// bits a move SLOT (PartyMon::reserved[1], Pet::ppUps, Combatant::ppUps): the
// slot keeps them if its move changes, for good: any move put in that slot gets
// the bonus. ACCEPTED BY THE OWNER (2026-10-30) as a storage limit, not the
// intended rule -- PP Ups should belong to the MOVE. A creature stores only its
// four move bytes (all 8 bits used) and one spare byte, so a per-move record
// does not fit; if PartyMon ever grows, see CLAUDE.md "PP Up: per slot".
#define PP_UPS_MAX 3
uint8_t ppMaxFor(uint8_t move, uint8_t ups);
inline uint8_t ppUpsAt(uint8_t packed, int slot) { return (uint8_t)((packed >> (2 * slot)) & 3); }
// One more Up on a slot. False if the slot has no real move or is at PP_UPS_MAX.
bool ppUpAdd(uint8_t &packed, const uint8_t moves[MOVE_SLOTS], int slot);

// What is left of `move` for creature `uid`. An invalid uid (an egg, a foe)
// is always full.
uint8_t ppLeft(const CreatureUid &uid, uint8_t move, uint8_t ups = 0);
// The same for the move in one SLOT: a move held in several slots (SKETCH, up
// to four, v3.69) keeps a count per slot. Falls back to ppLeft() when the
// book's entry has another move in that slot.
uint8_t ppLeftSlot(const CreatureUid &uid, uint8_t slot, uint8_t move, uint8_t ups = 0);

// Records what a battle left: `moves` and the PP still LEFT on each. Called
// after every turn, so no ending path can lose it -- but only in RAM: a turn
// is not worth a flash write. ppFlush() writes it, from loop() once the fight
// is over and from flushBeforePowerLoss().
// A move at full PP stores nothing; an entry with nothing used is removed.
// `ups` is the creature's packed PP Ups: what is USED is counted from its max.
void ppRecord(const CreatureUid &uid, const uint8_t moves[MOVE_SLOTS],
              const uint8_t left[MOVE_SLOTS], uint8_t ups = 0);

// Everyone's PP back. Returns true if anything was refilled (so a caller can
// tell a rest that mattered from one that did not).
bool ppRefillAll();

// Persistence. ppLoad() at boot; ppFlush() writes anything recorded since
// (a no-op when nothing changed); ppRefillAll() writes at once.
void ppLoad();
bool ppDirty();
void ppFlush();
uint8_t ppEntryCount();    // for tests and the console
