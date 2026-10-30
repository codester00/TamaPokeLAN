// What the Pokedex grid shows (owner, v3.89): a species fills in once it has
// been the player's -- raised, held right now, or a legendary or one-off caught
// -- and gets the green dot once it is registered (raised and fully logged).
// It used to fill only when registered and dot what was held right now. Read
// off the panel, cell by cell, not off dexObtained() alone: the grid is the
// caller that matters.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "wild.h"
#include <cstdio>
#include <set>
#include <string>

uint32_t g_seed = 0xF111;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void render();
bool dexObtained(int16_t dex);
extern Pet pet;
extern Party party;
extern Arduino_Canvas *gfx;
extern bool galleryOpen, galleryPick, galleryDirty, jumpOpen;
extern uint8_t galleryPage, galleryRegion;
extern int16_t galleryDetail, galleryJumpMark;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static const uint16_t DOT = 0x5DCD;            // UI_BAR_OK
// A cell's colours in its middle -- clear of the dot corner and of the rim's
// scrollbar, which crosses the bottom-right cell: a silhouette is one ink
// colour, a filled-in sprite several.
static int cellColours(int16_t dex) {
  int k = (dex - 1) % 16, x = 73 + (k % 4) * 80, y = 84 + (k / 4) * 80;
  const uint16_t *fb = gfx->buffer();
  uint16_t bg = fb[(y + 40) * 466 + x + 2];    // the cell's left edge: background
  std::set<uint16_t> seen;
  for (int yy = y + 14; yy < y + 66; yy++)
    for (int xx = x + 14; xx < x + 66; xx++) {
      uint16_t c = fb[yy * 466 + xx];
      if (c != bg) seen.insert(c);
    }
  return (int)seen.size();
}
static bool cellDot(int16_t dex) {
  int k = (dex - 1) % 16, x = 73 + (k % 4) * 80, y = 84 + (k / 4) * 80;
  return gfx->buffer()[(y + 9) * 466 + x + 9] == DOT;
}
static void showPage(int16_t dex) {
  galleryOpen = true; galleryPick = false; galleryDetail = 0; jumpOpen = false;
  galleryRegion = 0; galleryPage = (uint8_t)((dex - 1) / 16); galleryJumpMark = 0;
  galleryDirty = true;
  render();
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);      // CHARMANDER: raised, registered
  const int16_t RAISED = 4, HELD = 16, NONE = 10, LEGEND = 144;   // Charmander, Pidgey, Caterpie, Articuno
  for (auto &m : party.slots) m = PartyMon();
  PartyMon p; p.dex = HELD; p.level = 5;          // caught, never raised
  party.slots[0] = p;
  int li = -1;
  for (uint8_t i = 0; i < wildLegendCount(); i++) if (wildLegendDex(i) == LEGEND) li = i;
  ck(li >= 0, "(Articuno is an Explore legendary)");
  pet.setLegendCaught((uint8_t)li);               // caught once, then traded away

  ck(pet.isRegistered(RAISED) && !pet.isRegistered(HELD) && !pet.isRegistered(NONE) && !pet.isRegistered(LEGEND),
     "(only the raised one is registered)");
  ck(dexObtained(RAISED) && dexObtained(HELD) && dexObtained(LEGEND) && !dexObtained(NONE),
     "dexObtained: raised, held and a caught legendary are the player's; the rest is not");

  showPage(RAISED);
  int cr = cellColours(RAISED), ch = cellColours(HELD), cn = cellColours(NONE);
  printf("      colours: raised %d, held %d, never %d\n", cr, ch, cn);
  ck(cr > 4 && ch > 4, "a raised and a held species are drawn filled in");
  ck(cn >= 1 && cn <= 2, "a species never had is a silhouette");
  ck(cellDot(RAISED), "the raised one has the green dot");
  ck(!cellDot(HELD), "a held but never raised one has no dot");

  showPage(LEGEND);
  printf("      colours: caught legendary %d\n", cellColours(LEGEND));
  ck(cellColours(LEGEND) > 4 && !cellDot(LEGEND), "a legendary caught and traded away stays filled, no dot");

  // and the creature's page agrees with its cell: the name once it is yours
  std::string held, never;
  Arduino_Canvas::textLog = &held;  galleryDetail = HELD; render();
  Arduino_Canvas::textLog = &never; galleryDetail = NONE; render();
  Arduino_Canvas::textLog = nullptr;
  ck(held.find(DEX_TBL[HELD].name) != std::string::npos, "a held species' page shows its name");
  ck(never.find("???") != std::string::npos && never.find(DEX_TBL[NONE].name) == std::string::npos,
     "a species never had shows ???");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
