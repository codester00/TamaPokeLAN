// The Game Corner screens (v3.53), driven through onTap()/onSwipeV()/onRim():
// the hall row sits at the top of its region's list behind Lt. Surge's gate,
// opens a hall rather than a battle, and every button does what it says --
// buying coins with step money, a spin that pays what it shows, the card flip,
// and a prize creature or TM bought for coins. The rules are gamecorner_test's.
//
// GCSHOT=<dir> writes a PPM of each screen there, for looking at by eye.
#include <cstring>
#include <cstdlib>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "gamecorner.h"
#include "wild.h"
#include <cstdio>

uint32_t g_seed = 0x6C0D;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
void onRim(int dir);
void areaRowRect(int i, int *x, int *y, int *w, int *h);
void hallBtnRect(int i, int *x, int *y, int *w, int *h);
void hallActionRect(int *x, int *y, int *w, int *h);
void hallSelRect(int i, int *x, int *y, int *w, int *h);
void hallCardsLayout(int *cardY, int *cardH, int *leftY, int *msgY);
extern char hallMsg[];
void slotBtnRect(int i, int *x, int *y, int *w, int *h);
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
void openAreaList();
int areaListEntry(uint8_t region, int k, bool *isHall);
// Tap a hall's row, wherever the firmware puts it in the list.
static int hallRowOf(uint8_t region, uint8_t hall) {
  bool isHall;
  for (int k = 0, e; (e = areaListEntry(region, k, &isHall)) >= 0; k++)
    if (isHall && e == hall) return k;
  return -1;
}
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet;
extern bool exploreOpen, areaListOpen, battleOpen;
extern uint8_t exploreRegion, areaPage, hallOpen, hallMode, hallPage, slotShown, slotBet;
extern int8_t cardShown;
extern SlotResult slotRes;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static bool on(const char *scr) { return !strcmp(SCREEN_NAME[uiCurrentScreen()], scr); }

static void shot(const char *name) {
  const char *dir = getenv("GCSHOT");
  if (!dir) return;
  render();
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6 466 466 255\n");
  const uint16_t *fb = gfx->buffer();
  for (int i = 0; i < 466 * 466; i++) {
    uint16_t c = fb[i];
    unsigned char px[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                            (unsigned char)((c & 31) << 3) };
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

static void tapRect(void (*rect)(int, int *, int *, int *, int *), int i) {
  int x, y, w, h; rect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); render();
}
static void tapSpin() { tapRect(slotBtnRect, 1); }
static void tapHall(uint8_t region, uint8_t hall) {
  int k = hallRowOf(region, hall);
  areaPage = (uint8_t)(k / 3);
  tapRect(areaRowRect, k % 3);
}
static void tapAction() { int x, y, w, h; hallActionRect(&x, &y, &w, &h); onTap(x + w / 2, y + h / 2); render(); }
static void tapYes() {
  int t1, b1, t2, b2; uiConfirmRects(&t1, &b1, &t2, &b2); onTap(233, (t1 + b1) / 2); render();
}

int main() {
  setup();
  if (pet.isEgg()) pet.dbgHatchAs(25, false);
  if (!pet.uid.valid()) pet.uid = pet.mintUid();

  // --- 1. the hall sits in Kanto's list where the story puts it, behind
  // Lt. Surge: after the areas his badge opens, before the first that needs more
  exploreOpen = true; exploreRegion = 0;
  openAreaList();
  {
    int k = hallRowOf(0, HALL_CELADON);
    bool isHall;
    int before = k > 0 ? areaListEntry(0, k - 1, &isHall) : -1;
    int after = areaListEntry(0, k + 1, &isHall);
    ck(k > 0 && before >= 0 && wildAreaGate((uint16_t)before) <= 3 && after >= 0 &&
       wildAreaGate((uint16_t)after) > 3, "Celadon sits between the gate-3 areas and the next");
  }
  render();
  ck(on("area"), "the area list is open");
  shot("gc_list_locked");
  tapHall(0, HALL_CELADON);
  ck(!hallOpen && on("area"), "a locked hall does not open");
  pet.winBadge(0, 0, false); pet.winBadge(0, 1, false); pet.winBadge(0, 2, false);
  render();
  shot("gc_list");
  tapHall(0, HALL_CELADON);
  ck(hallOpen == HALL_CELADON + 1 && on("hall") && !battleOpen, "after Lt. Surge it opens the hall, not a battle");
  shot("gc_menu");

  // --- 2. coins for step money
  pet.wallet = 5000; pet.coins = 0;
  tapRect(hallBtnRect, 2);       // Celadon: SLOTS, PRIZES, BUY 50
  ck(!pet.coins, "BUY asks first");
  shot("gc_buy");
  tapYes();
  ck(pet.coins == COINS_PER_BUY && pet.wallet == 4000, "50 coins for $1,000");

  // --- 3. the slots: a spin pays what it shows, and only after the last STOP
  tapRect(hallBtnRect, 0);
  ck(hallMode == 1 && on("hall"), "SLOTS opens the machine");
  shot("gc_slots");
  uint16_t c0 = pet.coins;
  uint32_t won = 0; int spins = 0;
  for (int i = 0; i < 40 && pet.coins >= slotBet; i++) {
    uint16_t before = pet.coins;
    tapSpin();                                    // SPIN
    ck(pet.coins == before - slotBet && slotShown == 0, "SPIN takes the bet");
    tapSpin(); tapSpin();
    ck(pet.coins == before - slotBet, "nothing paid before the last reel stops");
    tapSpin();
    ck(slotShown == 3 && pet.coins == before - slotBet + slotRes.win, "the last STOP pays the spin's win");
    won += slotRes.win; spins++;
    if (bad) break;
  }
  printf("     %d spins, %u won\n", spins, (unsigned)won);
  (void)c0;
  tapRect(slotBtnRect, 0);
  ck(slotBet == 2, "BET steps to 2");
  shot("gc_slots_bet2");
  // leaving mid-spin settles it
  pet.coins = 10;
  tapSpin();
  uint16_t mid = pet.coins;
  onSwipeV(1); render();
  ck(hallMode == 0 && pet.coins == mid + slotRes.win, "backing out mid-spin pays what the spin won");

  // --- 4. prizes: paged on the rim, a TM and a creature for coins
  pet.coins = 9999;
  memset(pet.tmOwned, 0, sizeof(pet.tmOwned));
  tapRect(hallBtnRect, 1);
  ck(hallMode == 3 && hallPage == 0, "PRIZES opens the counter");
  shot("gc_prizes");
  onRim(1); render();
  ck(hallPage == 1 && on("hall"), "the rim pages the prize list and stays on it");
  int surfAt = -1;
  for (uint8_t i = 0; i < hallPrizeCount(HALL_CELADON); i++)
    if (hallPrize(HALL_CELADON, i).kind == PRIZE_TM && !strcmp(MOVE_TBL[hallPrize(HALL_CELADON, i).what].name, "SURF")) surfAt = i;
  hallPage = (uint8_t)(surfAt / 3);
  render();
  tapRect(areaRowRect, surfAt % 3);
  shot("gc_prize_confirm");
  tapYes();
  uint8_t surf = 0;
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, "SURF")) surf = (uint8_t)i;
  ck(pet.hasTm(surf) && pet.coins == 9999 - 4000, "the SURF TM for 4,000 coins");
  tapRect(areaRowRect, surfAt % 3);
  ck(pet.coins == 5999, "an owned TM cannot be bought twice");
  hallPage = 0; render();
  uint8_t inParty = party.count(), inBox = party.boxCount();
  tapRect(areaRowRect, 0);       // ABRA
  tapYes();
  ck(party.count() + party.boxCount() == inParty + inBox + 1 && pet.coins == 5999 - 180, "an ABRA for 180 coins joins the party or box");
  bool found = false;
  for (int i = 0; i < PARTY_SLOTS; i++) if (party.slots[i].dex == 63 && party.slots[i].uid.valid()) found = true;
  for (int i = 0; i < BOX_SLOTS; i++) if (party.box[i].dex == 63 && party.box[i].uid.valid()) found = true;
  ck(found, "with an ID of its own");

  // --- 5. down backs out a level at a time
  onSwipeV(1); render();
  ck(hallOpen && hallMode == 0, "down: prizes back to the hall");
  onSwipeV(1); render();
  ck(!hallOpen && on("area"), "down: the hall back to the area list");

  // --- 6. Goldenrod has the card flip
  exploreRegion = 1; openAreaList();
  pet.winBadge(1, 0, false); pet.winBadge(1, 1, false);
  render();
  tapHall(1, HALL_GOLDENROD);
  ck(hallOpen == HALL_GOLDENROD + 1, "Goldenrod opens after Bugsy");
  shot("gc_goldenrod");
  tapRect(hallBtnRect, 1);
  ck(hallMode == 2, "CARD FLIP opens the cards");
  pet.coins = 30;
  tapAction();
  ck(pet.coins == 30 && cardShown < 0, "no guess, no flip");
  tapRect(hallSelRect, 0);       // PIKACHU
  tapAction();
  ck(cardShown >= 0, "a guess flips a card");
  ck(pet.coins == 27 + cardPayout(0, CARD_ANY, (uint8_t)cardShown), "and pays Crystal's odds on it");
  shot("gc_cards");
  tapRect(hallSelRect, 1);       // and a number
  shot("gc_cards_both");

  // the result line is SEEN (v3.67): v3.53 drew it at y 246, under the guess
  // boxes from 232, so a win or a miss never showed. Asked of the layout, and
  // read off the panel: dark text pixels in the band, none before a result.
  {
    int cardY, cardH, leftY, msgY, sx, sy, sw, sh;
    hallCardsLayout(&cardY, &cardH, &leftY, &msgY);
    hallSelRect(0, &sx, &sy, &sw, &sh);
    ck(cardY + cardH <= leftY && leftY + 8 <= msgY && msgY + 16 <= sy,
       "card, cards-left line and result line stack above the guess boxes");
    auto inkIn = [&](int y0) {
      render();
      const uint16_t *fb = gfx->buffer();
      int n = 0;
      for (int y = y0; y < y0 + 16; y++)
        for (int x = 120; x < 346; x++) {
          uint16_t c = fb[y * 466 + x];
          if (((c >> 11) << 3) + (((c >> 5) & 63) << 2) + ((c & 31) << 3) < 300) n++;
        }
      return n;
    };
    ck(inkIn(msgY) > 20, "the result of the flip is drawn where it can be read");
    hallMsg[0] = 0;
    ck(inkIn(msgY) == 0, "(and that band is empty with no result)");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
