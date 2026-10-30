// A trainer win's prize money (v3.79): Gen 3's GetTrainerMoneyToGive() --
// 4 x the level of the trainer's last creature x the class value (25 leaders
// and Elite Four, 50 a champion). Hard pays that, easy half, a win after the
// first half again. The figures are pinned to the games' published prizes, not
// to the formula, and the win is played through the sketch's own taps.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x9121;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void onTap(int16_t x, int16_t y);
void startTrainerBattle(uint8_t idx, bool hard);
void startBattle(int16_t dex, uint8_t lvl);
int btlCellIndexAt(int16_t x, int16_t y);
extern Pet pet;
extern bool battleOpen, btlOver, btlWon;
extern uint8_t btlMsgCount, gymRegion;
extern uint32_t btlPrize;
extern Combatant btlYou;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
static void fresh(int16_t dex, uint8_t level) {
  pet.dbgHatchAs(dex, false);
  pet.ageMinutes = (uint32_t)(level - 1) * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}
static void playOut() {
  int16_t mx = 0, my = 0;
  int slot = 0;
  for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i]) { slot = i; break; }
  cellPoint(slot, mx, my);
  for (int t = 0; t < 800 && battleOpen; t++) {
    if (btlMsgCount) onTap(233, 233);
    else onTap(mx, my);
  }
}
// One fight against Kanto's Brock; returns what the wallet gained.
static uint32_t brock(bool hard, bool *won) {
  fresh(150, 100);                    // Mewtwo, capped to Brock's level by the ladder
  gymRegion = 0;
  uint32_t before = pet.wallet;
  startTrainerBattle(0, hard);
  playOut();
  *won = btlWon;
  return pet.wallet - before;
}

int main() {
  // ---- the rule, against the games' own figures -------------------------------
  ck(trainerPrize(0, 0, true, true) == 1400, "Brock pays FireRed's $1,400 on hard (Onix L14)");
  ck(trainerPrize(0, 7, true, true) == 5000, "Giovanni $5,000 (Rhydon L50)");
  ck(trainerPrize(0, 11, true, true) == 6000, "Lance $6,000 (FireRed's Dragonite L60)");
  ck(trainerPrize(2, 12, true, true) == 11600, "Emerald's Wallace $11,600 (Milotic L58, class 50)");
  ck(trainerPrize(0, 12, true, true) == 13000, "Kanto's champion $13,000 (Blastoise L65, the champion's class 50)");
  ck(trainerPrize(0, 0, false, true) == 700 && trainerPrize(0, 0, true, false) == 700 &&
     trainerPrize(0, 0, false, false) == 350, "easy halves it, and a rematch halves it again");
  ck(trainerPrize(GYM_REGIONS, 0, true, true) == 0 && trainerPrize(0, TRAINER_COUNT, true, true) == 0,
     "nothing outside the ladders");
  bool paid = true;
  for (uint8_t r = 0; r < GYM_REGIONS; r++)
    for (uint8_t i = 0; i < TRAINER_COUNT; i++) {
      uint32_t p = trainerPrize(r, i, true, true);
      if (!p || p > Pet::WALLET_CAP) paid = false;
    }
  ck(paid, "every leader of every region pays something, under the wallet's cap");

  // ---- the win, through the sketch ----------------------------------------------
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(150, false);
  pet.wallet = 0;
  bool won;
  uint32_t got = brock(false, &won);
  ck(won && got == 700, "first easy win over Brock: $700");
  ck(btlPrize == 0 || !battleOpen, "(the fight closed)");
  got = brock(false, &won);
  ck(won && got == 350, "a rematch on easy: $350");
  got = brock(true, &won);
  ck(won && got == 1400, "first hard win: the game's $1,400");
  got = brock(true, &won);
  ck(won && got == 700, "hard rematch: $700");
  {
    Pet reloaded; reloaded.begin();
    ck(reloaded.wallet == 700 + 350 + 1400 + 700, "the money is in the save");
  }
  // A wild win and a loss pay nothing.
  fresh(150, 100);
  uint32_t before = pet.wallet;
  startBattle(10, 2);
  playOut();
  ck(btlWon && pet.wallet == before, "a one-off wild win pays nothing");
  fresh(10, 1);                       // a level 1 Caterpie against Brock
  before = pet.wallet;
  gymRegion = 0;
  startTrainerBattle(0, false);
  playOut();
  ck(!btlWon && pet.wallet == before, "a loss pays nothing");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
