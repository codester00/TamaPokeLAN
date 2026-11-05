// The Battle Zone's economy, measured (v4.09 tuning pass). Not a test -- a
// report generator, built like league_sim.cpp with tests/run.sh's CORE sources:
//
//   g++ -std=c++17 -O2 -w -Itools/emu -I. tools/emu/zone_sim.cpp <CORE> -o /tmp/zone_sim
//   /tmp/zone_sim            # every section; ZS_N=<runs> for more samples
//
// It plays the firmware's own rules with no screen:
//   GAUNTLET   gauntletChallenger() / gauntletBp() / the boss rounds, the run's
//              team at full HP every fight with its PP CARRIED (the zone's PP
//              ledger), until a loss; challengers' moves from relearnFromLevel()
//              as zoneFoeMon() builds them, their AI and switching by round.
//   TOURNAMENT tourDraw() / tourFoe(), the player's level capped at each
//              opponent's best, the other matches fought by battleSimulate()
//              with their PP carried, the champion's bonus.
//   HALL       bhallFoe() / bhallPrize(), the live pet alone, its own PP.
// The player's side uses the smart AI (a player who picks sensibly) and its
// next creature by matchup, and never switches by choice.
//
// Player profiles (owner's, 2026-10-31, with the PP Up variants for v4.09):
//   CASUAL     random final forms, base stats 470-540, IV 16, training 50,
//              the four strongest level-up attacks only
//   GOOD       final forms picked by 1v1 wins (base stats >= 500), IV 31,
//              training 100, the full move list (TMs owned)
//   +PPUP      the same with three PP Ups on every move slot
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include "gauntlet.h"
#include "tournament.h"
#include "hall.h"
#include "zone.h"
#include "pp.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstring>
uint32_t g_seed = 909; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int N = 200;
enum Prof { CASUAL, GOOD, MID, PROF_COUNT };
static const char *PROF_NAME[] = { "CASUAL", "GOOD", "MID" };

static int bstOf(int16_t d) { const DexEntry &e = DEX_TBL[d]; return e.bHp + e.bAtk + e.bDef + e.bSpe + e.bSpA + e.bSpD; }

static void mk(Combatant &c, int16_t dex, uint8_t lvl, uint8_t iv, uint8_t tr, bool levelUpOnly, uint8_t ups) {
  Pet p; p.dbgHatchAs(dex, false);
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = iv;
  if (tr > Pet::trMaxFor(iv)) tr = Pet::trMaxFor(iv);
  p.trAtk = p.trDef = p.trSpe = tr;
  p.ageMinutes = (uint32_t)(lvl ? lvl - 1 : 0) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  if (levelUpOnly) {
    std::vector<uint8_t> c4;
    for (uint8_t i = 0; i < learnCount(dex); i++) {
      uint8_t at = learnLevel(dex, i), m = learnMove(dex, i);
      if (at == 0 || at > lvl || !m || m >= MOVE_COUNT || MOVE_TBL[m].cat == MC_STATUS) continue;
      if (std::find(c4.begin(), c4.end(), m) == c4.end()) c4.push_back(m);
    }
    std::sort(c4.begin(), c4.end(), [](uint8_t a, uint8_t b) { return MOVE_TBL[a].power > MOVE_TBL[b].power; });
    for (int s = 0; s < MOVE_SLOTS; s++) p.moves[s] = s < (int)c4.size() ? c4[s] : 0;
    if (!p.moves[0]) p.relearnFromLevel();
  }
  p.ppUps = ups;
  combatantFromPet(c, p);
  c.ppOn = true;
}

// The next standing creature by matchup (the firmware's simNext / btlZoneNext rule).
static int8_t nextBy(Combatant *t, uint8_t n, const Combatant &foe) {
  int8_t best = -1; int32_t bs = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (t[i].fainted()) continue;
    int32_t s = battleMatchScore(t[i].dex, foe.dex);
    if (best < 0 || s > bs) { best = (int8_t)i; bs = s; }
  }
  return best;
}
// One fight: our side smart, theirs as the format says. true = we win.
static bool fight(Combatant *a, uint8_t na, Combatant *b, uint8_t nb, bool smartB, bool swB) {
  int8_t ia = 0, ib = 0;
  uint8_t coolB = 0;
  TurnLog lg;
  for (int turn = 0; turn < BATTLE_SIM_TURNS; turn++) {
    int8_t tb = -1;
    if (swB && coolB >= AI_SWITCH_COOLDOWN && !b[ib].charging && !b[ib].recharge)
      tb = aiSwitchTarget(b, nb, (uint8_t)ib, a[ia]);
    if (tb >= 0) { b[ib] = battleStored(b[ib]); ib = tb; coolB = 0; }
    uint8_t ma = aiChooseMove(a[ia], b[ib], true);
    uint8_t mb = tb >= 0 ? 0 : aiChooseMove(b[ib], a[ia], smartB);
    Combatant &ca = a[ia], &cb = b[ib];
    bool aFirst = battleMovesFirst(ca, ma, cb, mb);
    Combatant &x = aFirst ? ca : cb, &y = aFirst ? cb : ca;
    battleAct(x, y, aFirst ? ma : mb, lg);
    if (!y.fainted() && !x.fainted()) battleAct(y, x, aFirst ? mb : ma, lg);
    if (!ca.fainted() && !cb.fainted()) { battleEndTurn(ca, lg); battleEndTurn(cb, lg); }
    if (coolB < 255) coolB++;
    if (ca.fainted()) { a[ia] = battleStored(ca); ia = nextBy(a, na, b[ib]); if (ia < 0) return false; }
    if (b[ib].fainted()) { b[ib] = battleStored(b[ib]); ib = nextBy(b, nb, a[ia]); if (ib < 0) return true; coolB = 0; }
  }
  return false;   // a stall counts as a loss for the player (the run ends)
}

// ---- teams ---------------------------------------------------------------
static std::vector<int16_t> casualPool, goodPool, midPool;
static void pools() {
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    const DexEntry &e = DEX_TBL[d];
    if (speciesEvolvesFurther(d) || e.rarity == R_LEGENDARIO || !speciesShowable(d)) continue;
    int b = bstOf(d);
    if (b >= 470 && b <= 540) casualPool.push_back(d);
    if (b >= 500) goodPool.push_back(d);
    if (b >= 500 && b <= 600) midPool.push_back(d);
  }
}
// GOOD: rank the pool once by 1v1 wins against a sample of the pool, take the
// strongest -- a player who has worked out what is good, not one who counters
// each challenger.
static std::vector<int16_t> goodRank;
static void rankGood(uint8_t lvl) {
  std::vector<std::pair<int, int16_t>> sc;
  for (int16_t d : goodPool) {
    int w = 0;
    for (int k = 0; k < 40; k++) {
      int16_t o = goodPool[(k * 37 + d) % goodPool.size()];
      Combatant a, b; mk(a, d, lvl, 31, 100, false, 0); mk(b, o, lvl, 31, 100, false, 0);
      if (fight(&a, 1, &b, 1, true, false)) w++;
    }
    sc.push_back({ w, d });
  }
  std::sort(sc.begin(), sc.end(), [](const std::pair<int, int16_t> &x, const std::pair<int, int16_t> &y) { return x.first > y.first; });
  for (auto &p : sc) goodRank.push_back(p.second);
}
static void team(Prof p, uint8_t size, uint8_t lvl, uint8_t ups, uint32_t &s, Combatant *out) {
  std::vector<int16_t> picked;
  while (picked.size() < size) {
    s = s * 1103515245u + 12345u;
    int16_t d = p == CASUAL ? casualPool[(s >> 8) % casualPool.size()]
              : p == MID ? midPool[(s >> 8) % midPool.size()]
                            : goodRank[(s >> 8) % 12];   // one of the 12 strongest
    if (std::find(picked.begin(), picked.end(), d) == picked.end()) picked.push_back(d);
  }
  for (uint8_t i = 0; i < size; i++)
    mk(out[i], picked[i], lvl, p == CASUAL ? 16 : p == MID ? 20 : 31, p == CASUAL ? 50 : p == MID ? 75 : 100, p == CASUAL, ups);
}

// ---- the gauntlet --------------------------------------------------------
struct GRun { int wins; int bp; int battles; int ppLeftPct; };
static GRun gauntletRun(Prof p, uint8_t size, uint8_t lvl, uint8_t ups, bool doubled, uint32_t seed) {
  GauntletRules r; r.size = size;   // any type, no cap, all forms: the owner's playtest
  Combatant us[6];
  uint32_t s = seed;
  team(p, size, lvl, ups, s, us);
  GRun g = { 0, 0, 0, 0 };
  for (uint16_t round = 0; round < 200; round++) {
    ZoneFoe f;
    if (!gauntletChallenger(r, round, seed, lvl, 19, f)) break;
    Combatant them[6];
    for (uint8_t k = 0; k < f.count; k++) mk(them[k], f.dex[k], f.level[k], f.iv, f.tr, false, 0);
    Combatant fresh[6];
    for (uint8_t i = 0; i < size; i++) {           // full HP, the PP the run has left
      uint8_t pp[MOVE_SLOTS]; memcpy(pp, us[i].pp, sizeof(pp));
      fresh[i] = battleStored(us[i]);
      fresh[i].hp = fresh[i].maxHp; fresh[i].ailment = AIL_NONE;
      memcpy(fresh[i].pp, pp, sizeof(pp));
    }
    g.battles++;
    bool won = fight(fresh, size, them, f.count, f.smart, f.switching);
    for (uint8_t i = 0; i < size; i++) memcpy(us[i].pp, fresh[i].pp, sizeof(us[i].pp));
    if (!won) {   // how much PP the team had left when it lost (attacking moves)
      int left = 0, mx = 0;
      for (uint8_t i = 0; i < size; i++)
        for (int m = 0; m < MOVE_SLOTS; m++) {
          uint8_t mv = us[i].moves[m];
          if (!mv || MOVE_TBL[mv].cat == MC_STATUS) continue;
          left += us[i].pp[m]; mx += ppMaxFor(mv, ppUpsAt(us[i].ppUps, m));
        }
      g.ppLeftPct = mx ? left * 100 / mx : 0;
      break;
    }
    g.bp += gauntletBp(round, doubled);
    g.wins++;
  }
  return g;
}

// ---- the tournament ------------------------------------------------------
// ZS_TOURFILL (the owner's idea, 2026-11-05, measured before building): every
// ladder seat brings six -- its own roster, each evolved to its final form
// (ZS_TOUREVO), then the strongest final forms of the seat's specialty type --
// and with ZS_TOURLVL=n everyone fights at level n.
static int16_t finalOf(int16_t d) {
  for (int i = 0; i < 4 && DEX_TBL[d].evolvesTo; i++) d = (int16_t)DEX_TBL[d].evolvesTo;
  return d;
}
static void tourBoost(uint16_t id, ZoneFoe &f) {
  if (!getenv("ZS_TOURFILL") || (id & TOUR_GEN)) return;
  uint8_t region = (uint8_t)(id / TRAINER_COUNT), seat = (uint8_t)(id % TRAINER_COUNT);
  const Trainer &t = TRAINER_SETS[region].list[seat];
  uint8_t best = 1; for (uint8_t k = 0; k < f.count; k++) if (f.level[k] > best) best = f.level[k];
  if (getenv("ZS_TOURLVL")) best = (uint8_t)atoi(getenv("ZS_TOURLVL"));
  if (getenv("ZS_TOUREVO")) for (uint8_t k = 0; k < f.count; k++) f.dex[k] = finalOf(f.dex[k]);
  std::vector<std::pair<int, int16_t>> c;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    const DexEntry &e = DEX_TBL[d];
    if ((e.type1 != t.type && e.type2 != t.type) || speciesEvolvesFurther(d) || e.rarity == R_LEGENDARIO || !speciesShowable(d)) continue;
    bool have = false; for (uint8_t k = 0; k < f.count; k++) if (f.dex[k] == d) have = true;
    if (!have) c.push_back({ bstOf(d), d });
  }
  std::sort(c.begin(), c.end(), [](const std::pair<int, int16_t> &x, const std::pair<int, int16_t> &y) { return x.first > y.first; });
  // a varied pick among the type's 12 strongest, by the seat
  uint32_t s = id * 2654435761u + 7;
  while (f.count < 6 && !c.empty()) {
    s = s * 1103515245u + 12345u;
    size_t k = (s >> 8) % (getenv("ZS_TOURFILLANY") ? c.size() : std::min<size_t>(12, c.size()));
    f.dex[f.count++] = c[k].second; c.erase(c.begin() + k);
  }
  for (uint8_t k = 0; k < f.count; k++) f.level[k] = best;
}

static int tourRun(Prof p, uint8_t size, uint8_t ups, uint32_t seed, int *bp) {
  uint16_t br[TOUR_SIZE];
  tourDraw(seed, br);
  uint16_t alive = 0xFFFF;
  static Combatant opp[TOUR_SIZE][6]; static uint8_t oppN[TOUR_SIZE]; static bool built[TOUR_SIZE];
  memset(built, 0, sizeof(built));
  Combatant us[6]; uint32_t s = seed ^ 0xABCD; team(p, size, 100, ups, s, us);
  uint8_t usedPp[6][MOVE_SLOTS]; for (int i = 0; i < size; i++) memcpy(usedPp[i], us[i].pp, sizeof(usedPp[i]));
  *bp = 0;
  for (uint8_t round = 0; round < TOUR_ROUNDS; round++) {
    uint8_t idx[TOUR_SIZE], n = 0;
    for (uint8_t i = 0; i < TOUR_SIZE; i++) if (alive & (1u << i)) idx[n++] = i;
    for (uint8_t q = 0; q + 1 < n; q += 2) {
      uint8_t a = idx[q], b = idx[q + 1];
      for (uint8_t c : { a, b }) {
        if (br[c] == TOUR_PLAYER || built[c]) continue;
        ZoneFoe f; tourFoe(br[c], 0, seed, 19, f);
        oppN[c] = f.count;
        for (uint8_t k = 0; k < f.count; k++) mk(opp[c][k], f.dex[k], f.level[k], f.iv, f.tr, false, 0);
        built[c] = true;
      }
      auto freshOf = [](Combatant *t, uint8_t n2, Combatant *o) {
        for (uint8_t i = 0; i < n2; i++) { o[i] = battleStored(t[i]); o[i].hp = o[i].maxHp; o[i].ailment = AIL_NONE; memcpy(o[i].pp, t[i].pp, sizeof(o[i].pp)); }
      };
      bool sw = tourSwitches(round);
      if (br[a] == TOUR_PLAYER || br[b] == TOUR_PLAYER) {
        uint8_t o = br[a] == TOUR_PLAYER ? b : a;
        ZoneFoe f; tourFoe(br[o], round, seed, 19, f);
        uint8_t cap = 1; for (uint8_t k = 0; k < f.count; k++) if (f.level[k] > cap) cap = f.level[k];
        Combatant mine[6], theirs[6];
        uint32_t s2 = seed ^ 0xABCD; team(p, size, cap, ups, s2, mine);    // the same six, at the cap
        for (uint8_t i = 0; i < size; i++) memcpy(mine[i].pp, usedPp[i], sizeof(mine[i].pp));
        freshOf(opp[o], oppN[o], theirs);
        uint8_t fieldN = getenv("ZS_TOURCAP") && oppN[o] < size ? oppN[o] : size;   // the hard ladder's team cap
        uint8_t theirN = oppN[o] > size ? size : oppN[o];  // they bring as many as you (tourNextFight(), v4.10)
        bool won = fight(mine, fieldN, theirs, theirN, true, sw);
        for (uint8_t i = 0; i < fieldN; i++) memcpy(usedPp[i], mine[i].pp, sizeof(usedPp[i]));
        if (!won) return round;
        *bp += ZONE_BP_WIN[ZONE_TOURNAMENT];
        alive &= (uint16_t)~(1u << o);
        for (uint8_t k = 0; k < oppN[o]; k++) memcpy(opp[o][k].pp, theirs[k].pp, sizeof(theirs[k].pp));
      } else {
        Combatant ta[6], tb[6];
        freshOf(opp[a], oppN[a], ta); freshOf(opp[b], oppN[b], tb);
        uint8_t res = battleSimulate(ta, oppN[a], tb, oppN[b], true, sw, sw);
        uint8_t w = res == 0 ? a : b, l = res == 0 ? b : a;
        Combatant *wt = res == 0 ? ta : tb;
        for (uint8_t k = 0; k < oppN[w]; k++) memcpy(opp[w][k].pp, wt[k].pp, sizeof(wt[k].pp));
        alive &= (uint16_t)~(1u << l);
      }
    }
  }
  *bp += ZONE_TOUR_CHAMP_BP;
  return TOUR_ROUNDS;
}

// ---- the Hall ------------------------------------------------------------
// Hall experiment knobs (ZS_ONLY=hallx): the pet's build, and the foe's
// training ceiling (gHallTrMax; 100 = the firmware) or a foe that ramps to the
// pet's own IV and training (gHallMirror).
static uint8_t gPetIv = 31, gPetTr = 100; static bool gPetLvlOnly = false;
static int gHallTrMax = 100; static bool gHallMirror = false; static int gHallRefill = 6;
static int gLvlPct0 = 0, gLvlPct1 = 0;   // when set: the foe's level = pet level x pct, ramping rung 0 -> 17
static int hallRun(int16_t dex, uint8_t lvl, uint32_t seed, uint32_t *money, int *battles) {
  Combatant me; mk(me, dex, lvl, gPetIv, gPetTr, gPetLvlOnly, 0);
  *money = 0; *battles = 0;
  for (uint8_t rung = 0; rung < HALL_RUNGS; rung++) {
    HallFoe hf;
    if (!bhallFoe(seed, rung, dex, lvl, hf)) continue;
    uint8_t fiv = hf.iv, ftr = hf.tr;
    if (gHallMirror) { fiv = (uint8_t)(8 + rung * (gPetIv - 8) / (HALL_RUNGS - 1)); ftr = (uint8_t)(rung * gPetTr / (HALL_RUNGS - 1)); }
    else if (gHallTrMax != 100) ftr = (uint8_t)(rung * gHallTrMax / (HALL_RUNGS - 1));
    uint8_t flv = hf.level;
    if (gLvlPct1) { int pc = gLvlPct0 + (gLvlPct1 - gLvlPct0) * rung / (HALL_RUNGS - 1); flv = (uint8_t)std::max(1, lvl * pc / 100); }
    Combatant f; mk(f, hf.dex, flv, fiv, ftr, false, 0);
    Combatant mine = battleStored(me); mine.hp = mine.maxHp; mine.ailment = AIL_NONE;
    (*battles)++;
    bool won = fight(&mine, 1, &f, 1, hf.smart, false);
    if (!won) return rung;
    *money += bhallPrize(rung, hf.level);
    me = mine;                                 // its PP is its own, carried
    // a nap refills it between sittings: the sim assumes one sitting per 6 rungs
    if (rung % gHallRefill == gHallRefill - 1) battleFullPp(me);
  }
  return HALL_RUNGS;
}

static void pct(const std::vector<int> &v, int *p10, int *p50, int *p90, double *mean) {
  std::vector<int> s = v; std::sort(s.begin(), s.end());
  *p10 = s[s.size() / 10]; *p50 = s[s.size() / 2]; *p90 = s[s.size() * 9 / 10];
  double t = 0; for (int x : s) t += x; *mean = t / s.size();
}

int main() {
  if (getenv("ZS_N")) N = atoi(getenv("ZS_N"));
  const char *only = getenv("ZS_ONLY");
  pools();
  rankGood(100);
  printf("pools: casual %zu, good %zu; good top-12:", casualPool.size(), goodPool.size());
  for (int i = 0; i < 12; i++) printf(" %s", DEX_TBL[goodRank[i]].name);
  printf("\n\n");

  if (!only || strstr(only, "gaunt")) {
    printf("GAUNTLET (any type, no cap, all forms), level 100, %d runs each\n", N);
    printf("%-8s %-4s %-5s | wins p10 p50 p90 mean | BP/run mean (daily 2x) | BP/battle\n", "profile", "team", "PPup");
    for (uint8_t size : { 3, 6 })
      for (int p = 0; p < PROF_COUNT; p++)
        for (uint8_t ups : { 0, 0xFF }) {
          std::vector<int> w, b, b2; int bat = 0, ppl = 0, dry = 0;
          for (int i = 0; i < N; i++) {
            GRun g = gauntletRun((Prof)p, size, 100, ups, false, 1000 + i * 7919);
            w.push_back(g.wins); b.push_back(g.bp); bat += g.battles; ppl += g.ppLeftPct; if (g.ppLeftPct < 15) dry++;
            b2.push_back(g.bp * 2);
          }
          int a1, a2, a3; double m, mb, mb2; pct(w, &a1, &a2, &a3, &m);
          int x, y, z; pct(b, &x, &y, &z, &mb); pct(b2, &x, &y, &z, &mb2);
          double tot = 0; for (int v : b) tot += v;
          printf("%-8s %-4u %-5s | %4d %4d %4d %5.1f | %6.1f (%6.1f)        | %.2f | PP left at the loss %2d%%, runs ended nearly dry %2d%%\n", PROF_NAME[p], size,
                 ups ? "x3" : "-", a1, a2, a3, m, mb, mb2, tot / bat, ppl / N, dry * 100 / N);
          fflush(stdout);
        }
    printf("\nBP for a run of exactly W wins (no 2x): ");
    for (int W : { 5, 10, 15, 20, 25, 30, 40, 50 }) {
      int t = 0; for (int r = 0; r < W; r++) t += gauntletBp((uint16_t)r, false);
      printf("W%d=%d  ", W, t);
    }
    printf("\n\n");
  }

  if (!only || strstr(only, "tour")) {
    printf("TOURNAMENT, %d runs each (rounds reached 0-4, 4 = champion)\n", N);
    for (int p = 0; p < PROF_COUNT; p++)
      for (uint8_t size : { 3, 6 })
        for (uint8_t ups : { 0, 0xFF }) {
          int champs = 0, bpT = 0, roundsT = 0;
          for (int i = 0; i < N; i++) {
            int bp; int r = tourRun((Prof)p, size, ups, 5000 + i * 104729, &bp);
            if (r == TOUR_ROUNDS) champs++;
            bpT += r == TOUR_ROUNDS ? bp : r * ZONE_BP_WIN[ZONE_TOURNAMENT];
            roundsT += r < TOUR_ROUNDS ? r + 1 : TOUR_ROUNDS;
          }
          printf("%-8s team %u PPup %-3s | champion %3d%% | BP/entry %5.1f | battles/entry %.2f\n", PROF_NAME[p], size,
                 ups ? "x3" : "-", champs * 100 / N, bpT / (double)N, roundsT / (double)N);
          fflush(stdout);
        }
    printf("\n");
  }

  if (!only || strstr(only, "money")) {
    printf("REMATCH MONEY (trainerPrize(), a badge already held), per region\n");
    printf("%-7s | 8 gyms easy  hard | League run easy  hard | best single hard\n", "region");
    const char *RN[] = { "Kanto", "Johto", "Hoenn", "Sinnoh", "Unova", "Kalos", "Alola", "Galar", "Paldea" };
    for (uint8_t r = 0; r < GYM_REGIONS; r++) {
      uint32_t ge = 0, gh = 0, le = 0, lh = 0, best = 0;
      int lf = TRAINER_COUNT - TRAINER_ELITE4 - 1;
      for (int i = 0; i < TRAINER_COUNT; i++) {
        uint32_t e = trainerPrize(r, (uint8_t)i, false, false), h = trainerPrize(r, (uint8_t)i, true, false);
        if (i < lf) { ge += e; gh += h; } else { le += e; lh += h; }
        if (h > best) best = h;
      }
      printf("%-7s | %10lu %6lu | %15lu %6lu | %lu\n", RN[r], (unsigned long)ge, (unsigned long)gh,
             (unsigned long)le, (unsigned long)lh, (unsigned long)best);
    }
    printf("\n");
  }

  if (only && strstr(only, "hallx")) {
    // v4.10: a perfectly trained pet (training 100, TMs) of any final form of
    // base stats 470+, legendaries included, at three IV levels; the firmware's
    // foe ramps; one PP refill (a night's sleep) halfway.
    std::vector<int16_t> hp;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (!speciesEvolvesFurther(d) && speciesShowable(d) && bstOf(d) >= 470) hp.push_back(d);
    gHallRefill = getenv("ZS_HALLREFILL") ? atoi(getenv("ZS_HALLREFILL")) : 9;
    printf("HALL (foe IV %d->%d, training %d->%d, %d under to rung %d, smart from %d, PP refill every %d), %zu pets, %d runs: clear %%\n",
           HALL_IV_LO, HALL_IV_HI, HALL_TR_LO, HALL_TR_HI, HALL_UNDER, HALL_LEVEL_AT, HALL_SMART_FROM, gHallRefill, hp.size(), N);
    for (uint8_t iv : { 10, 20, 31 }) {
      printf("  pet IV %2u, training 100:", iv);
      for (uint8_t lvl : { 50, 75, 100 }) {
        gPetIv = iv; gPetTr = 100; gPetLvlOnly = false;
        int clears = 0; long rungs = 0;
        for (int i = 0; i < N; i++) {
          int16_t d = hp[(i * 7919u) % hp.size()];
          uint32_t m; int b; int r = hallRun(d, lvl, 77 + i * 131, &m, &b);
          rungs += r; if (r == HALL_RUNGS) clears++;
        }
        printf("  L%-3u %3d%% (avg %4.1f rungs)", lvl, clears * 100 / N, rungs / (double)N);
        fflush(stdout);
      }
      printf("\n");
    }
    gPetIv = 31; gPetTr = 100; gHallRefill = 6;
    return 0;
  }
  if (!only || strstr(only, "hall")) {
    printf("BATTLE HALL, fully trained pet, %d runs each\n", N);
    for (uint8_t lvl : { 50, 100 }) {
      int clears = 0; double money = 0, bat = 0;
      for (int i = 0; i < N; i++) {
        int16_t d = goodRank[i % 12];
        uint32_t m; int b;
        int r = hallRun(d, lvl, 77 + i * 131, &m, &b);
        if (r == HALL_RUNGS) clears++;
        money += m; bat += b;
      }
      uint32_t full = 0; for (uint8_t r = 0; r < HALL_RUNGS; r++) full += bhallPrize(r, (uint8_t)(lvl));
      printf("level %3u | clears %3d%% | $/run mean %7.0f | battles/run %.1f | a full clear pays $%lu | energy for a clear %u\n",
             lvl, clears * 100 / N, money / N, bat / N, (unsigned long)full, (unsigned)(HALL_RUNGS * HALL_ENERGY_COST));
    }
  }
  return 0;
}
