// The player's guide's data, as JSON, read straight from the firmware's own
// tables and rules. A report, not a test. tools/gen_guide.py builds it and
// writes docs/players-guide/ from it, so the guide cannot drift from the game
// (CLAUDE.md: derive, never restate).
//
//   python3 tools/gen_guide.py          # builds this, runs it, writes the guide
//
// What it asks the firmware rather than re-deriving: a leader's moves come from
// Pet::relearnFromLevel() at the leader's level (foeFromSpecies()'s own route),
// prize money from trainerPrize(), a TM's price from tmPrice(), a hall's prizes
// from hallPrize(), which species can hatch from what eggCanHold() asks
// (pack, art, not an Explore legendary).
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "wild.h"
#include "wild_areas.h"
#include "trainers.h"
#include "items.h"
#include "tms.h"
#include "zone.h"
#include "gamecorner.h"
#include "types.h"
#include "noart.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0x6D1E; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static bool first;
static void sep() { if (!first) printf(","); first = false; }
static void str(const char *s) {
  putchar('"');
  for (; s && *s; s++) { if (*s == '"' || *s == '\\') putchar('\\'); putchar(*s); }
  putchar('"');
}


// ---- the Safari: each strategy's catch odds, played by the firmware's own
// rules (safariFlees() as the turn starts, the action, then safariWatch()), the
// order btlSafariAct() plays them in.
static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n) { rng = rng * 1103515245u + 12345u; return (rng >> 8) % n; }
// a strategy: the first few actions, then what to do once they are used up
struct Strat { const char *name; uint8_t opening[4]; uint8_t nOpen; uint8_t repeat; };
// action 0 ball, 1 bait/pokeblock, 2 rock/mud/go near; repeat: 0 none,
// 1 re-throw action 1 when it wears off, 2 re-throw action 2 when it wears off
static const Strat STRATS[3][5] = {
  { { "Balls only", {0}, 0, 0 }, { "Rock, then balls; rock again when it calms down", {2}, 1, 2 },
    { "Bait, then balls; bait again when it stops eating", {1}, 1, 1 }, { "One rock, then balls", {2}, 1, 0 },
    { nullptr, {0}, 0, 0 } },
  { { "Balls only", {0}, 0, 0 }, { "Go near once, then balls", {2}, 1, 0 },
    { "Go near three times, then balls", {2, 2, 2}, 3, 0 }, { "One Pokeblock, then balls", {1}, 1, 0 },
    { "Three Pokeblocks, then balls", {1, 1, 1}, 3, 0 } },
  { { "Balls only", {0}, 0, 0 }, { "Bait once, then balls", {1}, 1, 0 },
    { "Bait twice, then balls", {1, 1}, 2, 0 }, { "Mud once, then balls", {2}, 1, 0 },
    { nullptr, {0}, 0, 0 } },
};
static double safariOdds(int16_t dex, uint8_t rules, const Strat &st) {
  const int RUNS = 20000;
  int caught = 0;
  for (int run = 0; run < RUNS; run++) {
    SafariState s;
    safariBegin(s, dex, rules);
    uint8_t k = 0;
    for (;;) {
      bool flee = safariFlees(s, (uint16_t)rnd(65536));
      uint8_t a = 0;
      if (k < st.nOpen) a = st.opening[k++];
      else if (st.repeat == 1 && rules == SAFARI_FRLG && !s.baitTurns) a = 1;
      else if (st.repeat == 2 && rules == SAFARI_FRLG && !s.rockTurns) a = 2;
      if (a == 0) {
        if (!s.balls) break;
        s.balls--;
        uint16_t r[4];
        for (uint16_t &x : r) x = (uint16_t)rnd(65536);
        if (safariBallShakes(s, r) >= 4) { caught++; break; }
        if (!s.balls) break;
      } else if (a == 1) safariBait(s, (uint8_t)rnd(rules == SAFARI_PLATINUM ? 10 : 5));
      else safariRock(s, (uint8_t)rnd(rules == SAFARI_PLATINUM ? 10 : 5));
      if (flee) break;
      safariWatch(s);
    }
  }
  return 100.0 * caught / RUNS;
}

int main() {
  gRegionArt = 0xFFFF;
  printf("{\n");

  // ---- species ----
  printf("\"dex\":[null");
  for (int d = 1; d <= DEX_COUNT; d++) {
    const DexEntry &e = DEX_TBL[d];
    printf(",\n{\"n\":"); str(e.name);
    printf(",\"t\":[%u,%u],\"to\":%u,\"lv\":%u,\"rar\":%u,\"b\":[%u,%u,%u,%u,%u,%u]",
           e.type1, e.type2, e.evolvesTo, e.evolveLevel, e.rarity,
           e.bHp, e.bAtk, e.bDef, e.bSpA, e.bSpD, e.bSpe);
    printf(",\"art\":%d,\"legend\":%d,\"catch\":%u,\"g\":%d,\"kg\":%u}",
           speciesHasArt(d) ? 1 : 0, wildIsExploreLegend(d) ? 1 : 0,
           DEX_CATCH_RATE[d], DEX_GENDER_RATE[d], DEX_WEIGHT_HG[d]);
  }
  printf("],\n");

  printf("\"types\":[");
  first = true;
  for (int t = 0; t < 18; t++) { sep(); str(typeName(t)); }
  printf("],\n\"chart\":[");
  for (int a = 0; a < 18; a++) {
    printf(a ? ",[" : "[");
    for (int b = 0; b < 18; b++) printf(b ? ",%u" : "%u", TYPE_FX[a][b]);
    printf("]");
  }
  printf("],\n");

  printf("\"regions\":[");
  first = true;
  for (int r = 0; r < REGION_COUNT; r++) {
    sep(); printf("{\"n\":"); str(REGIONS[r].name);
    printf(",\"lo\":%u,\"hi\":%u,\"start\":[", REGIONS[r].lo, REGIONS[r].hi);
    for (int i = 0; i < REGIONS[r].starterCount; i++) printf(i ? ",%d" : "%d", REGIONS[r].starters[i]);
    printf("]}");
  }
  printf("],\n");

  // ---- evolution tables ----
  printf("\"stones\":[");
  first = true;
  for (int k = IT_FIRESTONE; k <= IT_BLACKAUGURITE; k++) { sep(); str(ITEM_TBL[k].name); }
  printf("],\n\"itemEvos\":[");
  first = true;
  for (int i = 0; i < ITEM_EVO_COUNT; i++) {
    sep(); printf("[%d,%d,%u,%u]", ITEM_EVOS[i].from, ITEM_EVOS[i].to, ITEM_EVOS[i].stone, ITEM_EVOS[i].gender);
  }
  printf("],\n\"tradeEvos\":[");
  first = true;
  for (int i = 0; i < TRADE_EVO_COUNT; i++) {
    sep(); printf("[%d,%d,%d]", TRADE_EVOS[i].from, TRADE_EVOS[i].to, TRADE_EVOS[i].partner);
  }
  printf("],\n\"branchEvos\":[");
  first = true;
  for (int i = 0; i < BRANCH_EVO_COUNT; i++) {
    sep(); printf("[%d,%d,%u,%d]", BRANCH_EVOS[i].from, BRANCH_EVOS[i].to, BRANCH_EVOS[i].kind, BRANCH_EVOS[i].arg);
  }
  printf("],\n\"genderEvos\":[");
  first = true;
  for (int i = 0; i < GENDER_EVO_COUNT; i++) {
    sep(); printf("[%d,%d,%u]", GENDER_EVOS[i].from, GENDER_EVOS[i].to, GENDER_EVOS[i].female);
  }
  printf("],\n\"eevee\":[");
  for (int i = 0; i < EEVEE_EVO_COUNT; i++) printf(i ? ",%d" : "%d", EEVEE_EVOS[i]);
  printf("],\n");

  // ---- Explore ----
  printf("\"wx\":[");
  first = true;
  for (int w = 0; w < WILD_WX_COUNT; w++) { sep(); str(wildWeatherName(w)); }
  printf("],\n\"areas\":[");
  for (int a = 0; a < WILD_AREA_COUNT; a++) {
    const WildArea &A = WILD_AREAS[a];
    printf(a ? ",\n{" : "\n{");
    printf("\"n\":"); str(A.name);
    printf(",\"r\":%u,\"gate\":%u,\"lv\":[%u,%u],\"wx\":%u,\"mode\":%u,\"slots\":[",
           A.region, A.gate, A.lvMin, A.lvMax, A.wxMask, A.mode);
    for (int i = 0; i < A.count; i++) {
      const WildSlot &s = WILD_SLOTS[A.first + i];
      printf(i ? ",[%d,%u,%u,%u,%u,%u]" : "[%d,%u,%u,%u,%u,%u]", s.dex, s.weight, s.when, s.floor, s.gate, s.wx);
    }
    printf("],\"stones\":[");
    first = true;
    for (int i = 0; i < WILD_STONE_COUNT; i++)
      if (WILD_STONES[i].area == a) { sep(); printf("%u", WILD_STONES[i].stone); }
    printf("]}");
  }
  printf("],\n\"legends\":[");
  first = true;
  for (int i = 0; i < WILD_LEGEND_COUNT; i++) {
    const WildLegend &L = WILD_LEGENDS[i];
    sep(); printf("{\"d\":%d,\"a\":%u,\"lv\":%u,\"gate\":%u,\"roams\":%d,\"needs\":[",
                  L.dex, L.area, L.level, L.gate, (L.flags & WILD_LEGEND_ROAMS) ? 1 : 0);
    for (int k = 0; k < L.needCount; k++)
      printf(k ? ",%d" : "%d", WILD_LEGENDS[WILD_LEGEND_NEEDS[L.needFirst + k]].dex);
    printf("]}");
  }
  printf("],\n\"fixed\":[");
  first = true;
  for (int i = 0; i < WILD_FIXED_COUNT; i++) {
    const WildFixed &F = WILD_FIXED[i];
    sep(); printf("{\"d\":%d,\"a\":%u,\"lv\":%u,\"gate\":%u,\"shiny\":%d,\"days\":%u}",
                  F.dex, F.area, F.level, F.gate, (F.flags & WILD_FIXED_SHINY) ? 1 : 0, F.days);
  }
  printf("],\n\"swarms\":[");
  first = true;
  for (int i = 0; i < WILD_SWARM_COUNT; i++) {
    const WildSwarm &S = WILD_SWARMS[i];
    sep(); printf("{\"d\":%d,\"a\":%u,\"share\":%u,\"gate\":%u,\"floor\":%u}",
                  S.dex, S.area, S.share, S.gate, S.floor);
  }
  printf("],\n\"safariFlee\":[");
  first = true;
  for (int i = 0; i < WILD_SAFARI_FLEE_COUNT; i++) {
    sep(); printf("[%d,%u,%u]", WILD_SAFARI_FLEE[i].dex, WILD_SAFARI_FLEE[i].rules, WILD_SAFARI_FLEE[i].rate);
  }
  printf("],\n");

  // ---- ladders ----
  printf("\"badges\":[");
  for (int r = 0; r < GYM_REGIONS; r++) {
    printf(r ? ",[" : "[");
    first = true;
    for (int b = 0; b < TRAINER_GYMS; b++) { sep(); str(BADGE_NAMES[r][b]); }
    printf("]");
  }
  printf("],\n\"ladders\":[");
  for (int r = 0; r < GYM_REGIONS; r++) {
    printf(r ? ",\n[" : "\n[");
    for (int s = 0; s < TRAINER_COUNT; s++) {
      const Trainer &t = TRAINER_SETS[r].list[s];
      printf(s ? ",{" : "{");
      printf("\"n\":"); str(t.name); printf(",\"place\":"); str(t.place);
      printf(",\"type\":%u,\"hardTr\":%u,\"prize\":[%u,%u],\"team\":[", t.type, hardTraining(r, s),
             (unsigned)trainerPrize(r, s, false, true), (unsigned)trainerPrize(r, s, true, true));
      for (int k = 0; k < t.count; k++) {
        Pet foe;
        foe.dbgHatchAs(t.team[k].dex, false);
        foe.ageMinutes = (uint32_t)(t.team[k].level ? t.team[k].level - 1 : 0) * MINUTES_PER_LEVEL;
        foe.relearnFromLevel();
        printf(k ? ",{" : "{");
        printf("\"d\":%u,\"lv\":%u,\"m\":[", t.team[k].dex, t.team[k].level);
        bool f = true;
        for (int m = 0; m < MOVE_SLOTS; m++)
          if (foe.moves[m]) { printf(f ? "%u" : ",%u", foe.moves[m]); f = false; }
        printf("]}");
      }
      printf("]}");
    }
    printf("]");
  }
  printf("],\n");

  // ---- moves, items, TMs, prizes ----
  printf("\"moves\":[");
  for (int m = 0; m < MOVE_COUNT; m++) {
    const MoveEntry &M = MOVE_TBL[m];
    printf(m ? ",\n[" : "\n[");
    str(M.name);
    printf(",%u,%u,%u,%u,%u]", M.type, M.cat, M.power, M.acc, M.pp);
  }
  printf("],\n\"tms\":[");
  for (int i = 0; i < TM_COUNT; i++) printf(i ? ",[%u,%u]" : "[%u,%u]", TM_MOVES[i], tmPrice(TM_MOVES[i]));
  printf("],\n\"sigTms\":[");
  for (int i = 0; i < SIG_TM_COUNT; i++) {
    int16_t who = -1;
    for (int d = 1; d <= DEX_COUNT && who < 0; d++)
      for (uint16_t k = LEARN_OFS[d]; k < LEARN_OFS[d + 1]; k++)
        if (LEARN_TBL[k].move == SIG_TM_MOVES[i]) { who = d; break; }
    printf(i ? ",[%u,%u]" : "[%u,%u]", SIG_TM_MOVES[i], zoneSigTmBp(SIG_TM_MOVES[i]));
    (void)who;
  }
  printf("],\n\"sigLearners\":[");
  first = true;
  for (int i = 0; i < SIG_TM_COUNT; i++) {
    sep(); printf("[%u,[", SIG_TM_MOVES[i]);
    bool f = true;
    for (int d = 1; d <= DEX_COUNT; d++)
      for (uint16_t k = LEARN_OFS[d]; k < LEARN_OFS[d + 1]; k++)
        if (LEARN_TBL[k].move == SIG_TM_MOVES[i]) { printf(f ? "%d" : ",%d", d); f = false; break; }
    printf("]]");
  }
  printf("],\n\"items\":[");
  for (int k = 0; k < ITEM_COUNT; k++) {
    const ItemEntry &I = ITEM_TBL[k];
    printf(k ? ",\n[" : "\n[");
    str(I.name);
    printf(",%u,%d,%u,%u,%u,%u]", I.category, I.param, I.amount, I.dropWeight, I.price,
           itemSellPrice((ItemKey)k));
  }
  printf("],\n\"zonePrizes\":[");
  for (int i = 0; i < ZONE_PRIZE_COUNT; i++) printf(i ? ",[%u,%u]" : "[%u,%u]", ZONE_PRIZES[i].item, ZONE_PRIZES[i].bp);
  printf("],\n\"safari\":[");
  first = true;
  for (int a = 0; a < WILD_AREA_COUNT; a++) {
    if (WILD_AREAS[a].mode != WILD_MODE_SAFARI) continue;
    uint8_t rules = wildSafariRules(a);
    int16_t done[64]; int nd = 0;
    for (int i = 0; i < WILD_AREAS[a].count; i++) {
      int16_t dex = WILD_SLOTS[WILD_AREAS[a].first + i].dex;
      bool dup = false;
      for (int j = 0; j < nd; j++) if (done[j] == dex) dup = true;
      if (dup || nd >= 64) continue;
      done[nd++] = dex;
      sep(); printf("{\"a\":%d,\"d\":%d,\"rules\":%u,\"odds\":[", a, dex, rules);
      bool f = true;
      for (int k = 0; k < 5; k++) {
        if (!STRATS[rules][k].name) continue;
        printf(f ? "[" : ",["); f = false;
        str(STRATS[rules][k].name);
        printf(",%.1f]", safariOdds(dex, rules, STRATS[rules][k]));
      }
      printf("]}");
    }
  }
  printf("],\n\"halls\":[");
  for (int h = 0; h < HALL_COUNT; h++) {
    printf(h ? ",{" : "{");
    printf("\"n\":"); str(hallName(h));
    printf(",\"r\":%u,\"gate\":%u,\"cards\":%d,\"prizes\":[", hallRegion(h), hallGate(h), hallHasCards(h) ? 1 : 0);
    for (int i = 0; i < hallPrizeCount(h); i++) {
      const Prize &p = hallPrize(h, i);
      printf(i ? ",[%u,%u,%u,%u]" : "[%u,%u,%u,%u]", p.kind, p.what, p.level, p.price);
    }
    printf("]}");
  }
  printf("]\n}\n");
  return 0;
}
