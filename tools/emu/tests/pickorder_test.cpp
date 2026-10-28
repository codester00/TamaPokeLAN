// The team picker's battle order (v3.70). The squad used to be the live pet
// first and then the party in slot order, whatever the player tapped, so a
// creature meant to come in second (a Smeargle waiting to SKETCH) could only
// lead or come in when something fainted. Now the order of the taps is the
// order of the fight, each cell shows its place, the last team is remembered by
// creature ID, and the picker pages on the rim with the scrollbar every paged
// screen has. Driven through the real screen.
#include <cstring>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include <cstdio>
uint32_t g_seed = 0x0D3E; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void onRim(int dir);
void pickDefault(uint8_t cap);
uint8_t pickPosition(uint8_t n);
void pickCellPos(uint8_t i, int *x, int *y);
void pickGoPos(int *x, int *y);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
uint8_t uiRimPages();
extern Pet pet;
extern bool pickOpen, gymOpen, battleOpen;
extern uint8_t pickTrainer, pickPage, pickRegion;
extern bool pickHard;
extern uint16_t squadMask;
extern Combatant btlSquad[];
extern uint8_t btlSquadN;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// open the picker for Brock on easy, as the gym list does
static void openPick() {
  pickTrainer = 0; pickHard = false; pickRegion = 0; pickPage = 0;
  pickDefault(6);
  pickOpen = true; gymOpen = false; battleOpen = false;
  render();
}
// tap the cell of candidate n (0 = the pet, k = party slot k-1) on its page
static void tapCandidate(uint8_t n) {
  uint8_t seen = 0;
  for (uint8_t c = 0; c <= PARTY_SLOTS; c++) {
    bool exists = c == 0 ? !pet.isEgg() : !party.slots[c - 1].empty();
    if (!exists) continue;
    if (c == n) {
      pickPage = (uint8_t)(seen / 6);
      int x, y; pickCellPos((uint8_t)(seen % 6), &x, &y);
      onTap((int16_t)(x + 40), (int16_t)(y + 30));
      return;
    }
    seen++;
  }
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  pet.ageMinutes = 30UL * MINUTES_PER_LEVEL;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  const int16_t DEX[PARTY_SLOTS] = { 25, 235, 7, 1, 16, 19 };   // SMEARGLE is slot 2
  for (int i = 0; i < PARTY_SLOTS; i++) {
    PartyMon m; m.dex = DEX[i]; m.level = 30;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
    m.moves[0] = MV_TACKLE;
    m.uid = pet.mintUid();
    party.replaceAt(i, m);
  }
  if (!pet.uid.valid()) pet.uid = pet.mintUid();

  // --- 1. it opens with six, numbered in pool order --------------------------------
  openPick();
  ck(!strcmp(SCREEN_NAME[uiCurrentScreen()], "pick"), "(the picker is open)");
  ck(pickPosition(0) == 1 && pickPosition(1) == 2 && pickPosition(6) == 0,
     "it opens with the first six, the pet leading");

  // --- 2. the order of the taps is the order of the fight ---------------------------
  for (uint8_t n = 0; n <= PARTY_SLOTS; n++) if (squadMask & (1 << n)) tapCandidate(n);
  ck(squadMask == 0 && pickPosition(0) == 0, "tapping each chosen one takes it out");
  tapCandidate(3);   // SQUIRTLE leads
  tapCandidate(2);   // SMEARGLE second
  tapCandidate(0);   // the pet third
  ck(pickPosition(3) == 1 && pickPosition(2) == 2 && pickPosition(0) == 3,
     "each cell's place is the order it was tapped in");
  tapCandidate(3);
  ck(pickPosition(2) == 1 && pickPosition(0) == 2, "taking the lead out moves the rest up");
  tapCandidate(3);
  ck(pickPosition(3) == 3, "and tapping it again puts it at the back");
  // SMEARGLE, pet, SQUIRTLE: fight
  { int x, y; pickGoPos(&x, &y); onTap((int16_t)x, (int16_t)y); }
  ck(battleOpen && btlSquadN == 3, "FIGHT starts the battle with the three chosen");
  ck(btlSquad[0].dex == 235 && btlSquad[1].dex == pet.speciesId && btlSquad[2].dex == 7,
     "in the order picked: SMEARGLE leads, the pet comes in second");
  battleOpen = false;

  // --- 3. the last team is remembered, by creature ------------------------------------
  openPick();
  ck(pickPosition(2) == 1 && pickPosition(0) == 2 && pickPosition(3) == 3 && pickPosition(1) == 0,
     "the next time it opens with the last team, in its order");
  PartyMon a = party.slots[0], b = party.slots[1];   // swap PIKACHU and SMEARGLE in the party
  party.replaceAt(0, b); party.replaceAt(1, a);
  openPick();
  ck(pickPosition(1) == 1 && pickPosition(2) == 0,
     "a creature moved to another party slot is still the one remembered");
  // a hard leader with a smaller team: the remembered order, cut to its size
  pickTrainer = 0; pickHard = true; pickPage = 0;
  pickDefault(2);
  ck(pickPosition(1) == 1 && pickPosition(0) == 2 && pickPosition(3) == 0,
     "a two-creature leader keeps the first two of it");

  // --- 4. seven candidates page on the rim, with the scrollbar -------------------------
  openPick();
  ck(uiRimPages() == 2, "seven candidates make two pages, paged on the rim");
  // pixels on the scrollbar's ring (radius 222, the right-hand side) that are
  // not the screen's own background, right of everything else it draws (x 420+)
  auto rimInk = [&]() {
    render();
    const uint16_t *fb = gfx->buffer();
    const uint16_t bg = fb[16 * 466 + 233];   // the top of the disc, under nothing
    long n = 0;
    for (int y = 0; y < 466; y++)
      for (int x = 420; x < 466; x++) {
        int dx = x - 233, dy = y - 233, r2 = dx * dx + dy * dy;
        if (r2 < 218 * 218 || r2 > 226 * 226) continue;
        if (fb[y * 466 + x] != bg) n++;
      }
    return n;
  };
  long withBar = rimInk();
  ck(withBar > 40, "the picker draws a scrollbar on the rim");
  onRim(1); render();
  ck(pickPage == 1 && pickOpen, "the rim drag pages it, and it stays open");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
