// The player's guide's tested teams: see the comment above main().
//   python3 tools/gen_guide.py --sim     # builds and runs this (minutes), writes docs/players-guide/sim.json
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
#include <cstring>
#include "wild_areas.h"
uint32_t g_seed = 4242; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static const int LF = TRAINER_COUNT - TRAINER_ELITE4 - 1;
// The hard leaders' training, for the sweep (LS_SWEEP); the firmware's value by default.
static int gFoeTr = -1;
static uint8_t gRegion = 0;
static uint8_t gSeat = 0;
static uint8_t foeTr() { return gFoeTr >= 0 ? (uint8_t)gFoeTr : hardTraining(gRegion, gSeat); }
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

// The player's guide's leader advice (tools/gen_guide.py --sim). For every
// leader, Elite Four seat and champion: the best team a player can actually
// have by then -- species met in that region's Explore areas open at that
// point (later-story slots excluded), each raised to the level cap and evolved
// by level as far as the cap allows -- picked by 1v1 wins against the leader's
// own team, then measured whole: a good player's build (IV 20, training 75,
// TMs), hard and easy, HP and PP fresh. JSON lines on stdout.
static int16_t evolveTo(int16_t d, uint8_t lvl) {
  for (int k = 0; k < 4; k++) {
    const DexEntry &e = DEX_TBL[d];
    if (!e.evolvesTo || !e.evolveLevel || e.evolveLevel > lvl || !speciesShowable(e.evolvesTo)) break;
    d = e.evolvesTo;
  }
  return d;
}
int main() {
  const int RUNS = getenv("GS_RUNS") ? atoi(getenv("GS_RUNS")) : 40;
  for (uint8_t r = 0; r < GYM_REGIONS; r++) {
    const Trainer *T = TRAINER_SETS[r].list;
    for (uint8_t seat = 0; seat < TRAINER_COUNT; seat++) {
      const Trainer &t = T[seat];
      uint8_t gate = seat < 8 ? seat : 8, cap = capOf(t);
      std::vector<int16_t> pool;
      for (uint16_t a = WILD_REGION_AREAS[r][0]; a < WILD_REGION_AREAS[r][0] + WILD_REGION_AREAS[r][1]; a++) {
        const WildArea &A = WILD_AREAS[a];
        if (A.gate > gate) continue;
        for (uint16_t i = 0; i < A.count; i++) {
          const WildSlot &s = WILD_SLOTS[A.first + i];
          if (s.gate > gate || !speciesShowable(s.dex)) continue;
          int16_t d = evolveTo(s.dex, cap);
          if (std::find(pool.begin(), pool.end(), d) == pool.end()) pool.push_back(d);
        }
      }
      gRegion = r; gSeat = seat; gFoeTr = -1;
      std::vector<std::pair<int, int16_t>> sc;
      for (int16_t d : pool) {
        int w = 0;
        for (int k = 0; k < t.count; k++)
          for (int rep = 0; rep < 3; rep++) {
            Combatant A, B;
            mk(A, d, cap, 20, 75, M_FULL);
            mk(B, t.team[k].dex, t.team[k].level, HARD_IV, foeTr(), M_FULL);
            w += fight(&A, 1, &B, 1);
          }
        sc.push_back({ w * 1000 + (int)(DEX_TBL[d].bHp + DEX_TBL[d].bAtk + DEX_TBL[d].bDef + DEX_TBL[d].bSpe + DEX_TBL[d].bSpA + DEX_TBL[d].bSpD), d });
      }
      std::sort(sc.rbegin(), sc.rend());
      std::vector<int16_t> team;
      for (size_t i = 0; i < sc.size() && team.size() < 6; i++) team.push_back(sc[i].second);
      int pct[2] = { 0, 0 };
      for (int hard = 0; hard < 2; hard++) {
        int wins = 0;
        uint8_t n = (uint8_t)std::min<int>(hard ? t.count : 6, (int)team.size());
        for (int i = 0; i < RUNS; i++) {
          Combatant us[6], them[6];
          for (int k = 0; k < n; k++) mk(us[k], team[k], cap, 20, 75, M_FULL);
          for (int k = 0; k < t.count; k++)
            mk(them[k], t.team[k].dex, t.team[k].level, hard ? HARD_IV : EASY_IV, hard ? foeTr() : 0, M_FULL);
          wins += fight(us, n, them, t.count);
        }
        pct[hard] = wins * 100 / RUNS;
      }
      printf("{\"r\":%u,\"s\":%u,\"pool\":%zu,\"team\":[", r, seat, pool.size());
      for (size_t i = 0; i < team.size(); i++) printf(i ? ",%d" : "%d", team[i]);
      printf("],\"easy\":%d,\"hard\":%d}\n", pct[0], pct[1]);
      fflush(stdout);
    }
  }
  return 0;
}
