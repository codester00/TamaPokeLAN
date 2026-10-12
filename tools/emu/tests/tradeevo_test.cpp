// Trade evolution, as the owner specified it:
//
//   - PURE trade: Kadabra, Haunter and the rest no longer evolve at level 40.
//   - A creature received by trade is MARKED, not evolved. It evolves when it
//     is made the live pet, so both forms reach the Pokedex.
//   - Karrablast and Shelmet only for each other; Clamperl has two targets.
//
// Drives the REAL sketch for the card, because the card used to keep its own
// copy of the evolution rule (CLAUDE.md § 1) and a test of evoInfo() alone
// would pass with the card ignoring it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include "i18n.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x7EAD;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
extern Pet pet;
const char *uiEvoLine(char *buf, size_t n, uint16_t *col);

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

// A banked creature as it would arrive: valid, cared for, its own ID.
static PartyMon banked(int16_t dex, uint8_t level, uint32_t serial) {
  PartyMon m;
  m.dex = dex;
  m.level = level;
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
  m.stateVersion = 1;
  m.fullness = m.joy = m.energy = 90;
  m.hygiene = 100;
  m.weight = 25;
  m.ageMinutes = (uint32_t)(level - 1) * MINUTES_PER_LEVEL;
  m.uid.mac[0] = 0xBB;
  m.uid.serial = serial;
  return m;
}
static void cared(Pet &p) { p.fullness = p.joy = p.energy = p.hygiene = 100; p.sleeping = false; }
static const char *card() {
  static char buf[32];
  uint16_t col;
  return uiEvoLine(buf, sizeof(buf), &col);
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the table
  ck(Pet::onlyByTrade(64) && Pet::onlyByTrade(93) && Pet::onlyByTrade(95),
     "Kadabra, Haunter and Onix evolve only by trade");
  ck(!Pet::onlyByTrade(61) && !Pet::onlyByTrade(79),
     "Poliwhirl and Slowpoke keep their level route (trade reaches their OTHER form)");
  ck(!Pet::onlyByTrade(349), "Feebas keeps its level route (the games also level it)");
  ck(!Pet::onlyByTrade(4) && !Pet::onlyByTrade(133), "an ordinary line and Eevee are untouched");
  ck(DEX_TBL[186].rarity == R_EVO && DEX_TBL[199].rarity == R_EVO && DEX_TBL[368].rarity == R_EVO,
     "Politoed, Slowking and Gorebyss are evolution-only now, so they never hatch");

  // --- 2. pure trade: a Kadabra that was never traded does not evolve, at any level
  {
    PartyMon k = banked(64, 100, 101);
    ck(pet.switchTo(k), "(a level 100 Kadabra is the live pet)");
    cared(pet);
    ck(pet.evoInfo().status == Pet::EVO_NEEDS_TRADE, "it is waiting for a trade, not a level");
    ck(!pet.canEvolveNow(), "and it cannot evolve");
    pet.evolve();
    ck(pet.speciesId == 64, "evolve() does nothing to it");
    ck(!strcmp(card(), T(S_EVO_TRADE)), "and the CARD says it evolves by trade");
  }

  // --- 3. marking on receipt
  {
    PartyMon k = banked(64, 20, 102);
    Pet::markTraded(k, 25);
    ck(k.flags & PMON_TRADE_EVO, "a Kadabra received for anything is marked");
    PartyMon p = banked(25, 20, 103);
    Pet::markTraded(p, 64);
    ck(!(p.flags & PMON_TRADE_EVO), "a Pikachu is not");
    PartyMon kb = banked(588, 20, 104);
    Pet::markTraded(kb, 25);
    ck(!(kb.flags & PMON_TRADE_EVO), "a Karrablast traded for a Pikachu is not");
    Pet::markTraded(kb, 616);
    ck(kb.flags & PMON_TRADE_EVO, "a Karrablast traded for a Shelmet is");
    Pet::markTraded(kb, 25);
    ck(!(kb.flags & PMON_TRADE_EVO), "and loses the mark if traded on for something else");
  }

  // --- 4. it evolves when RAISED, at any level, and both forms reach the Pokedex
  {
    PartyMon k = banked(64, 20, 105);
    Pet::markTraded(k, 25);
    // Forget both forms first, so the registrations below are this test's doing.
    pet.dexReg[(64 - 1) >> 3] &= (uint8_t)~(1 << ((64 - 1) & 7));
    pet.dexReg[(65 - 1) >> 3] &= (uint8_t)~(1 << ((65 - 1) & 7));
    ck(pet.switchTo(k), "(the traded Kadabra becomes the live pet)");
    ck(pet.isRegistered(64) && !pet.isRegistered(65), "raising it registers Kadabra first");
    cared(pet);
    Pet::EvoInfo e = pet.evoInfo();
    ck(e.status == Pet::EVO_READY && e.byTrade && e.target == 65,
       "a level 20 traded Kadabra is ready to become Alakazam");
    ck(!strcmp(card(), T(S_EVO_READY)), "and the card says so");
    pet.evolve();
    ck(pet.speciesId == 65 && pet.isRegistered(65), "it evolves, and Alakazam is registered too");
    ck(!pet.tradeEvo, "the mark is spent");
    ck(pet.uid.serial == 105, "it is still the same creature");
  }

  // --- 5. care still matters; a frozen companion never evolves
  {
    PartyMon k = banked(93, 30, 106);   // Haunter
    Pet::markTraded(k, 25);
    pet.switchTo(k);
    cared(pet);
    pet.hygiene = 10;
    ck(pet.evoInfo().status == Pet::EVO_BLOCKED && !pet.canEvolveNow(),
       "a neglected traded creature waits until it is cared for");
    ck(!strcmp(card(), T(S_EVO_BLOCKED)), "and the card says why");
    Pet frozen;
    frozen.reviveFrom(k);
    cared(frozen);
    ck(frozen.tradeEvo && !frozen.canEvolveNow(), "a revived (frozen) companion never evolves");
  }

  // --- 6. the branch species: trade picks the other form, levels still work
  {
    PartyMon pw = banked(61, 40, 107);   // Poliwhirl, past its level 36
    pet.switchTo(pw);
    cared(pet);
    ck(pet.evoInfo().target == 62 && pet.canEvolveNow(), "an untraded Poliwhirl still levels into Poliwrath");
    Pet::markTraded(pw, 25);
    pw.level = 10;
    pw.ageMinutes = 9 * MINUTES_PER_LEVEL;
    pet.switchTo(pw);
    cared(pet);
    ck(pet.evoInfo().target == 186 && pet.canEvolveNow(), "a traded one becomes Politoed, at any level");
  }

  // --- 7. Porygon2 needs a trade of its own
  {
    PartyMon pg = banked(137, 30, 108);
    Pet::markTraded(pg, 25);
    pet.switchTo(pg);
    cared(pet);
    pet.evolve();
    ck(pet.speciesId == 233, "(Porygon became Porygon2)");
    ck(pet.evoInfo().status == Pet::EVO_NEEDS_TRADE, "and Porygon2 then waits for ANOTHER trade");
  }

  // --- 8. Clamperl: two targets, decided by the creature, not by the moment
  {
    CreatureUid a, b;
    a.serial = 200;
    b.serial = 201;
    int16_t ta = Pet::tradeTarget(366, a, 25), tb = Pet::tradeTarget(366, b, 25);
    ck((ta == 367 && tb == 368) || (ta == 368 && tb == 367), "two Clamperl can become Huntail or Gorebyss");
    ck(Pet::tradeTarget(366, a, 25) == ta, "and the same one always becomes the same");
  }

  // --- 9. the mark survives banking, a reload and the party
  {
    PartyMon k = banked(67, 25, 109);   // Machoke
    Pet::markTraded(k, 25);
    pet.switchTo(k);
    pet.saveNow();
    Pet reread;
    reread.begin();
    ck(reread.tradeEvo && reread.speciesId == 67, "the live pet's mark survives a reload");
    PartyMon back = pet.toPartyMon();
    ck(back.flags & PMON_TRADE_EVO, "and is carried back into the party when banked");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
