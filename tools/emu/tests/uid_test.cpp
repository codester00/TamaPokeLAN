// Creature IDs (CreatureUid, party.h): every creature gets one when it comes
// into being, keeps it for life, and no two share one.
//
// Drives the REAL sketch -- setup(), the console, wildToPartyMon() and
// backfillUids() -- because the rule that matters is "every path that MAKES a
// creature mints an ID", and a test that only called mintUid() would prove the
// minting while every caller ignored it (CLAUDE.md § 1: test the caller).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

uint32_t g_seed = 0x51D;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }

static std::vector<std::string> gIn;
static size_t gAt = 0;
int FakeSerial::available() { return gAt < gIn.size() ? 1 : 0; }
String FakeSerial::readStringUntil(char) {
  if (gAt >= gIn.size()) return String("");
  return String(gIn[gAt++].c_str());
}

void setup();
void handleSerial();
int backfillUids();
PartyMon wildToPartyMon();
extern Pet pet;
extern Party party;
extern int16_t wildDex;
extern uint8_t wildLvl;

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}
static void console(const char *line) {
  gIn = { line };
  gAt = 0;
  while (gAt < gIn.size()) handleSerial();
}

// Every creature this save holds, wherever it lives.
static std::vector<CreatureUid> everyone() {
  std::vector<CreatureUid> v;
  if (!pet.isEgg()) v.push_back(pet.uid);
  if (!pet.endedMon.empty()) v.push_back(pet.endedMon.uid);
  for (auto &m : party.slots) if (!m.empty()) v.push_back(m.uid);
  for (auto &m : party.box) if (!m.empty()) v.push_back(m.uid);
  return v;
}
static bool allValidAndDistinct(const std::vector<CreatureUid> &v) {
  for (size_t i = 0; i < v.size(); i++) {
    if (!v[i].valid()) return false;
    for (size_t j = i + 1; j < v.size(); j++)
      if (v[i] == v[j]) return false;
  }
  return true;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);

  // --- 1. a hatch mints, from this board
  {
    ck(pet.uid.valid(), "a hatched creature has an ID");
    uint64_t mac = ESP.getEfuseMac();
    bool macOk = true;
    for (int i = 0; i < 6; i++) macOk &= pet.uid.mac[i] == (uint8_t)(mac >> (8 * i));
    ck(macOk, "and it names this board's MAC, in radio byte order");
    ck(pet.uid.salt == Pet::uidSalt(), "and this boot's salt");
    ck(pet.uid.serial == pet.uidSerial, "and the latest serial");
  }

  // --- 2. evolution keeps the ID: it is the same creature
  {
    CreatureUid before = pet.uid;
    pet.ageMinutes = 40 * MINUTES_PER_LEVEL;
    pet.fullness = pet.joy = pet.energy = pet.hygiene = 100;
    pet.careMistakes = 0;
    bool could = pet.canEvolveNow();
    pet.evolve();
    ck(could && pet.speciesId == 5, "(Charmander evolved, so the next check means something)");
    ck(pet.uid == before, "evolution keeps the ID");
  }

  // --- 3. banking and swapping carry it, both ways
  {
    CreatureUid live = pet.uid;
    PartyMon banked = pet.toPartyMon();
    ck(banked.uid == live, "banking a creature carries its ID");
    Pet other;
    other.switchTo(banked);
    ck(other.uid == live, "switching it back in carries its ID");
    Pet frozen;
    frozen.reviveFrom(banked);
    ck(frozen.uid == live, "reviving it carries its ID");
  }

  // --- 4. every other way a creature comes into being mints a NEW one
  {
    console("PARTY 25");
    console("PARTY 133");
    wildDex = 16; wildLvl = 5;
    PartyMon caught = wildToPartyMon();
    ck(caught.uid.valid(), "a wild catch has an ID");
    ck(party.add(caught), "(the catch went into the party)");
    // The catch must be minted from the LIVE pet's counter, not the throwaway
    // Pet the foe was built in -- whose counter is always at 1, so two catches
    // in a row would share serial 1. Two of them prove it.
    wildDex = 19;
    PartyMon caught2 = wildToPartyMon();
    ck(caught2.uid != caught.uid, "two wild catches never share an ID");
    ck(party.add(caught2), "(the second catch went into the party)");
    console("EGG 7");   // a fresh hatch on the live pet
    std::vector<CreatureUid> all = everyone();
    printf("      %u creatures held\n", (unsigned)all.size());
    ck(all.size() == 5, "(the live pet plus four banked)");
    ck(allValidAndDistinct(all), "every creature held has its own ID");
  }

  // --- 5. IDs and the counter survive a reload
  {
    pet.saveNow();
    party.save();
    CreatureUid live = pet.uid;
    uint32_t serial = pet.uidSerial;
    CreatureUid slot0 = party.slots[0].uid;
    Pet reread;
    reread.begin();
    ck(reread.uid == live, "the live creature's ID survives a reload");
    ck(reread.uidSerial == serial, "and so does the counter");
    Party rp;
    rp.begin();
    ck(rp.slots[0].uid == slot0, "a party member's ID survives a reload");
  }

  // --- 6. a save from before IDs: backfilled once, uniquely, and kept
  {
    // Rewrite the party as v3.30 wrote it: 48-byte records, no ID.
    static_assert(offsetof(PartyMon, uid) == 48, "the pre-ID record is the first 48 bytes");
    uint8_t old[48 * PARTY_SLOTS] = {};
    for (int i = 0; i < PARTY_SLOTS; i++) memcpy(old + 48 * i, &party.slots[i], 48);
    Preferences p;
    p.begin("tamapoke", false);
    p.putBytes("party", old, sizeof(old));
    p.end();
    party.begin();
    ck(party.slots[0].dex != 0 && !party.slots[0].uid.valid(),
       "an old party record loads with no ID");
    pet.uid = CreatureUid();   // and the live pet as a v2 checkpoint would leave it
    int given = backfillUids();
    printf("      backfill gave %d IDs\n", given);
    ck(given == 4 + 1, "the backfill gives every creature without one an ID");
    ck(allValidAndDistinct(everyone()), "and none of them collide with each other or the rest");
    ck(backfillUids() == 0, "a second boot gives out nothing (it only fills gaps)");
    CreatureUid slot0 = party.slots[0].uid;
    Party rp;
    rp.begin();
    ck(rp.slots[0].uid == slot0, "and what it gave is saved");
  }

  // --- 7. a rewound counter cannot re-issue an ID, thanks to the boot salt
  {
    uint32_t at = pet.uidSerial;
    CreatureUid first = pet.mintUid();
    pet.uidSerial = at;                 // what an IMPORT of an older backup does
    CreatureUid sameBoot = pet.mintUid();
    // The companion check: without a new boot the rewind DOES collide, so the
    // assertion below is testing the salt and not passing by luck.
    ck(sameBoot == first, "(a rewound counter on the same boot repeats an ID)");
    pet.uidSerial = at;
    Pet::uidNewBoot();                  // IMPORT, WIPE and a torn write all reboot
    CreatureUid nextBoot = pet.mintUid();
    ck(nextBoot.serial == first.serial && nextBoot != first,
       "after a reboot the same serial is a different ID");
  }

  // --- 8. an egg is not a creature
  {
    Pet egg;
    egg.begin();
    egg.dbgHatchAs(1, false);
    ck(egg.uid.valid(), "(hatched, it has one)");
    egg.newEgg();
    ck(!egg.uid.valid(), "a new egg has no ID until it hatches");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
