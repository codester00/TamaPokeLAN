// A move offer waiting UNDER another screen (v3.93). Found by the v3.92
// playthrough: the pet levelled up while the bag's party ring was open, the
// ring stayed on the panel, and the next tap went to the hidden "wants to
// learn" rows -- render() drew one screen, onTap() answered another, and
// uiCurrentScreen() (the crash breadcrumb) named a third. One order now:
// uiCurrentScreen() is render()'s, and onTap() asks it.
//
// For each screen: what is DRAWN (the offer's title in the panel's text), what
// uiCurrentScreen() says, and whether a tap on every offer row teaches a move.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>
#include <string>

uint32_t g_seed = 0x1EA4;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet;
extern bool partyOpen, movePickOpen, galleryOpen, kbOpen, clockOpen, trainOpen, cardOpen, menuOpen, bagOpen;
extern bool gameOpen, sackOpen, spdOpen, catchOpen;
extern uint8_t miniGame;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void closeAll() {
  partyOpen = movePickOpen = galleryOpen = kbOpen = clockOpen = trainOpen = cardOpen = menuOpen = bagOpen = false;
  gameOpen = sackOpen = spdOpen = catchOpen = false;   // a tap on the training list starts a game
  miniGame = 0;                                        // MG_NONE
}
static void offer() {
  pet.ageMinutes = 59UL * MINUTES_PER_LEVEL;          // level 60, every slot full
  for (int i = 0; i < MOVE_SLOTS; i++) pet.moves[i] = (uint8_t)(1 + i);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.learnRange(0, 60, true);
}
static bool offerDrawn() {
  std::string log;
  Arduino_Canvas::textLog = &log;
  render();
  Arduino_Canvas::textLog = nullptr;
  return log.find("wants to learn") != std::string::npos;
}

struct Case { const char *name; bool *flag; bool offerOnTop; };

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);
  offer();
  ck(pet.hasLearnOffer(), "(a move offer is waiting)");

  bool none = false;
  const Case cases[] = {
    { "the party ring",  &partyOpen,    false },
    { "the move picker", &movePickOpen, false },
    { "the Pokedex",     &galleryOpen,  false },
    { "the keyboard",    &kbOpen,       false },
    { "the clock",       &clockOpen,    false },
    { "the training list", &trainOpen,  false },
    { "the bag",         &bagOpen,      false },
    { "the card",        &cardOpen,     true },
    { "the menu",        &menuOpen,     true },
    { "the pet screen",  &none,         true },
  };
  for (const Case &c : cases) {
    closeAll();
    offer();
    *c.flag = true;
    char m[120];
    bool drawn = offerDrawn();
    snprintf(m, sizeof(m), "%s: the offer is %s", c.name, c.offerOnTop ? "drawn on top" : "not drawn");
    ck(drawn == c.offerOnTop, m);
    snprintf(m, sizeof(m), "%s: uiCurrentScreen() names what is drawn", c.name);
    ck(!strcmp(SCREEN_NAME[uiCurrentScreen()], "learn") == drawn, m);
    if (!c.offerOnTop) {
      // a tap on every row the hidden offer would answer: the screen drawn takes
      // it (the move picker's own rows DO change a move -- that is the picker),
      // so what must survive is the offer itself, still waiting
      // (the queue holds several moves: one answered still leaves an offer, so
      // count them)
      uint8_t queued = pet.learnQCount, first = pet.learnOffer();
      for (int i = 0; i < MOVE_SLOTS; i++) {
        closeAll();
        *c.flag = true;                                // a tap may close the screen: put it back
        onTap(233, 104 + i * 56 + 25);                 // LEARN_ROW_Y(i)
      }
      snprintf(m, sizeof(m), "%s: a tap where the hidden rows are is not an answer to the offer", c.name);
      ck(queued > 1 && pet.learnQCount == queued && pet.learnOffer() == first, m);
    } else {
      onTap(233, 104 + 25);                            // the first row
      snprintf(m, sizeof(m), "%s: the offer on top takes the tap", c.name);
      ck(!pet.hasLearnOffer() || pet.moves[0] != 1, m);
    }
    closeAll();
  }
  // every screen has a name: HEALTH and the crash breadcrumb print it
  bool named = true;
  for (int s = 0; s <= 40 && named; s++) {
    if (!SCREEN_NAME[s]) { named = false; break; }
    if (!strcmp(SCREEN_NAME[s], "jump")) break;          // the last one
  }
  ck(named, "every screen has a name, up to the last (jump)");
  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
