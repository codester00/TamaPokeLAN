// Evolution stones (v3.81, plan § E1-E3): a stone evolution happens ONLY with
// its stone, used from the bag on the live pet -- no level route, and the
// farewell does not wait for one. Gloom's two stones, Kirlia's and Snorunt's
// Dawn Stone by gender beside their own level route, Scyther's Black Augurite
// against its trade (whichever first), the Mart's prices, and a stone spent
// only when it works. The bag is tapped as a player taps it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x5707;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void bagTap(int16_t x, int16_t y);
int uiBagRowCenterY(int i);
const char *uiEvoLine(char *buf, size_t n, uint16_t *col);
extern Pet pet;
extern Inventory bag;
extern bool bagOpen;
extern uint8_t bagPage;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void raise(Pet &p, int16_t dex, uint8_t lvl, uint8_t gender = GENDER_NONE) {
  p.dbgHatchAs(dex, false);
  for (int i = 0; i < 64 && gender != GENDER_NONE; i++) {
    p.uid = p.mintUid();
    if (p.gender() == gender) break;
  }
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.energy = p.fullness = p.joy = p.hygiene = 100;
  p.careMistakes = 0;
  p.tradeEvo = false;
}
// Only this item in the bag, then tap its row as a player does.
static bool useFromBag(ItemKey k) {
  for (ItemKey j = 1; j < ITEM_COUNT; j++) while (bag.count(j)) bag.consume(j, bag.count(j));
  bag.add(k, 1);
  bagOpen = true; bagPage = 0;
  bagTap(233, (int16_t)uiBagRowCenterY(0));
  return bag.count(k) == 0;   // spent
}

int main() {
  setup();
  gRegionArt = 0xFFFF;
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // ---- the catalogue --------------------------------------------------------------
  {
    bool ok = true;
    for (ItemKey k = IT_FIRESTONE; k <= IT_ICESTONE; k++)
      if (itemEntry(k).price != 2100 || itemEntry(k).category != IC_STONE) ok = false;
    ck(ok && itemEntry(IT_BLACKAUGURITE).price == 3000, "ten stones at $2,100 and the Black Augurite at $3,000");
    ck(itemForSale(IT_THUNDERSTONE) && itemUsableInField(IT_THUNDERSTONE) &&
       !itemUsableInBattle(IT_THUNDERSTONE), "sold at the Mart, used from the bag, never in a fight");
    ck(!strcmp(itemEntry(IT_THUNDERSTONE).name, "THUNDERSTONE"), "THUNDERSTONE, the games' Gen 1-4 spelling");
    ck(DEX_TBL[182].rarity == R_EVO && DEX_TBL[475].rarity == R_EVO && DEX_TBL[478].rarity == R_EVO &&
       DEX_TBL[900].rarity == R_EVO, "Bellossom, Gallade, Froslass and Kleavor no longer hatch");
  }

  // ---- a stone evolution: no level route, the card, the farewell --------------------
  raise(pet, 25, 60);
  {
    char buf[48]; uint16_t col;
    const char *line = uiEvoLine(buf, sizeof(buf), &col);
    ck(pet.evoInfo().status == Pet::EVO_NEEDS_ITEM && !pet.canEvolveNow(),
       "a level 60 Pikachu does not evolve by level");
    ck(strstr(line, "THUNDERSTONE") != nullptr, "its card says which stone");
    ck(!pet.evolutionAhead(), "and its farewell does not wait for a stone");
  }
  ck(!useFromBag(IT_WATERSTONE) && pet.speciesId == 25 && bag.count(IT_WATERSTONE) == 1,
     "the wrong stone does nothing and is kept");
  ck(useFromBag(IT_THUNDERSTONE) && pet.speciesId == 26 && !bagOpen,
     "a Thunderstone from the bag makes it a Raichu, is spent, and the bag closes to show it");
  raise(pet, 25, 5);
  ck(useFromBag(IT_THUNDERSTONE) && pet.speciesId == 26, "at any level");
  raise(pet, 82, 50);
  ck(pet.evoInfo().status == Pet::EVO_NEEDS_ITEM && useFromBag(IT_THUNDERSTONE) && pet.speciesId == 462,
     "Magneton has no level route left: a Thunderstone makes Magnezone");

  // ---- two stones, one creature ------------------------------------------------------
  raise(pet, 44, 30);
  ck(useFromBag(IT_LEAFSTONE) && pet.speciesId == 45, "Gloom: the Leaf Stone makes Vileplume");
  raise(pet, 44, 30);
  ck(useFromBag(IT_SUNSTONE) && pet.speciesId == 182, "and the Sun Stone Bellossom");

  // ---- the Dawn Stone by gender, beside the level route ------------------------------
  raise(pet, 281, 20, GENDER_MALE);
  ck(useFromBag(IT_DAWNSTONE) && pet.speciesId == 475, "a male Kirlia: the Dawn Stone makes Gallade");
  raise(pet, 281, 20, GENDER_FEMALE);
  ck(!useFromBag(IT_DAWNSTONE) && pet.speciesId == 281, "a female one: nothing, and the stone is kept");
  raise(pet, 281, 40, GENDER_FEMALE);
  ck(pet.evoInfo().target == 282 && pet.canEvolveNow(), "and Kirlia still levels into Gardevoir");
  raise(pet, 361, 20, GENDER_FEMALE);
  ck(useFromBag(IT_DAWNSTONE) && pet.speciesId == 478, "a female Snorunt: Froslass");
  raise(pet, 361, 20, GENDER_MALE);
  ck(!useFromBag(IT_DAWNSTONE), "a male one: nothing");

  // ---- Scyther: the augurite or the trade, whichever first ---------------------------
  raise(pet, 123, 30);
  ck(pet.evoInfo().status == Pet::EVO_NEEDS_TRADE, "an unmarked Scyther's card: by trade");
  ck(useFromBag(IT_BLACKAUGURITE) && pet.speciesId == 900, "the Black Augurite makes Kleavor");
  raise(pet, 123, 30);
  pet.tradeEvo = true;                  // traded, or the Linking Cord
  ck(!useFromBag(IT_BLACKAUGURITE) && pet.speciesId == 123,
     "a Scyther marked for Scizor refuses the augurite, which is kept");
  pet.tradeEvo = false;

  // ---- no effect where nothing can evolve ----------------------------------------------
  raise(pet, 26, 30);
  ck(!useFromBag(IT_THUNDERSTONE), "a Raichu: nothing");
  raise(pet, 25, 30);
  pet.frozen = true;
  ck(!useFromBag(IT_THUNDERSTONE) && pet.speciesId == 25, "a frozen companion never evolves");
  pet.frozen = false;
  {
    Pet p;
    raise(p, 25, 30);
    gRegionArt = 1u << 0;               // Kanto only: Raichu is drawable
    ck(p.stoneTarget(2) == 26, "(Raichu with only the Kanto pack)");
    raise(p, 361, 20, GENDER_FEMALE);
    ck(p.stoneTarget(8) == 0, "Froslass cannot be drawn without Sinnoh's pack: no effect");
    gRegionArt = 0xFFFF;
  }
  {
    Pet reloaded; reloaded.begin();
    ck(reloaded.speciesId == pet.speciesId, "an evolution by stone is saved");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
