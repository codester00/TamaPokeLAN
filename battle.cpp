#include "battle.h"
#include "pp.h"
#include "dex.h"
#include "types.h"

// ---------- building a combatant ----------

static void fill(Combatant &c, int16_t dex, uint8_t lvl, uint16_t hp,
                 uint16_t a, uint16_t d, uint16_t sa, uint16_t sd, uint16_t sp) {
  c = Combatant();
  c.dex = dex;
  c.level = lvl;
  c.maxHp = hp ? hp : 1;
  c.hp = c.maxHp;
  c.base[SI_ATK] = a; c.base[SI_DEF] = d;
  c.base[SI_SPA] = sa; c.base[SI_SPD] = sd; c.base[SI_SPE] = sp;
}

void combatantFromPet(Combatant &c, const Pet &p) {
  fill(c, p.speciesId, p.level(), p.vitStat(), p.atkStat(), p.defStat(),
       p.spaStat(), p.spdStat(), p.speStat());
  for (int i = 0; i < MOVE_SLOTS; i++) c.moves[i] = p.moves[i];
  c.shiny = p.shiny;
  c.cheated = p.cheated;
  const char *nm = p.nick[0] ? p.nick : DEX_TBL[p.speciesId].name;
  snprintf(c.name, sizeof(c.name), "%s", nm);
  // PP starts full; the player's side then reads what is left from the PP
  // book (buildSquad), opponents keep it full.
  c.ppUps = p.ppUps;
  for (int i = 0; i < MOVE_SLOTS; i++) c.pp[i] = ppMaxFor(c.moves[i], ppUpsAt(c.ppUps, i));
  c.ppOn = true;
  c.uid = p.uid;
}

void combatantFromParty(Combatant &c, const PartyMon &m) {
  fill(c, m.dex, (uint8_t)m.level, party.vitOf(m), party.atkOf(m), party.defOf(m),
       party.spaOf(m), party.spdOf(m), party.speOf(m));
  for (int i = 0; i < MOVE_SLOTS; i++) c.moves[i] = m.moves[i];
  c.shiny = m.shiny != 0;
  c.cheated = (m.flags & PMON_CHEATED) != 0;
  const char *nm = m.nick[0] ? m.nick : DEX_TBL[m.dex].name;
  snprintf(c.name, sizeof(c.name), "%s", nm);
  // PP starts full; the player's side then reads what is left from the PP
  // book (buildSquad), opponents keep it full.
  c.ppUps = m.reserved[1];
  for (int i = 0; i < MOVE_SLOTS; i++) c.pp[i] = ppMaxFor(c.moves[i], ppUpsAt(c.ppUps, i));
  c.ppOn = true;
  c.uid = m.uid;
}

// ---------- stat stages ----------

// The series' own table, as a fraction so it stays integer: +1 is 3/2, -1 is
// 2/3, and so on out to +6 = 4x and -6 = 1/4.
uint16_t stagedStat(uint16_t base, int8_t stage) {
  if (stage > 6) stage = 6;
  if (stage < -6) stage = -6;
  uint16_t num = 2 + (stage > 0 ? stage : 0);
  uint16_t den = 2 + (stage < 0 ? -stage : 0);
  uint32_t v = (uint32_t)base * num / den;
  return v < 1 ? 1 : (v > 65535 ? 65535 : (uint16_t)v);
}

static uint16_t effStat(const Combatant &c, uint8_t idx) {
  uint16_t v = stagedStat(c.base[idx], c.stage[idx]);
  // burn halves physical attack, paralysis halves speed -- the two ailments
  // that do something beyond chip damage
  if (idx == SI_ATK && c.ailment == AIL_BURN) v = v / 2 ? v / 2 : 1;
  if (idx == SI_SPE && c.ailment == AIL_PARA) v = v / 2 ? v / 2 : 1;
  return v;
}

// ---------- damage ----------

// roll is 217..255, the series' damage spread, passed in so tests can pin it.
uint16_t battleDamage(const Combatant &atk, const Combatant &def, uint8_t mv,
                      bool crit, uint8_t roll) {
  if (!mv || mv >= MOVE_COUNT) return 0;
  const MoveEntry &m = MOVE_TBL[mv];
  if (m.cat == MC_STATUS) return 0;

  if (m.effect == EF_FIXED_LVL) return atk.level ? atk.level : 1;
  if (m.effect == EF_FIXED) return m.param > 0 ? (uint16_t)m.param : 1;

  uint16_t A, D;

  if (crit) {
    uint8_t aIdx = (m.cat == MC_PHYS) ? SI_ATK : SI_SPA;
    uint8_t dIdx = (m.cat == MC_PHYS) ? SI_DEF : SI_SPD;

    // A critical hit ignores the attacker's negative stages and the defender's
    // positive ones -- so a Barrier cannot make you immune to a lucky roll --
    // but keeps the stages that favour the attacker, and burn still halves it.
    // That is the series' rule from Gen 2 on. (v3.28 and earlier dropped EVERY
    // stage and the burn, which was Gen 1's rule while this comment described
    // Gen 2's.)
    int8_t aStage = atk.stage[aIdx] > 0 ? atk.stage[aIdx] : 0;
    A = stagedStat(atk.base[aIdx], aStage);
    if (aIdx == SI_ATK && atk.ailment == AIL_BURN) A = A / 2 ? A / 2 : 1;

    int8_t dStage = def.stage[dIdx] < 0 ? def.stage[dIdx] : 0;
    D = stagedStat(def.base[dIdx], dStage);
  } else {
    A = (m.cat == MC_PHYS) ? effStat(atk, SI_ATK) : effStat(atk, SI_SPA);
    D = (m.cat == MC_PHYS) ? effStat(def, SI_DEF) : effStat(def, SI_SPD);
  }

  if (!D) D = 1;

  uint32_t dmg = (2UL * atk.level / 5 + 2) * m.power * A / D / 50 + 2;
  // 1.5x, as from Gen 6 on. Gens 1-5 used 2x; the rest of this engine (an
  // 18-type chart, the per-move physical/special split, the 1/16 crit rate)
  // already follows Gen 6, so this keeps the rules one generation's.
  if (crit) dmg = dmg * 3 / 2;
  if (hasStab(atk.dex, m.type)) dmg = dmg * 3 / 2;

  uint16_t eff = typeEffVsDex(m.type, def.dex);
  dmg = dmg * eff / 100;
  if (eff == 0) return 0;               // immune: no chip, no minimum
  dmg = dmg * roll / 255;

  return dmg < 1 ? 1 : (dmg > 65535 ? 65535 : (uint16_t)dmg);
}

// ---------- turn order ----------

bool battleMovesFirst(const Combatant &a, uint8_t ma,
                      const Combatant &b, uint8_t mb) {
  int pa = (ma && ma < MOVE_COUNT && MOVE_TBL[ma].effect == EF_PRIORITY)
               ? MOVE_TBL[ma].param : 0;
  int pb = (mb && mb < MOVE_COUNT && MOVE_TBL[mb].effect == EF_PRIORITY)
               ? MOVE_TBL[mb].param : 0;
  if (pa != pb) return pa > pb;

  uint16_t sa = effStat(a, SI_SPE), sb = effStat(b, SI_SPE);
  if (sa != sb) return sa > sb;
  return random(2) == 0;                // a genuine speed tie is a coin flip
}

// ---------- one action ----------

static void applyStages(Combatant &c, uint8_t mask, int8_t delta) {
  static const uint8_t BIT[SI_COUNT] = { ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE };
  for (int i = 0; i < SI_COUNT; i++) {
    if (!(mask & BIT[i])) continue;
    int v = c.stage[i] + delta;
    c.stage[i] = v > 6 ? 6 : (v < -6 ? -6 : (int8_t)v);
  }
}

static void hurt(Combatant &c, uint16_t amount) {
  c.hp = (amount >= c.hp) ? 0 : c.hp - amount;
}

static void heal(Combatant &c, uint16_t amount) {
  uint32_t v = (uint32_t)c.hp + amount;
  c.hp = v > c.maxHp ? c.maxHp : (uint16_t)v;
}

// ---------- PP ----------

void battleFullPp(Combatant &c) {
  for (int i = 0; i < MOVE_SLOTS; i++) c.pp[i] = ppMaxFor(c.moves[i], ppUpsAt(c.ppUps, i));
  c.ppOn = true;
}

bool battleSlotReady(const Combatant &c, int slot) {
  if (slot < 0 || slot >= MOVE_SLOTS) return false;
  uint8_t mv = c.moves[slot];
  if (!mv || mv >= MOVE_COUNT) return false;
  return !c.ppOn || c.pp[slot] > 0;
}

bool battleMustStruggle(const Combatant &c) {
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (battleSlotReady(c, i)) return false;
  return true;
}

uint8_t battleMoveForSlot(const Combatant &c, int slot) {
  if (battleMustStruggle(c)) return MV_STRUGGLE;
  if (slot < 0 || slot >= MOVE_SLOTS) return 0;
  uint8_t mv = c.moves[slot];
  return mv < MOVE_COUNT ? mv : 0;
}

int8_t battleNextStanding(const Combatant *squad, uint8_t n, uint8_t at) {
  for (uint8_t k = 1; k < n; k++) {
    uint8_t i = (uint8_t)((at + k) % n);
    if (!squad[i].fainted()) return (int8_t)i;
  }
  return -1;
}

// The ONE place PP is spent, and the one place a move with none left is
// refused: whatever chose it (the screen, the AI, a test), a move at 0 PP
// becomes STRUGGLE here. A move the creature does not know (STRUGGLE itself)
// costs nothing.
//
// A move can sit in more than one slot -- SKETCH, up to four (v3.69) -- so the
// slot spent is one with PP left, and `slot` says which (-1: none of its own).
static uint8_t spendPp(Combatant &c, uint8_t mv, int8_t &slot) {
  slot = -1;
  if (!mv) return mv;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (c.moves[i] != mv) continue;
    if (slot < 0) slot = (int8_t)i;
    if (!c.ppOn || c.pp[i]) { slot = (int8_t)i; break; }
  }
  if (!c.ppOn || slot < 0) return mv;
  if (!c.pp[slot]) return MV_STRUGGLE;
  c.pp[slot]--;
  return mv;
}

uint8_t battleSketchTarget(const Combatant &atk, const Combatant &def) {
  uint8_t t = def.lastMove;
  if (!t || t >= MOVE_COUNT || t == MV_STRUGGLE || t == MV_SKETCH) return 0;
  for (int i = 0; i < MOVE_SLOTS; i++) if (atk.moves[i] == t) return 0;
  return t;
}

void battleAct(Combatant &atk, Combatant &def, uint8_t mv, TurnLog &log) {
  log = TurnLog();
  log.move = mv;
  if (atk.fainted() || def.fainted()) { log.skipped = true; return; }

  // --- things that can cost the turn before a move is even chosen
  if (atk.recharge) { atk.recharge = false; log.skipped = true; return; }
  if (atk.ailment == AIL_FREEZE) {
    if (random(100) < 20) atk.ailment = AIL_NONE;   // thaws
    else { log.skipped = true; return; }
  }
  if (atk.ailment == AIL_SLEEP) {
    if (atk.ailTurns) atk.ailTurns--;
    if (atk.ailTurns == 0) atk.ailment = AIL_NONE;
    else { log.skipped = true; return; }
  }
  if (atk.ailment == AIL_PARA && random(100) < 25) { log.skipped = true; return; }
  if (atk.confuseTurns) {
    atk.confuseTurns--;
    if (random(100) < 33) {               // hits itself instead
      uint16_t self = (2UL * atk.level / 5 + 2) * 40 *
                          atk.base[SI_ATK] / (atk.base[SI_DEF] ? atk.base[SI_DEF] : 1) / 50 + 2;
      hurt(atk, self);
      log.hurtSelf = true;
      log.damage = self;
      return;
    }
  }

  // a wound-up EF_CHARGE move fires this turn instead of whatever was picked,
  // and costs nothing more: its PP went when it was wound up
  int8_t slot = -1;
  if (atk.charging) { mv = atk.charging; atk.charging = 0; }
  else {
    mv = spendPp(atk, mv, slot);
    if (mv && mv < MOVE_COUNT && MOVE_TBL[mv].effect == EF_CHARGE) {
      atk.charging = mv;
      log.move = mv;
      log.charged = true;
      return;
    }
  }

  if (!mv || mv >= MOVE_COUNT) { log.skipped = true; return; }
  const MoveEntry &m = MOVE_TBL[mv];
  log.move = mv;

  // SKETCH: the slot it was used from becomes the foe's last move, with full
  // PP. Read before this turn's own move is recorded, so it copies what the
  // foe did, never itself.
  if (m.effect == EF_SKETCH) {
    uint8_t t = battleSketchTarget(atk, def);
    atk.lastMove = mv;
    if (!t || slot < 0) { log.failed = true; return; }
    atk.moves[slot] = t;
    atk.pp[slot] = ppMaxFor(t, ppUpsAt(atk.ppUps, slot));   // the slot keeps its PP Ups
    log.sketched = t;
    log.sketchSlot = slot;
    return;
  }
  atk.lastMove = mv;   // what a SKETCH facing it copies, hit or miss

  // --- accuracy. acc 0 means it cannot miss (SWIFT, and every status move)
  if (m.acc && m.effect != EF_NEVER_MISS && random(100) >= m.acc) {
    log.missed = true;
    return;
  }

  if (m.cat == MC_STATUS) {
    if (m.effect == EF_HEAL) {
      heal(atk, (uint32_t)atk.maxHp * (m.param > 0 ? m.param : 50) / 100);
      log.healed = true;
    } else if (m.effect == EF_STAGE) {
      Combatant &t = (m.target == TG_SELF) ? atk : def;
      applyStages(t, m.statMask, m.stages);
      log.stageMask = m.statMask;
      log.stageDelta = m.stages;
      log.stageOnUser = (m.target == TG_SELF);
    }
    return;
  }

  // --- damage, including multi-hit. The total is 32-bit: a multi-hit move
  // summing five big hits could wrap a uint16_t.
  uint8_t hits = (m.effect == EF_MULTI) ? (uint8_t)(2 + random(4)) : 1;
  uint32_t total = 0;
  log.effPct = typeEffVsDex(m.type, def.dex);
  if (log.effPct == 0) { log.immune = true; return; }

  for (uint8_t h = 0; h < hits; h++) {
    bool crit = random(16) == 0;                 // ~6%, the series' base rate
    uint16_t d = battleDamage(atk, def, mv, crit, (uint8_t)(217 + random(39)));
    total += d;
    if (crit) log.crit = true;
    hurt(def, d);
    if (def.fainted()) { hits = h + 1; break; }
  }

  log.hits = hits;
  log.damage = total > 65535 ? 65535 : (uint16_t)total;

  // recoil and drain need damage actually dealt -- a zero-damage hit used to
  // still cost the attacker its 1 HP minimum of recoil
  if (m.effect == EF_RECOIL && m.param > 0 && total > 0) {
    uint32_t rec = total / m.param;
    hurt(atk, rec ? (uint16_t)rec : 1);
  }
  if (m.effect == EF_DRAIN && m.param > 0 && total > 0) {
    heal(atk, (uint16_t)(total * m.param / 100));
  }
  if (m.effect == EF_RECHARGE) atk.recharge = true;

  // --- secondary ailment. Never overwrites an existing one, and confusion is
  // tracked separately so it can stack with a real status, as in the games.
  if (m.ailment != AIL_NONE && m.ailChance && !def.fainted() &&
      random(100) < m.ailChance) {
    if (m.ailment == AIL_CONFUSE) {
      if (!def.confuseTurns) {
        def.confuseTurns = 2 + random(3);
        log.inflicted = AIL_CONFUSE;
      }
    } else if (def.ailment == AIL_NONE) {
      // a type cannot be given the status it is made of, and Steel cannot be
      // poisoned either
      bool immune = (m.ailment == AIL_BURN && hasStab(def.dex, T_FIRE)) ||
                    (m.ailment == AIL_FREEZE && hasStab(def.dex, T_ICE)) ||
                    (m.ailment == AIL_POISON && (hasStab(def.dex, T_POISON) || hasStab(def.dex, T_STEEL))) ||
                    (m.ailment == AIL_PARA && hasStab(def.dex, T_ELECTRIC));
      if (!immune) {
        def.ailment = m.ailment;
        if (m.ailment == AIL_SLEEP) def.ailTurns = 2 + random(3);
        log.inflicted = m.ailment;
      }
    }
  }
  // --- a damaging move's own stat change (Bubble Beam, Close Combat...). On
  // the user it happens whatever became of the target, as in the games; on the
  // target only while it is still standing.
  if (m.hitMask && total > 0 && random(100) < m.hitChance) {
    bool onUser = (m.hitTarget == TG_SELF);
    Combatant &t = onUser ? atk : def;
    if (!t.fainted()) {
      applyStages(t, m.hitMask, m.hitStages);
      log.stageMask = m.hitMask;
      log.stageDelta = m.hitStages;
      log.stageOnUser = onUser;
    }
  }
  log.targetFainted = def.fainted();
}

// ---------- end of turn ----------

void battleEndTurn(Combatant &c, TurnLog &log) {
  log = TurnLog();
  if (c.fainted()) return;
  if (c.ailment == AIL_BURN || c.ailment == AIL_POISON) {
    uint16_t chip = c.maxHp / 16;
    if (!chip) chip = 1;
    hurt(c, chip);
    log.damage = chip;
    log.inflicted = c.ailment;
    log.targetFainted = c.fainted();
  }
}

// ---------- move choice ----------

uint8_t aiChooseMove(const Combatant &self, const Combatant &foe, bool smart) {
  uint8_t legal[MOVE_SLOTS], n = 0;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (battleSlotReady(self, i)) legal[n++] = self.moves[i];
  if (!n) return battleMustStruggle(self) ? MV_STRUGGLE : 0;
  if (!smart) return legal[random(n)];

  // 32-bit on purpose: a kill bonus on top of real damage passes 32767, and an
  // int16_t best score wrapped negative and made the AI skip its best move.
  int32_t bestScore = INT32_MIN;
  uint8_t best = legal[0];

  for (uint8_t i = 0; i < n; i++) {
    uint8_t mv = legal[i];
    const MoveEntry &m = MOVE_TBL[mv];
    int32_t sc = 0;

    if (m.cat == MC_STATUS) {
      // A status move costs a whole turn, so it has to buy more than chip
      // damage would. Healing is worth it only when actually hurt; a boost is
      // worth it early and worthless once the stage is already stacked.
      if (m.effect == EF_HEAL) {
        // Worth it only when actually hurt. Healing forever is stopped by PP
        // (RECOVER has 5), not by the AI second-guessing itself (v3.55: the
        // v3.52 heal rules were removed by the owner's call).
        int32_t missing = (int32_t)self.maxHp - self.hp;
        sc = (missing * 100 / (self.maxHp ? self.maxHp : 1)) - 30;
      } else if (m.effect == EF_STAGE) {
        static const uint8_t BIT[SI_COUNT] = { ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE };
        int stacked = 0, hit = 0;
        const Combatant &t = (m.target == TG_SELF) ? self : foe;
        for (int k = 0; k < SI_COUNT; k++)
          if (m.statMask & BIT[k]) { stacked += t.stage[k] * (m.stages > 0 ? 1 : -1); hit++; }
        if (!hit) hit = 1;

        // diminishing: +2 ATK is strong at stage 0, pointless at +6
        sc = 26 - (stacked * 12 / hit);
        // and never set up when one more hit would finish you
        if (self.hp * 3 < self.maxHp) sc -= 40;
      } else if (m.effect == EF_SKETCH) {
        sc = battleSketchTarget(self, foe) ? 40 : -100;   // nothing to copy: wasted
      } else {
        sc = 5;
      }
    } else {
      uint16_t dmg = battleDamage(self, foe, mv, false, 236);  // average roll
      sc = dmg;
      if (dmg >= foe.hp) sc += 1000;              // a kill this turn beats all
      uint8_t acc = m.acc ? m.acc : 100;
      sc = sc * acc / 100;                        // discount what tends to miss
      if (m.effect == EF_RECHARGE) sc -= dmg / 4; // a free turn for the foe
      if (m.effect == EF_RECOIL) sc -= dmg / 6;
      if (m.effect == EF_CHARGE) sc -= dmg / 3;   // a turn spent winding up
      // the side effects only count when the hit lands at all: a move the
      // target is immune to must never win on its rider
      if (dmg && m.ailment != AIL_NONE && foe.ailment == AIL_NONE)
        sc += m.ailChance / 4;
      // a stat change that comes with the hit: good for us when it lowers the
      // foe or raises the user, a cost when it lowers the user (Close Combat)
      if (dmg && m.hitMask) {
        int good = (m.hitTarget == TG_SELF) ? m.hitStages : -m.hitStages;
        sc += good * (int32_t)m.hitChance / 8;
      }
    }

    if (sc > bestScore) { bestScore = sc; best = mv; }
  }
  return best;
}
