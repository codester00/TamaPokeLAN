// The whole Pokedex through the firmware's own evolution rules (v3.85 audit).
//
// Every species that can be drawn is raised as the player would raise it --
// by level with good care, by each stone from the bag, by trade (or the
// Linking Cord), with Applin's and Charcadet's choices, Meltan's walk, Eevee's
// bond by day, by night and knowing a Fairy move -- under several creature IDs
// so both genders and both ID splits come up. What it actually BECOMES is then
// held against the evolution tables, both ways:
//   - every route the tables list really happens (or is refused because its
//     target cannot be drawn, which is the rule);
//   - nothing evolves into what no table lists;
//   - no level evolution is due again at the level it just evolved at (the
//     "evolves twice at once" chains of plan § E11);
//   - at level 100 nothing still waits on a level evolution, and a creature
//     whose evolutions all need an item or a trade does not hold back its
//     farewell (evolutionAhead());
// and every drawable species can be OBTAINED: hatched, met in Explore (an
// area, a legendary, a one-off, a swarm), a Game Corner prize, or evolved from
// one of those -- Shedinja too.
//
// The rules are never restated here: each answer is the firmware's.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "wild.h"
#include "wild_areas.h"     // a test may read the tables directly, for comparison
#include "gamecorner.h"
#include <cstdio>
#include <cstring>
#include <set>
#include <map>
#include <vector>
uint32_t g_seed = 0xA0D17; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const int ID_VARIANTS = 12;
static uint8_t gFairy = 0;   // a Fairy move, for Sylveon

// A creature of `dex`, the v-th ID, `lvl`, fully cared for and awake.
static void raise(Pet &p, int16_t dex, int v, uint8_t lvl) {
  p.dbgHatchAs(dex, false);
  for (int i = 0; i <= v; i++) p.uid = p.mintUid();
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.energy = p.fullness = p.joy = p.hygiene = 100;
  p.careMistakes = 0;
  p.sleeping = false;
  p.tradeEvo = false;
  p.evoSteps = 0;
  p.bond = 0;
}

int main() {
  gRegionArt = 0xFFFF;
  for (int m = 1; m < MOVE_COUNT; m++) if (MOVE_TBL[m].type == T_FAIRY && MOVE_TBL[m].power) { gFairy = (uint8_t)m; break; }

  // ---- what the tables say each species becomes -----------------------------------
  std::map<int16_t, std::set<int16_t>> expect;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (d == DEX_EEVEE) { for (int i = 0; i < EEVEE_EVO_COUNT; i++) expect[d].insert(EEVEE_EVOS[i]); }
    else if (DEX_TBL[d].evolvesTo) expect[d].insert(DEX_TBL[d].evolvesTo);
  }
  for (int i = 0; i < ITEM_EVO_COUNT; i++) expect[ITEM_EVOS[i].from].insert(ITEM_EVOS[i].to);
  for (int i = 0; i < GENDER_EVO_COUNT; i++) expect[GENDER_EVOS[i].from].insert(GENDER_EVOS[i].to);
  for (int i = 0; i < BRANCH_EVO_COUNT; i++)
    if (BRANCH_EVOS[i].kind != BR_SHED) expect[BRANCH_EVOS[i].from].insert(BRANCH_EVOS[i].to);
  for (int i = 0; i < TRADE_EVO_COUNT; i++) expect[TRADE_EVOS[i].from].insert(TRADE_EVOS[i].to);

  // ---- what the firmware actually does ------------------------------------------
  std::map<int16_t, std::set<int16_t>> got;
  std::set<int16_t> shedFrom;
  std::vector<std::string> doubles, stuck, held;
  int species = 0, routes = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (!speciesShowable(d)) continue;
    species++;
    for (int v = 0; v < ID_VARIANTS; v++) {
      // by level (and Eevee's bond, Meltan's walk), with the choice where offered
      for (int flavour = 0; flavour < (d == DEX_EEVEE ? 3 : 1); flavour++) {
        int16_t choices[4];
        uint8_t nc = 1;
        for (uint8_t pick = 0; pick < nc; pick++) {
          Pet p;
          raise(p, d, v, 1);
          if (d == DEX_EEVEE) {
            p.bond = 100;
            p.lastSeenEpoch = flavour == 1 ? 1767225600UL : 1767225600UL + 12 * 3600;   // 00:00 / 12:00
            if (flavour == 2) p.moves[0] = gFairy;
          }
          p.evoSteps = 100;             // Meltan's 10,000 steps (others ignore it)
          for (uint8_t L = 1; L <= MAX_LEVEL; L++) {
            p.ageMinutes = (uint32_t)(L - 1) * MINUTES_PER_LEVEL;
            if (!p.canEvolveNow()) continue;
            nc = p.evoChoices(choices);
            int16_t before = p.speciesId;
            if (nc > 1) p.evolveChoice(choices[pick]); else { nc = 1; p.evolve(); }
            if (p.speciesId == before) break;
            routes++;
            got[d].insert(p.speciesId);
            if (p.shedPending) shedFrom.insert(d);
            Pet::EvoInfo e2 = p.evoInfo();
            if (e2.status == Pet::EVO_READY && !e2.byTrade && p.canEvolveNow()) {
              char b[96];
              snprintf(b, sizeof(b), "%s -> %s at L%u, then due again at once", DEX_TBL[d].name,
                       DEX_TBL[p.speciesId].name, L);
              doubles.push_back(b);
            }
            break;
          }
          if (p.speciesId == d) {
            // never evolved by level: at 100 nothing may still be waiting on one,
            // and the farewell must not wait for an item or a trade
            p.ageMinutes = (uint32_t)(MAX_LEVEL - 1) * MINUTES_PER_LEVEL;
            uint8_t st = p.evoInfo().status;
            if (st == Pet::EVO_LEVELS || st == Pet::EVO_READY || st == Pet::EVO_BLOCKED) {
              if (p.evoTargetShowable()) {
                char b[96]; snprintf(b, sizeof(b), "%s (ID %d) at L100: status %u", DEX_TBL[d].name, v, st);
                stuck.push_back(b);
              }
            }
            if ((st == Pet::EVO_NEEDS_ITEM || st == Pet::EVO_NEEDS_TRADE || st == Pet::EVO_FINAL) &&
                p.evolutionAhead() && d != 808 /* Meltan's walk is set above */) {
              char b[96]; snprintf(b, sizeof(b), "%s (ID %d): farewell held at L100", DEX_TBL[d].name, v);
              held.push_back(b);
            }
          }
        }
      }
      // by each stone from the bag
      for (uint8_t s = 0; s < EVO_STONE_COUNT; s++) {
        Pet p;
        raise(p, d, v, 50);
        if (!p.stoneTarget(s)) continue;
        int16_t before = p.speciesId;
        if (p.useStone(s) && p.speciesId != before) { got[d].insert(p.speciesId); routes++; }
      }
      // by trade (a real trade's mark, or the Linking Cord's)
      {
        Pet p;
        raise(p, d, v, 50);
        p.tradeEvo = true;
        int16_t before = p.speciesId;
        if (p.canEvolveNow()) { p.evolve(); if (p.speciesId != before) { got[d].insert(p.speciesId); routes++; } }
      }
    }
  }
  printf("      %d drawable species, %d evolutions performed\n", species, routes);

  // ---- held against the tables ----------------------------------------------------
  std::vector<std::string> broken, refusedArt, extra;
  for (auto &kv : expect) {
    int16_t d = kv.first;
    if (!speciesShowable(d)) continue;
    for (int16_t t : kv.second) {
      if (got[d].count(t)) continue;
      char b[96]; snprintf(b, sizeof(b), "%s -> %s", DEX_TBL[d].name, DEX_TBL[t].name);
      (speciesShowable(t) ? broken : refusedArt).push_back(b);
    }
  }
  for (auto &kv : got)
    for (int16_t t : kv.second)
      if (!expect[kv.first].count(t)) {
        char b[96]; snprintf(b, sizeof(b), "%s -> %s", DEX_TBL[kv.first].name, DEX_TBL[t].name);
        extra.push_back(b);
      }
  for (auto &s : broken) printf("      BROKEN %s\n", s.c_str());
  for (auto &s : extra) printf("      UNLISTED %s\n", s.c_str());
  for (auto &s : doubles) printf("      TWICE %s\n", s.c_str());
  for (auto &s : stuck) printf("      STUCK %s\n", s.c_str());
  for (auto &s : held) printf("      HELD %s\n", s.c_str());
  printf("      %u evolutions refused because the target has no art (the rule):", (unsigned)refusedArt.size());
  for (auto &s : refusedArt) printf(" %s;", s.c_str());
  printf("\n");
  ck(broken.empty(), "every evolution the tables list happens through the firmware");
  ck(extra.empty(), "and nothing evolves into what no table lists");
  ck(doubles.empty(), "no creature is due to evolve again at the level it just evolved at");
  ck(stuck.empty(), "at level 100 nothing still waits on a level evolution");
  ck(held.empty(), "an evolution that needs an item or a trade never holds the farewell");
  ck(shedFrom.count(290) == 1, "Nincada's evolution brings its Shedinja");

  // ---- can every drawable species be obtained? -------------------------------------
  std::set<int16_t> have;
  // eggs: every region's every tier, through the firmware's own roll
  {
    Pet e;
    for (uint8_t r = 0; r < REGION_COUNT; r++)
      for (uint8_t t = R_COMUN; t <= R_LEGENDARIO; t++)
        for (int i = 0; i < 6000; i++) { int16_t x = e.rollInRegion(r, t); if (x > 0) have.insert(x); }
  }
  size_t hatched = have.size();
  for (unsigned i = 0; i < sizeof(WILD_SLOTS) / sizeof(WILD_SLOTS[0]); i++)
    if (WILD_SLOTS[i].dex > 0 && speciesShowable(WILD_SLOTS[i].dex)) have.insert(WILD_SLOTS[i].dex);
  for (uint8_t i = 0; i < wildLegendCount(); i++) if (speciesShowable(wildLegendDex(i))) have.insert(wildLegendDex(i));
  for (uint8_t i = 0; i < WILD_FIXED_COUNT; i++) have.insert(WILD_FIXED[i].dex);
  for (uint8_t i = 0; i < WILD_SWARM_COUNT; i++) have.insert(WILD_SWARMS[i].dex);
  for (uint8_t h = 0; h < 2; h++)
    for (uint8_t i = 0; i < hallPrizeCount(h); i++)
      if (hallPrize(h, i).kind == PRIZE_MON) have.insert((int16_t)hallPrize(h, i).what);
  size_t met = have.size();
  // A trade with a named partner (Karrablast for a Shelmet) needs that partner
  // owned too -- the Linking Cord asks for it in the party.
  auto partnerOf = [](int16_t d, int16_t t) -> int16_t {
    for (int i = 0; i < TRADE_EVO_COUNT; i++)
      if (TRADE_EVOS[i].from == d && TRADE_EVOS[i].to == t) return TRADE_EVOS[i].partner;
    return 0;
  };
  for (bool grew = true; grew;) {
    grew = false;
    std::set<int16_t> add;
    for (int16_t d : have) {
      for (int16_t t : got[d]) {
        int16_t mate = partnerOf(d, t);
        if (!have.count(t) && (!mate || have.count(mate))) add.insert(t);
      }
      if (shedFrom.count(d) && !have.count(292)) add.insert(292);
    }
    if (!add.empty()) { have.insert(add.begin(), add.end()); grew = true; }
  }
  // Known, recorded in CLAUDE.md (v3.55): each needs an art-less creature first.
  const int16_t KNOWN_NO_ROUTE[] = { 589, 617, 733 };   // Escavalier, Accelgor, Toucannon
  std::vector<int16_t> none;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (!speciesShowable(d) || have.count(d)) continue;
    bool known = false;
    for (int16_t k : KNOWN_NO_ROUTE) if (k == d) known = true;
    if (!known) none.push_back(d);
    printf("      NO ROUTE %s%s\n", DEX_TBL[d].name, known ? " (known)" : "");
  }
  printf("      obtainable: %u of %d drawable (%u hatch, %u more met in Explore or won, the rest by evolving)\n",
         (unsigned)have.size(), species, (unsigned)hatched, (unsigned)(met - hatched));
  ck(none.empty(), "every drawable species can be obtained, but the known three");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
