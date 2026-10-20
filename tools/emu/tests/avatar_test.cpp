// The avatar picker (v3.38): every Gen 1 trainer class is playable, which is
// too many to step through one tap at a time, so the avatar on the player card
// opens a paged grid instead.
//
// Driven through the sketch's own onTap()/onRim()/onSwipe handlers and aimed
// with the firmware's own layout functions (avatarCellPos), never re-typed
// coordinates -- see CLAUDE.md, "a test that keeps its own copy of a LAYOUT".
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "avatars.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0xA7A7;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y); void onSwipeV(int dir); void onSwipe(int dir); void onRim(int dir);
uint8_t uiCurrentScreen(); extern const char *const SCREEN_NAME[];
uint8_t avatarPages(); void avatarCellPos(uint8_t slot, int *x, int *y);
int avatarAt(int16_t x, int16_t y);
int uiTapFinger();
void uiTileGo(int i); int uiTileIndex();
extern Pet pet;
extern bool playerOpen, avatarPickOpen;
extern uint8_t playerPage, avatarPage;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static bool on(const char *name) { return !strcmp(SCREEN_NAME[uiCurrentScreen()], name); }

#define PER_PAGE 9   // the grid is 3x3; avatarAt() is what the test trusts

// The player card's avatar, as the card draws it: centred, below the name.
static void tapCardAvatar() { onTap(233, 108); }

static void openPicker() {
  playerOpen = true; playerPage = 0; avatarPickOpen = false;
  tapCardAvatar();
}

// Tap the middle of the cell that shows avatar i, paging there on the rim.
static bool pick(int i) {
  openPicker();
  if (!avatarPickOpen) return false;
  for (int k = 0; k < 8 && avatarPage != i / PER_PAGE; k++)
    onRim(avatarPage < i / PER_PAGE ? 1 : -1);
  int x, y;
  avatarCellPos((uint8_t)(i % PER_PAGE), &x, &y);
  onTap((int16_t)(x + uiTapFinger() / 2), (int16_t)(y + uiTapFinger() / 2));
  return true;
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);   // a hang still shows how far it got
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();

  // ---- the stored index is APPEND-ONLY: the first eight never move
  static const char *FIRST[8] = { "RED", "BLUE", "GIRL", "ACE M", "ACE F", "HIKER", "SAILOR", "YOUNGSTER" };
  bool same = AVATAR_COUNT >= 8;
  for (int i = 0; same && i < 8; i++) same = !strcmp(AVATARS[i].name, FIRST[i]);
  ck(same, "the eight original avatars keep their saved numbers");
  ck(AVATAR_COUNT == 19, "every trainer class is playable (19 avatars)");
  ck(!strcmp(NPC_ART[NPC_MOM].name, "MOM") && !strcmp(NPC_ART[NPC_OAK].name, "OAK"),
     "Mom and Oak are drawn by the game, not offered as avatars");
  bool npcOffered = false;
  for (int i = 0; i < AVATAR_COUNT; i++)
    npcOffered |= !strcmp(AVATARS[i].name, "MOM") || !strcmp(AVATARS[i].name, "OAK");
  ck(!npcOffered, "and neither appears in the picker");

  // ---- opening
  pet.avatar = 14;
  openPicker();
  ck(on("avatar"), "tapping the avatar on the card opens the picker");
  ck(avatarPage == 14 / PER_PAGE, "and it opens on the page holding the one you wear");
  gfx->frameReady = false; render();
  ck(gfx->frameReady, "the picker flushes");
  ck(avatarPages() == (AVATAR_COUNT + PER_PAGE - 1) / PER_PAGE, "every avatar has a page");

  // ---- every avatar can be chosen, and choosing it closes back to the card
  int chosen = 0;
  for (int i = 0; i < AVATAR_COUNT; i++) {
    pet.avatar = (uint8_t)((i + 1) % AVATAR_COUNT);
    if (pick(i) && pet.avatar == i && !avatarPickOpen && on("player")) chosen++;
    else printf("      avatar %d (%s) could not be chosen\n", i, AVATARS[i].name);
  }
  ck(chosen == AVATAR_COUNT, "every avatar is reachable and selectable by a tap");

  // and the choice is saved
  pick(AVATAR_COUNT - 1);
  { Pet after; after.begin(); ck(after.avatar == AVATAR_COUNT - 1, "the choice survives a reload"); }

  // ---- hit areas: each is a whole finger-sized cell, inside the glass
  bool cellsOk = true, inGlass = true;
  for (uint8_t slot = 0; slot < PER_PAGE; slot++) {
    int x, y; avatarCellPos(slot, &x, &y);
    int f = uiTapFinger();
    const int cx[4] = { x, x + f - 1, x, x + f - 1 }, cy[4] = { y, y, y + f - 1, y + f - 1 };
    for (int k = 0; k < 4; k++) {
      int dx = cx[k] - 233, dy = cy[k] - 233;
      if (dx * dx + dy * dy > 228 * 228) inGlass = false;
    }
    avatarPage = 0;
    int want = slot, n = 0;
    for (int py = 0; py < 466; py += 2)
      for (int px = 0; px < 466; px += 2)
        if (avatarAt((int16_t)px, (int16_t)py) == want) n++;
    if (n * 4 < f * f * 9 / 10) { cellsOk = false; printf("      slot %u answers only %d px\n", slot, n * 4); }
  }
  ck(cellsOk, "every cell answers across its whole finger-sized area");
  ck(inGlass, "and no cell's corner leaves the round panel");

  // ---- an empty cell on the last page does nothing
  openPicker();
  for (int k = 0; k < 8 && avatarPage + 1 < avatarPages(); k++) onRim(1);
  ck(avatarPage + 1 == avatarPages(), "the rim pages through to the last page");
  int empty = AVATAR_COUNT % PER_PAGE;            // first unused slot of the last page
  if (empty) {
    uint8_t before = pet.avatar;
    int x, y; avatarCellPos((uint8_t)empty, &x, &y);
    onTap((int16_t)(x + uiTapFinger() / 2), (int16_t)(y + uiTapFinger() / 2));
    ck(avatarPickOpen && pet.avatar == before, "an empty cell past the last avatar does nothing");
  }

  // ---- leaving: down is back to the card, sideways too; neither leaves the card
  openPicker(); onSwipeV(1);
  ck(!avatarPickOpen && on("player"), "swiping down goes back to the player card");
  openPicker(); onSwipe(1);
  ck(!avatarPickOpen && on("player"), "a sideways swipe backs out to the card as well");
  openPicker(); onSwipeV(-1);
  ck(avatarPickOpen, "swiping up has nowhere deeper to go and stays");

  // ---- jumping along the tile axis lands on the CARD, never a stale picker
  avatarPickOpen = false;
  int playerTile = uiTileIndex();  // the card's own place on the axis
  openPicker();
  uiTileGo(playerTile);          // what every tile jump does: close, then open
  ck(on("player") && !avatarPickOpen, "re-entering the player tile shows the card, not the picker");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
