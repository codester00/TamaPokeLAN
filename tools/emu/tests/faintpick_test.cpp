// Choosing who comes on after a faint, and the shift style (v4.02,
// postgame-plan step 1). Your creature goes down with more than one left
// standing: you pick, the pick costs no turn, a fainted one is refused, and
// with only one left it comes straight on. A trainer's goes down: SHIFT asks
// whether to switch before its next comes on (NO keeps yours, YES is a free
// switch, BACK keeps yours too). On hard the trainer picks its next by
// matchup; on easy, in order. Driven through battleTap().
#include <cstring>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "types.h"
#include "trainers.h"
#include <cstdio>
uint32_t g_seed = 0xFA17; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void startBattle(int16_t dex, uint8_t lvl);
void startTrainerBattle(uint8_t idx, bool hard);
void btlSwitchCell(uint8_t k, int *x, int *y);
void btlCellCenter(int i, int *x, int *y);
uint8_t btlMenuNow();
int8_t btlTrainerNext();
extern Pet pet;
extern bool battleOpen, btlOver, btlHard;
extern Combatant btlYou, btlFoe, btlSquad[];
extern uint8_t btlSquadN, btlSquadAt, btlFoeAt, btlMenu, btlMsgCount, btlFoeSent, btlRegion;
extern int8_t btlTrainer;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) battleTap(233, 300); }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); battleTap(x, y); }
static void tapOpt(uint8_t k) { int x, y; btlSwitchCell(k, &x, &y); battleTap(x + 80, y + 27); }
static void team() {
  if (pet.isEgg()) pet.dbgHatchAs(150, false);
  pet.dbgHatchAs(150, false);   // MEWTWO
  pet.ageMinutes = 60UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  const int16_t DEX[3] = { 6, 9, 3 };   // CHARIZARD, BLASTOISE, VENUSAUR
  for (int i = 0; i < 3; i++) {
    PartyMon m; m.dex = DEX[i]; m.level = 60;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
    m.moves[0] = MV_TACKLE;
    m.uid = pet.mintUid();
    party.slots[i] = m;
  }
  squadMask = 0x0F;   // the pet and the three
}
// Our creature at 1 HP against a level 100 MEWTWO until it goes down.
static void faintOurs() {
  for (int t = 0; t < 30 && !btlYou.fainted(); t++) {
    btlYou.hp = 1;
    btlMenu = 1;
    tapCell(0);
    clearMsgs();
  }
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  team();

  // --- 1. yours faints: you choose ------------------------------------------------
  startBattle(150, 100);
  ck(btlSquadN == 4, "(a squad of four)");
  btlSquad[1].hp = 0;                    // CHARIZARD already down
  faintOurs();
  ck(btlMenuNow() == 4, "a faint with two left standing opens the choice");
  tapOpt(0);                             // CHARIZARD: fainted
  ck(btlMenuNow() == 4 && btlSquadAt == 0, "a fainted one is refused");
  uint16_t hp3 = btlSquad[3].hp, foeHp = btlFoe.hp;
  tapOpt(2);                             // VENUSAUR, not the next standing (BLASTOISE)
  ck(btlSquadAt == 3 && btlYou.dex == 3, "the one chosen comes on, not the next in line");
  clearMsgs();
  ck(btlYou.hp == hp3 && btlFoe.hp == foeHp, "and choosing costs no turn");
  ck(btlMenuNow() == 0, "then the fight goes on");
  battleOpen = false;

  // --- 2. one left standing: straight on --------------------------------------------
  team();
  startBattle(150, 100);
  btlSquad[1].hp = 0; btlSquad[2].hp = 0;
  faintOurs();
  ck(btlMenuNow() == 0 && btlSquadAt == 3, "with one left it comes straight on");
  battleOpen = false;

  // --- 3. SHIFT: the trainer's goes down --------------------------------------------
  for (uint8_t i = 0; i < 13; i++) pet.winBadge(0, i, false);
  for (int run = 0; run < 3; run++) {
    team();
    startTrainerBattle(0, false);        // Brock: GEODUDE then ONIX
    btlFoe.hp = 1;
    btlYou.hp = btlYou.maxHp;
    btlMenu = 1;
    tapCell(0);
    clearMsgs();
    ck(btlMenuNow() == 5, run == 0 ? "the foe's faint asks whether to switch" : "(asked again)");
    if (run == 0) {
      tapCell(3);                        // NO
      clearMsgs();
      ck(btlSquadAt == 0 && btlFoeAt == 1, "NO keeps yours, and ONIX comes on");
    } else {
      tapCell(2);                        // YES
      ck(btlMenuNow() == 6, "YES opens the switch list");
      if (run == 1) {
        uint16_t full = btlSquad[1].hp;
        tapOpt(0);                       // CHARIZARD
        clearMsgs();
        ck(btlSquadAt == 1 && btlYou.hp == full && btlFoeAt == 1,
           "a free switch: CHARIZARD comes on unhurt, then ONIX");
      } else {
        battleTap(233, 396 + 20);        // BACK
        clearMsgs();
        ck(btlSquadAt == 0 && btlFoeAt == 1, "BACK keeps yours, and ONIX comes on");
      }
    }
    battleOpen = false;
  }

  // --- 4. who a trainer sends next -------------------------------------------------
  {
    int differ = 0, worse = 0, resend = 0, easyOrder = 0;
    for (uint8_t r = 0; r < GYM_REGIONS; r++)
      for (uint8_t k = 0; k < TRAINER_COUNT; k++) {
        const Trainer &t = TRAINER_SETS[r].list[k];
        if (t.count < 3) continue;
        for (int16_t d = 1; d <= 151; d += 7) {
          btlRegion = r; btlTrainer = (int8_t)k; btlFoeSent = 1; btlYou.dex = d;
          btlHard = false;
          int8_t e = btlTrainerNext();
          if (e != 1) easyOrder++;
          btlHard = true;
          int8_t h = btlTrainerNext();
          if (h <= 0) resend++;
          if (h != e) differ++;
          // the one sent must be at least as good a typing match as the one in order
          auto score = [&](int i) {
            int16_t c = t.team[i].dex;
            int off = 0, def = 0;
            for (int a = 0; a < 2; a++) {
              uint8_t ty = a ? DEX_TBL[c].type2 : DEX_TBL[c].type1;
              int v = typeEffPct(ty, DEX_TBL[d].type1, DEX_TBL[d].type2); if (v > off) off = v;
              uint8_t py = a ? DEX_TBL[d].type2 : DEX_TBL[d].type1;
              int w = typeEffPct(py, DEX_TBL[c].type1, DEX_TBL[c].type2); if (w > def) def = w;
            }
            return off - def;
          };
          if (h > 0 && score(h) < score(e)) worse++;
        }
      }
    ck(easyOrder == 0, "on easy, a trainer sends its team in order");
    ck(resend == 0, "on hard, never one already sent");
    ck(worse == 0, "on hard, never a worse typing match than the one in order");
    ck(differ > 0, "and hard does differ from the order (the matchup engaged)");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
