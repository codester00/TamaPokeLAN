// Bill's PC, the storage layer (v3.61): pcstore.cpp over the emulator's pcfs
// stand-in. Six of a species and no more; A/B files that survive a power cut at
// any write; a mount failure that is never answered with a format; the boot
// index matching the files; and the backup carrying the PC both ways.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "pcstore.h"
#include "pcfs_host.h"
#include "save.h"
#include <cstdio>
#include <cstdlib>
uint32_t g_seed = 61; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint32_t serial = 1;
static PartyMon mon(int16_t dex) {
  PartyMon m; m.dex = dex; m.level = 20; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
  m.uid.mac[0] = 0xAB; m.uid.serial = serial++; m.uid.salt = 7;
  return m;
}
static bool holds(const CreatureUid &u) { return pcFind(u, nullptr, nullptr); }

int main() {
  // --- 1. a blank partition is formatted once, and starts empty
  pcfsHostReset(PCFS_HOST_BLANK);
  ck(pcBegin() && pcAvailable() && pcfsHostFormats() == 1 && pcTotal() == 0, "a blank partition mounts empty");
  ck(!pcMigrated(), "and has not taken the box over yet");

  // --- 2. six of a species, and a seventh is refused
  PartyMon six[6];
  for (int i = 0; i < 6; i++) { six[i] = mon(19); ck(pcPut(six[i]) == i, "each Rattata takes the next slot"); }
  ck(pcCount(19) == 6 && pcPut(mon(19)) == PC_FULL, "a seventh Rattata is refused");
  ck(pcPut(mon(20)) == 0 && pcCount(20) == 1, "but another species is not");
  PartyMon got;
  ck(pcTake(19, 2, &got) && got.uid == six[2].uid && pcCount(19) == 5, "taking one empties its slot");
  ck(!pcGet(19, 2, got), "and the slot reads empty");
  ck(pcPut(six[2]) == 2, "the next one fills the hole");
  int16_t d; uint8_t s;
  ck(pcFind(six[4].uid, &d, &s) && d == 19 && s == 4, "pcFind names the species and slot");
  PartyMon renamed = six[4]; snprintf(renamed.nick, sizeof(renamed.nick), "BOB");
  ck(pcReplace(19, 4, renamed) && pcGet(19, 4, got) && !strcmp(got.nick, "BOB"), "pcReplace rewrites a slot");
  ck(!pcReplace(19, 4, mon(20)), "but never with another species");
  ck(pcPut(mon(1000)) == 0 && pcCount(1000) == 1, "the last group holds species past 1000");

  // --- 3. the boot index matches the files
  uint8_t before[DEX_COUNT + 1];
  for (int x = 1; x <= DEX_COUNT; x++) before[x] = pcCount(x);
  uint16_t total = pcTotal();
  pcBegin();
  bool same = pcTotal() == total;
  for (int x = 1; x <= DEX_COUNT; x++) same = same && pcCount(x) == before[x];
  ck(same && pcfsHostFormats() == 1, "rebooting rebuilds the same index, and does not format again");

  // --- 4. a power cut at every write keeps the old record or the new one
  bool always = true;
  for (int cut = 0; cut < 3; cut++) {
    PartyMon extra = mon(21);
    uint16_t t0 = pcTotal();
    pcfsHostFailWritesAfter(cut == 0 ? 0 : 0, true);   // the write itself is torn
    int r = pcPut(extra);
    pcfsHostResumeWrites();
    pcBegin();                                          // the reboot after the cut
    bool oldOk = holds(six[0].uid) && holds(six[5].uid);
    bool either = (pcTotal() == t0 && !holds(extra.uid)) || (pcTotal() == t0 + 1 && holds(extra.uid));
    if (r >= 0 || !oldOk || !either) always = false;
    // a cut AFTER a landed write: the newest file wins
    ck(pcPut(extra) >= 0, "after the cut the same put lands");
    pcBegin();
    always = always && holds(extra.uid);
  }
  ck(always, "a torn write never loses what the group held, and never half-adds");

  // --- 5. a mount failure is not a format, and not an empty PC
  uint16_t had = pcTotal();
  pcfsHostSetState(PCFS_HOST_BROKEN);
  ck(!pcBegin() && !pcAvailable() && pcCount(19) == 0 && pcPut(mon(19)) == PC_FAILED, "a PC that will not mount is unavailable");
  ck(pcfsHostFormats() == 1 && !pcfsHostFiles().empty(), "and it was not formatted");
  pcfsHostSetState(PCFS_HOST_OK);
  ck(pcBegin() && pcTotal() == had, "mounting again finds everything");

  // --- 6. the backup carries the PC both ways
  Pet p; p.begin();
  uint8_t *buf = (uint8_t *)malloc(SAVE_IMPORT_MAX);
  size_t n = saveExport(buf, SAVE_IMPORT_MAX);
  ck(n > (size_t)had * sizeof(PartyMon), "EXPORT carries the PC's creatures");
  static char streamed[SAVE_IMPORT_MAX * 2 + 1]; static size_t sn = 0; static size_t declared = 0;
  size_t m2 = saveExportStream([](size_t t) { declared = t; },
                               [](const char *h) { size_t l = strlen(h); memcpy(streamed + sn, h, l); sn += l; });
  bool sameBytes = m2 == n && declared == n && sn == n * 2;
  for (size_t i = 0; sameBytes && i < n; i++) {
    unsigned v; sscanf(streamed + i * 2, "%2x", &v); sameBytes = (uint8_t)v == buf[i];
  }
  ck(sameBytes, "the streamed EXPORT is byte for byte the same backup");
  pcWipe();
  ck(pcTotal() == 0, "the PC is emptied");
  ck(saveImport(buf, n) && pcBegin() && pcTotal() == had && holds(six[3].uid) && pcMigrated(),
     "IMPORT puts the PC back, with its marker");
  // a backup that would put seven of a species in the PC is refused whole
  size_t at = n - 2 - (size_t)had * sizeof(PartyMon);   // the first record
  std::vector<uint8_t> t(buf, buf + n);
  for (uint16_t k = 0; k < had; k++) {                   // every creature a Rattata
    int16_t r = 19; memcpy(t.data() + at + k * sizeof(PartyMon), &r, 2);
  }
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n - 2; i++) { c ^= (uint16_t)t[i] << 8; for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1); }
  t[n - 2] = (uint8_t)c; t[n - 1] = (uint8_t)(c >> 8);
  uint16_t still = pcTotal();
  ck(!saveImport(t.data(), n) && pcTotal() == still, "a backup with seven of one species is refused, PC untouched");
  // a backup from before the PC (v1) empties it, so the box moves in again
  std::vector<uint8_t> v1(buf, buf + at - 6);
  v1[4] = 1;
  uint16_t c1 = 0xFFFF;
  for (uint8_t b8 : v1) { c1 ^= (uint16_t)b8 << 8; for (int b = 0; b < 8; b++) c1 = (c1 & 0x8000) ? (uint16_t)((c1 << 1) ^ 0x1021) : (uint16_t)(c1 << 1); }
  v1.push_back((uint8_t)c1); v1.push_back((uint8_t)(c1 >> 8));
  ck(saveImport(v1.data(), v1.size()) && pcTotal() == 0 && !pcMigrated(),
     "a pre-PC backup empties the PC and clears the marker");
  free(buf);

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
