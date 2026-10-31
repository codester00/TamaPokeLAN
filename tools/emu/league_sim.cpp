// Hard League runs (v3.94 rules), every region: can a well-built team clear the
// Elite Four and the champion in one run? Not a test -- a measurement, built
// like ladder_sim.cpp (tests/run.sh CORE sources):
//
//   g++ -std=c++17 -O2 -w -Itools/emu -I. tools/emu/league_sim.cpp <CORE> -o /tmp/league_sim
//
// The run is the firmware's: the five fights in order, the same team, HP back
// every fight, PP NOT back (carried per creature), each side's next standing
// creature after a faint, no items, no switching. Hard leaders are IV 31 and
// fully trained (foeFromSpecies()'s rule), moves from relearnFromLevel() (the
// rosters' rule), the smart AI. Our creatures are capped to each leader's best
// level, as gymLevelCap() does, and the team to its count, as teamCapFor().
//
// Team builds compared:
//   BEST   six picked by 1v1 wins against this League's creatures, IV 31,
//          training 100, the full move list (level-up + TMs)
//   NO-TM  the same six, but only their four strongest level-up attacks
//   HALF   BEST at IV 16 and training 50
//   RAW    BEST at IV 16, untrained
//   RANDOM six random final forms, IV 16, training 50, full move list
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstdlib>
uint32_t g_seed = 4242; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static const int LF = TRAINER_COUNT - TRAINER_ELITE4 - 1;
enum Moves { M_FULL, M_LEVELUP };
static void mk(Combatant &c, int16_t dex, uint8_t lvl, uint8_t iv, uint8_t tr, Moves mv) {
  Pet p; p.dbgHatchAs(dex, false);
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = iv;
  p.trAtk = p.trDef = p.trSpe = tr;
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  if (mv == M_LEVELUP) {             // the four strongest level-up attacks, as a player who never buys a TM
    std::vector<uint8_t> c4;
    for (uint8_t i = 0; i < learnCount(dex); i++) {
      uint8_t at = learnLevel(dex, i), m = learnMove(dex, i);
      if (at == 0 || at > lvl || !m || m >= MOVE_COUNT) continue;
      if (std::find(c4.begin(), c4.end(), m) == c4.end()) c4.push_back(m);
    }
    std::sort(c4.begin(), c4.end(), [](uint8_t a, uint8_t b) { return MOVE_TBL[a].power > MOVE_TBL[b].power; });
    for (int s = 0; s < MOVE_SLOTS; s++) p.moves[s] = s < (int)c4.size() ? c4[s] : 0;
  }
  combatantFromPet(c, p);
  c.ppOn = true;
}
static uint8_t capOf(const Trainer &t) { uint8_t c = 0; for (int i = 0; i < t.count; i++) if (t.team[i].level > c) c = t.team[i].level; return c; }

// One fight, squads already built. True if we win.
static bool fight(Combatant *us, uint8_t nu, Combatant *them, uint8_t nt) {
  int a = 0, b = 0; TurnLog lg;
  for (int turns = 0; turns < 600; turns++) {
    Combatant &A = us[a], &B = them[b];
    uint8_t ma = aiChooseMove(A, B, true), mb = aiChooseMove(B, A, true);
    Combatant *f = &A, *s = &B; uint8_t mf = ma, ms = mb;
    if (!battleMovesFirst(A, ma, B, mb)) { f = &B; s = &A; mf = mb; ms = ma; }
    battleAct(*f, *s, mf, lg); if (!s->fainted()) battleAct(*s, *f, ms, lg);
    battleEndTurn(A, lg); battleEndTurn(B, lg);
    if (A.fainted()) { int n = battleNextStanding(us, nu, (uint8_t)a); if (n < 0) return false; a = n; }
    if (B.fainted()) { int n = battleNextStanding(them, nt, (uint8_t)b); if (n < 0) return true; b = n; }
  }
  return false;
}
struct Build { uint8_t iv, tr; Moves mv; };
// One whole run. PP carried per team member across the five fights.
static int run(uint8_t region, const std::vector<int16_t> &team, Build bd) {
  const Trainer *T = TRAINER_SETS[region].list;
  uint8_t ppLeft[6][MOVE_SLOTS]; bool fresh[6] = { true, true, true, true, true, true };
  for (int seat = LF; seat < TRAINER_COUNT; seat++) {
    const Trainer &t = T[seat];
    uint8_t cap = capOf(t), n = (uint8_t)std::min<int>(t.count, (int)team.size());
    Combatant us[6], them[6];
    for (int i = 0; i < n; i++) {
      mk(us[i], team[i], cap, bd.iv, bd.tr, bd.mv);
      if (!fresh[i]) memcpy(us[i].pp, ppLeft[i], sizeof(ppLeft[i]));
    }
    for (int k = 0; k < t.count; k++) mk(them[k], t.team[k].dex, t.team[k].level, HARD_IV, Pet::trMaxFor(HARD_IV), M_FULL);
    bool won = fight(us, n, them, t.count);
    for (int i = 0; i < n; i++) { memcpy(ppLeft[i], us[i].pp, sizeof(ppLeft[i])); fresh[i] = false; }
    if (!won) return seat - LF;          // seats beaten
  }
  return 5;
}
int main() {
  const char *RN[] = { "Kanto", "Johto", "Hoenn", "Sinnoh", "Unova", "Kalos", "Alola", "Galar", "Paldea" };
  // the candidate pool: drawable final forms, no legendaries, strong ones
  std::vector<int16_t> pool;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    const DexEntry &e = DEX_TBL[d];
    int bst = e.bHp + e.bAtk + e.bDef + e.bSpe + e.bSpA + e.bSpD;
    static const int BST_MAX = getenv("LS_BST_MAX") ? atoi(getenv("LS_BST_MAX")) : 999;
    if (e.evolvesTo || e.rarity == R_LEGENDARIO || !speciesShowable(d) || bst < 470 || bst > BST_MAX) continue;
    pool.push_back(d);
  }
  printf("pool: %zu final forms, BST >= 470%s, no legendaries\n", pool.size(), getenv("LS_BST_MAX") ? " and <= LS_BST_MAX" : "");
  const int R = 40;
  for (uint8_t r = 0; r < GYM_REGIONS; r++) {
    const Trainer *T = TRAINER_SETS[r].list;
    // 1v1 score against every League creature, at the champion's cap
    uint8_t cap = capOf(T[TRAINER_COUNT - 1]);
    std::vector<std::pair<int, int16_t>> sc;
    for (int16_t d : pool) {
      int w = 0;
      for (int seat = LF; seat < TRAINER_COUNT; seat++)
        for (int k = 0; k < T[seat].count; k++) {
          Combatant A, B;
          mk(A, d, cap, 31, 100, M_FULL);
          mk(B, T[seat].team[k].dex, T[seat].team[k].level, HARD_IV, Pet::trMaxFor(HARD_IV), M_FULL);
          w += fight(&A, 1, &B, 1);
        }
      sc.push_back({ w, d });
    }
    std::sort(sc.rbegin(), sc.rend());
    std::vector<int16_t> best;
    for (int i = 0; i < 6; i++) best.push_back(sc[i].second);
    const Build B_BEST = { 31, 100, M_FULL }, B_NOTM = { 31, 100, M_LEVELUP }, B_HALF = { 16, 50, M_FULL }, B_RAW = { 16, 0, M_FULL };
    int wins[5] = {}, seats[5] = {};
    const Build bs[4] = { B_BEST, B_NOTM, B_HALF, B_RAW };
    for (int k = 0; k < 4; k++)
      for (int i = 0; i < R; i++) { int s = run(r, best, bs[k]); seats[k] += s; wins[k] += s == 5; }
    for (int i = 0; i < R; i++) {         // random teams
      std::vector<int16_t> t;
      while (t.size() < 6) { int16_t d = pool[random(pool.size())]; if (std::find(t.begin(), t.end(), d) == t.end()) t.push_back(d); }
      int s = run(r, t, B_HALF); seats[4] += s; wins[4] += s == 5;
    }
    printf("\n%s (cap L%u)  best six:", RN[r], cap);
    for (int16_t d : best) printf(" %s", DEX_TBL[d].name);
    printf("\n  %-34s run won   avg seats beaten\n", "");
    const char *nm[5] = { "BEST (IV31, trained 100, TMs)", "NO-TM (same, level-up moves only)", "HALF (IV16, trained 50)", "RAW (IV16, untrained)", "RANDOM six (IV16, trained 50)" };
    for (int k = 0; k < 5; k++) printf("  %-34s %3d%%      %.1f / 5\n", nm[k], wins[k] * 100 / R, seats[k] / (double)R);
    fflush(stdout);
  }
}
