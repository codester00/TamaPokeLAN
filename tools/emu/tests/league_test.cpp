// The League run (v3.94): the Elite Four and the champion in one run. Fights
// are played through battleTap() and the League screen's taps, not called
// around them.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "inventory.h"
#include "items.h"
#include "pp.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
#include <string>

uint32_t g_seed = 0x1EA6;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void battleTap(int16_t x, int16_t y);
int btlCellIndexAt(int16_t x, int16_t y);
void startTrainerBattle(uint8_t idx, bool hard);
int leagueNext();
bool gymSeatOpen(uint8_t region, uint8_t idx, bool hard);
void leagueBtnRect(int i, int *x, int *y, int *w, int *h);
bool uiButtonDisabled(int i);
int uiSleepButton(int *cx, int *cy);
void partySlotPos(int i, int *x, int *y);
extern Pet pet; extern Party party; extern Inventory bag;
extern bool blackoutOpen;
extern bool battleOpen, leagueOpen, gymOpen, gymPick, bagOpen, gymHard;
extern uint8_t btlMsgCount, btlMenu, gymRegion, gymPage, bagPage, ppUpWho;
extern uint32_t btlWinUntil;
extern uint16_t squadMask;
extern uint8_t squadOrderN, squadOrder[];
extern Combatant btlYou, btlFoe, btlSquad[];
extern uint8_t btlSquadN;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const int LF = TRAINER_COUNT - TRAINER_ELITE4 - 1;   // the first Elite Four seat
static int16_t cx[4], cy[4];
static void findCells() {
  for (int i = 0; i < 4; i++) cx[i] = -1;
  for (int16_t y = 200; y < 466; y += 3)
    for (int16_t x = 0; x < 466; x += 3) {
      int c = btlCellIndexAt(x, y);
      if (c >= 0 && c < 4 && cx[c] < 0) { cx[c] = x + 10; cy[c] = y + 10; }
    }
}
static void tick(int ms) { emuAdvanceMs(ms); }
// Plays the open fight to its end through the screen. win: the foe drops to 1 HP
// before every move; lose: every one of ours does.
static void play(bool win) {
  for (int g = 0; g < 3000 && battleOpen; g++) {
    tick(300);
    if (btlWinUntil || btlMsgCount) { battleTap(233, 233); continue; }
    if (btlMenu != 0 && btlMenu != 1) { btlMenu = 0; continue; }
    if (win) { btlFoe.hp = 1; btlYou.hp = btlYou.maxHp; }
    else { btlYou.hp = 1; for (int i = 0; i < btlSquadN; i++) if (btlSquad[i].hp) btlSquad[i].hp = 1; }
    if (btlMenu == 0) battleTap(cx[0], cy[0]);            // FIGHT
    for (int i = 0; i < 4 && btlMenu == 1; i++) battleTap(cx[i], cy[i]);
  }
}
static void tapBtn(int i) { int x, y, w, h; leagueBtnRect(i, &x, &y, &w, &h); render(); onTap((int16_t)(x + w / 2), (int16_t)(y + h / 2)); }
static uint8_t ppOf(int slot) { return ppLeftSlot(pet.uid, (uint8_t)slot, pet.moves[slot], ppUpsAt(pet.ppUps, slot)); }
static bool textOnScreen(const char *s) {
  std::string log; Arduino_Canvas::textLog = &log; render(); Arduino_Canvas::textLog = nullptr;
  return log.find(s) != std::string::npos;
}

int main() {
  emuVirtualClock(true);
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6, false);           // a Charizard
  pet.ageMinutes = 99UL * MINUTES_PER_LEVEL;
  for (auto &m : party.slots) m = PartyMon();
  pet.moves[0] = MV_SURF; pet.moves[1] = MV_TACKLE;   // SURF first: Agatha's ghosts ignore TACKLE pet.moves[2] = 0; pet.moves[3] = 0;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  ppRefillAll();
  gymRegion = 0; gymHard = false;
  for (int i = 0; i < LF; i++) pet.winBadge(0, (uint8_t)i, false);
  squadMask = 1; squadOrderN = 1; squadOrder[0] = 0;
  findCells();
  ck(cx[0] >= 0, "(the battle grid is found)");

  // 1. Past the first Elite Four seat nothing opens without a run.
  ck(gymSeatOpen(0, LF, false) && !gymSeatOpen(0, LF + 1, false), "only the first Elite Four seat is open");
  startTrainerBattle(LF + 1, false);
  ck(!battleOpen && leagueNext() == -1, "the second seat cannot be fought on its own");
  gymOpen = true; gymPick = false; gymPage = (uint8_t)((LF + 1) / 5);
  ck(textOnScreen("BEAT ALL IN ONE RUN"), "the gym list says how the later seats are reached");
  gymOpen = false;

  // 2. The first seat starts a run; no rest and no refill during it.
  startTrainerBattle(LF, false);
  ck(battleOpen && leagueNext() == LF && ppRefillHeld(), "the first seat starts a run, the refill held");
  ck(uiButtonDisabled(uiSleepButton(nullptr, nullptr)), "the bed button is refused during a run");
  uint32_t w0 = pet.wallet;
  play(true);
  uint32_t firstPay = pet.wallet - w0;
  ck(pet.hasBadge(0, LF, false) && leagueNext() == LF + 1, "a win keeps the badge and moves the run on");
  ck(leagueOpen, "the League screen comes up between fights");
  ck(firstPay == trainerPrize(0, LF, false, true), "the first win pays the full prize");
  uint8_t pp0 = ppOf(0);
  ck(pp0 < ppMaxFor(MV_SURF, 0), "(the fight spent PP)");
  ck(!ppRefillAll() && ppOf(0) == pp0, "PP does not come back between League fights");

  // 3. ETHER from the bag, between fights: 10 back, never past the max.
  bag.add(IT_ETHER, 2);
  uint8_t d = bag.distinctCount(), at = 0;
  for (uint8_t i = 0; i < d; i++) if (bag.keyAt(i) == IT_ETHER) at = i;
  tapBtn(1);                                            // BAG
  ck(bagOpen && leagueOpen, "BAG opens the bag over the League screen");
  bagPage = at / 5; render(); onTap(233, 112 + (at % 5) * 58 + 25);
  ck(ppUpWho != 0xFF, "an ETHER asks which move");
  render(); onTap(233, 112 + 0 * 58 + 25);
  uint8_t want = (uint8_t)(pp0 + 10 > ppMaxFor(MV_SURF, 0) ? ppMaxFor(MV_SURF, 0) : pp0 + 10);
  ck(ppOf(0) == want && bag.count(IT_ETHER) == 1, "the ETHER gives SURF 10 PP back");
  bag.add(IT_ETHER, 0);
  bagPage = at / 5; render(); onTap(233, 112 + (at % 5) * 58 + 25);
  render(); onTap(233, 112 + 1 * 58 + 25);              // TACKLE is full
  ck(bag.count(IT_ETHER) == 1, "an ETHER on a full move is refused and kept");
  bagOpen = false;
  ck(leagueOpen && !bagOpen, "closing the bag goes back to the League screen");

  // 4. NEXT fights the next seat; a loss ends the run, badges kept.
  tapBtn(0);
  ck(battleOpen && btlYou.dex == pet.speciesId, "NEXT starts the next seat with the same team");
  play(false);
  ck(leagueNext() == -1 && !ppRefillHeld(), "a loss ends the run and frees the refill");
  ck(pet.hasBadge(0, LF, false) && !gymSeatOpen(0, LF + 1, false) && gymSeatOpen(0, LF, false),
     "the badge stays, and the next try starts at the first seat again");
  ck(!uiButtonDisabled(uiSleepButton(nullptr, nullptr)) || pet.sleeping, "the bed works again");
  // Mom's blackout screen, and only that: a tap on the pet screen could start
  // the farewell this level 100 Charizard is offered
  ck(blackoutOpen, "the loss blacks out as ever");
  for (int g = 0; g < 20 && blackoutOpen; g++) { tick(3000); onTap(233, 233); }
  ck(pet.ceremony == CER_NONE, "(no ceremony started)");

  // 5. The rematch of a held badge pays half; then all five, and the run ends.
  ppRefillAll();
  pet.energy = 100;
  w0 = pet.wallet;
  startTrainerBattle(LF, false);
  play(true);
  ck(pet.wallet - w0 == trainerPrize(0, LF, false, false) && pet.wallet > w0 &&
     trainerPrize(0, LF, false, false) * 2 == trainerPrize(0, LF, false, true),
     "a rematch of a held badge pays half");
  for (int s = LF + 1; s < TRAINER_COUNT; s++) {
    tapBtn(0);
    play(true);
  }
  ck(pet.hasBadge(0, TRAINER_COUNT - 1, false), "the champion falls as the run's fifth fight");
  ck(leagueNext() == -1 && !ppRefillHeld() && !leagueOpen, "the champion's defeat ends the run");

  // 6. GIVE UP takes two taps.
  startTrainerBattle(LF, false);
  play(true);
  tapBtn(2);
  ck(leagueNext() == LF + 1, "one tap on GIVE UP only asks");
  tapBtn(2);
  ck(leagueNext() == -1 && !leagueOpen, "the second ends the run");

  // 7. A run lasts an hour at most.
  startTrainerBattle(LF, false);
  play(true);
  tick(59UL * 60 * 1000);
  ck(leagueNext() == LF + 1, "(still on before the hour)");
  tick(2UL * 60 * 1000);
  ck(leagueNext() == -1 && !ppRefillHeld(), "the hour ends the run");

  // 8. Hard keeps its own run: an easy run does not open a hard seat.
  for (int i = 0; i < LF; i++) pet.winBadge(0, (uint8_t)i, true);
  leagueOpen = false;
  startTrainerBattle(LF, false);
  ck(leagueNext() == LF && !gymSeatOpen(0, LF + 1, true), "an easy run opens no hard seat");
  play(true);

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
