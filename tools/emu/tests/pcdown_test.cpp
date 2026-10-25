// Bill's PC on a real board (v3.66). The first boards to run v3.61+ had another
// firmware's leftovers on the PC's partition: it would not mount, it was not
// blank, so it was never formatted -- the PC read unavailable forever, and a
// farewell with a full party offered only a release, though the old box had
// room. Now a board that has never had a working PC formats leftovers, one that
// has keeps refusing, and while the PC is down the old box stands in for it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "pcstore.h"
#include "pcfs_host.h"
#include <cstdio>
uint32_t g_seed = 66; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void loop();
void onTap(int16_t x, int16_t y);
bool pcEverHeld();
void pcMigrateBox();
bool storeNewCreature(const PartyMon &m);
bool roomFor(int16_t dex);
bool pcDeposit(uint8_t partyIdx);
bool partyHubAt(int16_t x, int16_t y);
extern Pet pet;
extern bool partyOpen, partyPick, boxOpen, pcOpen;
extern uint8_t boxSwapFrom;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint32_t serial = 500;
static PartyMon mon(int16_t dex) {
  PartyMon m; m.dex = dex; m.level = 20; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 18;
  m.stateVersion = 1; m.uid.mac[0] = 0xAB; m.uid.serial = serial++; m.uid.salt = 9;
  return m;
}
static bool inOldBox(const PartyMon &m) {
  for (auto &b : party.box) if (!b.empty() && b.uid == m.uid) return true;
  return false;
}

int main() {
  // --- 1. a board whose partition holds another firmware's leftovers, on a
  //        save that has never had a PC: setup() formats it and remembers
  pcfsHostReset(PCFS_HOST_BROKEN);
  pcfsHostFiles()["/leftover.bin"] = { 1, 2, 3 };
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  ck(pcAvailable() && pcfsHostFormats() == 1 && pcTotal() == 0,
     "leftovers on a board that never had a PC are formatted, and the PC works");
  ck(pcEverHeld(), "and the board remembers it has had a working PC");

  // --- 2. once it has held one, a mount that fails is never formatted over
  PartyMon kept = mon(25);
  ck(pcPut(kept) >= 0, "(a creature in the PC)");
  pcfsHostSetState(PCFS_HOST_BROKEN);
  ck(!pcBegin(!pcEverHeld()) && !pcAvailable() && pcfsHostFormats() == 1,
     "a board that has held a PC refuses a bad mount rather than formatting");
  pcfsHostSetState(PCFS_HOST_OK);
  int16_t d; uint8_t s;
  ck(pcBegin(!pcEverHeld()) && pcFind(kept.uid, &d, &s), "and the collection is still there when it mounts again");

  // --- 3. the PC down, party full: the old box stands in
  pcfsHostSetState(PCFS_HOST_BROKEN);
  pcBegin(!pcEverHeld());
  pcMigrateBox();
  ck(!pcAvailable(), "(the PC is down)");
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = mon(16);
  party.save();
  for (auto &b : party.box) b = PartyMon();
  party.boxSave();
  ck(roomFor(19), "room for a newcomer: the old box has space");
  PartyMon caught = mon(19);
  ck(storeNewCreature(caught) && inOldBox(caught), "a catch goes to the old box");

  // the farewell that started this: through loop(), with no chooser
  PartyMon bye = mon(235);   // SMEARGLE
  pet.setEnded(bye, CER_FAREWELL);
  loop();
  ck(pet.endedKind == CER_NONE && !partyPick && inOldBox(bye),
     "a farewell with a full party lands in the old box, not a release chooser");

  // the hub opens the old box, and an armed slot deposits into it
  partyOpen = true; partyPick = false; boxOpen = false; pcOpen = false;
  int16_t hx = 0, hy = 0;                      // the hub, asked of the firmware
  for (int16_t y = 0; y < 466 && !hx; y += 4)
    for (int16_t x = 0; x < 466; x += 4) if (partyHubAt(x, y)) { hx = x + 20; hy = y + 20; break; }
  onTap(hx, hy);
  ck(boxOpen && !pcOpen, "the party's hub opens the old box while the PC is down");
  boxOpen = false;
  PartyMon out = party.slots[2];
  ck(pcDeposit(2) && party.slots[2].empty() && inOldBox(out), "and a deposit goes into it");

  // full box too: no room, and the chooser is the honest answer
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = mon(16);
  for (auto &b : party.box) b = mon(16);
  ck(!roomFor(19) && !storeNewCreature(mon(19)), "with the old box full too there is no room");

  // --- 4. a box that had already moved: never read migration day's copy
  for (auto &b : party.box) b = PartyMon();
  party.boxSave();
  Preferences p; p.begin("tamapoke", false);
  PartyMon stale[BOX_SLOTS], left[BOX_SLOTS];
  stale[0] = mon(1); left[0] = mon(4);
  p.putBytes("box", stale, sizeof(stale));
  p.putBytes("boxl", left, sizeof(left));
  p.end();
  party.begin();
  pcMigrateBox();
  ck(!pcAvailable() && inOldBox(left[0]) && !inOldBox(stale[0]),
     "with the PC down after it took the box, the leftovers are read, not the stale box");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
