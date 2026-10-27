#pragma once
#include <Arduino.h>
#include "pet.h"
#include "party.h"
#include "moves.h"

// Turn resolution. Deliberately free of any UI or hardware so the whole thing
// can be simulated headlessly -- see tools/emu.
//
// Integer maths only, like types.h: no floats anywhere on the MCU.

enum : uint8_t { SI_ATK = 0, SI_DEF, SI_SPA, SI_SPD, SI_SPE, SI_COUNT };

// A creature as it exists inside a battle. Built from the live pet or from a
// banked party member; nothing here is ever written back, which is what keeps
// ailments battle-only.
struct Combatant {
  int16_t dex = 0;
  uint8_t level = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t base[SI_COUNT] = { 1, 1, 1, 1, 1 };  // before stat stages
  uint8_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  int8_t stage[SI_COUNT] = { 0, 0, 0, 0, 0 };   // -6..+6
  uint8_t ailment = AIL_NONE;
  uint8_t ailTurns = 0;      // sleep/freeze countdown
  uint8_t confuseTurns = 0;  // confusion runs alongside a real ailment
  bool recharge = false;     // EF_RECHARGE spent this creature's next turn
  uint8_t charging = 0;      // EF_CHARGE move already wound up
  bool shiny = false;        // which sprite variant to stream
  bool cheated = false;      // carries PMON_CHEATED: the plates show the mark (v3.60)
  char name[12] = "";
  // PP (v3.52). linkMonTo() builds a bare Combatant (ppOn false); a LAN fight
  // turns it on full with battleFullPp() (v3.55) and never touches the saved
  // PP book -- only the host spends, and LinkResult carries the guest's.
  uint8_t pp[MOVE_SLOTS] = { 0, 0, 0, 0 };
  bool ppOn = false;
  CreatureUid uid;           // whose PP book entry; invalid for an opponent
  // The last move this creature used in the fight, for the foe's SKETCH to
  // copy (v3.69). Per creature for the whole fight; 0 = none yet.
  uint8_t lastMove = 0;

  bool fainted() const { return hp == 0; }
};

// PP: every move full and counted -- a fresh fight's start (a LAN fight's
// combatants are built bare by linkMonTo(), and count PP from v3.55).
void battleFullPp(Combatant &c);
// PP: can slot i be chosen? (a real move with PP left, or PP not counted)
bool battleSlotReady(const Combatant &c, int slot);
// No move can be chosen -- every one is out of PP, or the creature knows none
// at all (an Abra: TELEPORT is not in our move list) -- so the only thing left
// is STRUGGLE.
bool battleMustStruggle(const Combatant &c);
// THE move a chosen slot means, for every side that picks one: the screen, the
// host reading the guest's action off the wire, a test. STRUGGLE when nothing
// can be chosen, else the move in that slot (spendPp() still turns an empty one
// into STRUGGLE). One answer, so a moveless creature cannot be stuck on one
// path and fighting on another.
uint8_t battleMoveForSlot(const Combatant &c, int slot);
// The squad member to send in when `at` faints: the first one still standing
// after it, wrapping round, never `at` itself; -1 when nobody is left. By
// standing, not by index -- after a switch the next index can be fainted while
// an earlier one is not.
int8_t battleNextStanding(const Combatant *squad, uint8_t n, uint8_t at);

void combatantFromPet(Combatant &c, const Pet &p);
void combatantFromParty(Combatant &c, const PartyMon &m);

// What one action did, so the UI can narrate it without recomputing anything.
struct TurnLog {
  uint8_t move = 0;
  uint16_t damage = 0;
  uint16_t effPct = 100;   // type effectiveness, percent
  uint8_t hits = 0;        // >1 for EF_MULTI
  uint8_t inflicted = AIL_NONE;
  int8_t stageDelta = 0;   // for EF_STAGE, so the UI can say "rose sharply"
  uint8_t stageMask = 0;
  bool stageOnUser = false; // the stages above landed on the attacker, not the target
  bool missed = false;
  bool crit = false;
  bool immune = false;     // type chart says it does nothing
  bool skipped = false;    // asleep, frozen, fully paralysed or recharging
  bool hurtSelf = false;   // confusion
  bool charged = false;    // spent the turn winding up
  bool healed = false;
  bool targetFainted = false;
  // SKETCH (v3.69): the move it copied, into which slot -- the caller keeps it
  // on the creature for good (sketchKeep() in the sketch) -- or that it failed.
  uint8_t sketched = 0;
  int8_t sketchSlot = -1;
  bool failed = false;
};

// SKETCH's one rule: what it copies from `def` into `atk`, or 0 when it fails
// (the foe has not moved, used STRUGGLE or SKETCH, or `atk` already knows it).
uint8_t battleSketchTarget(const Combatant &atk, const Combatant &def);

uint16_t stagedStat(uint16_t base, int8_t stage);
uint16_t battleDamage(const Combatant &atk, const Combatant &def, uint8_t mv,
                      bool crit, uint8_t roll);

// True if `a` using `ma` acts before `b` using `mb`. Priority first, then the
// staged speed, and paralysis halves it.
bool battleMovesFirst(const Combatant &a, uint8_t ma,
                      const Combatant &b, uint8_t mb);

// One creature's action. Applies damage, ailments, stat stages and recoil.
void battleAct(Combatant &atk, Combatant &def, uint8_t mv, TurnLog &log);

// Burn and poison chip damage, applied to one creature after both have acted.
void battleEndTurn(Combatant &c, TurnLog &log);

// Picks the attacker's move. `smart` is what separates hard mode from easy:
// easy picks uniformly at random, hard reads the type chart, accuracy, whether
// a move kills this turn, and whether a status or a stat boost is worth the
// turn it costs.
uint8_t aiChooseMove(const Combatant &self, const Combatant &foe, bool smart);
