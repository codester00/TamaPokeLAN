// Bill's PC through the sketch (v3.61): the NVS box moving in on the first
// boot, once and only once, even when the power goes part-way; more than six
// of a species kept rather than dropped; ownedCount() counting the live pet;
// new creatures and the box screen going through the PC.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "pcstore.h"
#include "pcfs_host.h"
#include <cstdio>
#include <map>
uint32_t g_seed = 62; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void pcMigrateBox();
uint8_t ownedCount(int16_t dex);
bool storeNewCreature(const PartyMon &m);
uint16_t boxViewPages();
bool boxViewAt(uint16_t idx, PartyMon &out);
bool boxViewSwap(uint8_t partyIdx, uint16_t idx);
extern Pet pet;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint32_t serial = 100;
static PartyMon mon(int16_t dex) {
  PartyMon m; m.dex = dex; m.level = 12; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 18;
  m.stateVersion = 1; m.uid.mac[0] = 0xCD; m.uid.serial = serial++; m.uid.salt = 3;
  return m;
}

static PartyMon oldBox[BOX_SLOTS];
static void seedOldBox() {
  serial = 100;
  for (auto &m : oldBox) m = PartyMon();
  for (int i = 0; i < 7; i++) oldBox[i] = mon(19);          // seven Rattata
  oldBox[9] = mon(16); oldBox[12] = mon(25); oldBox[17] = mon(150);
  Preferences p; p.begin("tamapoke", false);
  p.putBytes("box", oldBox, sizeof(oldBox));
  p.remove("boxl");
  p.end();
}
static void reboot() { party.begin(); pcBegin(); pcMigrateBox(); }
// every creature of the old box exactly once, across the PC and the leftovers
static bool eachOnce() {
  for (auto &o : oldBox) {
    if (o.empty()) continue;
    int n = 0;
    int16_t d; uint8_t s;
    if (pcFind(o.uid, &d, &s)) n++;
    for (auto &l : party.box) if (!l.empty() && l.uid == o.uid) n++;
    if (n != 1) return false;
  }
  return true;
}

int main() {
  pcfsHostReset(PCFS_HOST_BLANK);
  seedOldBox();
  setup();
  ck(pcAvailable() && pcMigrated(), "the first boot moves the box into the PC");
  ck(pcCount(19) == 6 && pcCount(16) == 1 && pcCount(25) == 1 && pcCount(150) == 1, "six Rattata and the rest");
  ck(party.boxCount() == 1 && party.box[0].dex == 19, "the seventh Rattata waits in the leftovers");
  ck(eachOnce(), "every creature is in exactly one place");
  {
    Preferences p; p.begin("tamapoke", true);
    PartyMon still[BOX_SLOTS];
    p.getBytes("box", still, sizeof(still));
    ck(!memcmp(still, oldBox, sizeof(still)), "the old box key is left exactly as it was");
  }
  reboot();
  ck(pcTotal() == 9 && party.boxCount() == 1 && eachOnce(), "a second boot moves nothing twice");

  // a power cut part-way through the move: the next boot finishes it
  for (int cut = 0; cut < 4; cut++) {
    pcfsHostReset(PCFS_HOST_BLANK);
    seedOldBox();
    party.begin();
    pcBegin();
    pcfsHostFailWritesAfter(cut, true);
    pcMigrateBox();
    pcfsHostResumeWrites();
    reboot();
    char w[64]; snprintf(w, sizeof w, "a cut after %d PC writes: nothing lost, nothing doubled", cut);
    ck(pcMigrated() && pcTotal() == 9 && party.boxCount() == 1 && eachOnce(), w);
  }

  // ownedCount: the PC, the party and the live pet
  pet.dbgHatchAs(19, false);
  for (auto &m : party.slots) m = PartyMon();
  party.slots[0] = mon(19);
  party.save();
  ck(ownedCount(19) == 6 + 1 + 1 + 1, "Rattata owned: six in the PC, one left over, one in the party, the live one");
  ck(ownedCount(1) == 0, "and none of a species never caught");

  // a new creature: the party first, then the PC; none with neither
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = mon(10 + i);
  party.save();
  ck(storeNewCreature(mon(30)) && pcCount(30) == 1, "a full party sends a catch to the PC");
  ck(!storeNewCreature(mon(19)), "and a seventh Rattata has nowhere to go");

  // the box screen pages over the PC, one species a page
  uint16_t pages = boxViewPages();
  ck(pages == pcSpeciesHeld() + 1, "one page per species, then the leftovers");
  PartyMon v;
  ck(boxViewAt(0, v) && v.dex == 16, "page one is the lowest species held");
  PartyMon out = party.slots[2];
  ck(boxViewSwap(2, 0) && party.slots[2].dex == 16 && pcCount(16) == 0 && pcCount(out.dex) == 1,
     "an exchange sends the party creature to its own species, the PC one to the party");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
