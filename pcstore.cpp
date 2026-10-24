#include "pcstore.h"
#include "pcfs.h"
#include "dex.h"
#include <string.h>
#include <stdio.h>

#define PC_GROUPS ((DEX_COUNT + PC_GROUP_SPECIES - 1) / PC_GROUP_SPECIES)
#define PC_GROUP_SLOTS (PC_GROUP_SPECIES * PC_PER_SPECIES)
#define PC_MAGIC 0x31435042u   // "BPC1"
#define PC_VERSION 1
static const char *const PC_MARKER = "/pc_migrated";

struct PcHeader {
  uint32_t magic;
  uint8_t version, group;
  uint16_t stride;      // sizeof(PartyMon) when written
  uint32_t gen;
  uint16_t slots;       // PC_GROUP_SLOTS when written
  uint16_t pad;
};
static_assert(sizeof(PcHeader) == 16, "the PC's record header is on disk");
static const size_t PC_REC = sizeof(PcHeader) + PC_GROUP_SLOTS * sizeof(PartyMon) + 2;

static bool avail = false;
static uint8_t counts[DEX_COUNT + 1];
static uint32_t gens[PC_GROUPS];     // newest valid generation per group; 0 = none
static int openGroup = -1;
static PartyMon cache[PC_GROUP_SLOTS];
static uint8_t io[PC_REC];

static uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)p[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

// Generation parity names the file, so the one being written is never the one
// that holds the newest good copy.
static void pathFor(uint8_t g, uint32_t gen, char *out) {
  snprintf(out, 20, "/pc_g%02u%c.bin", (unsigned)g, (gen & 1) ? 'a' : 'b');
}

static bool groupOk(int16_t dex) { return dex >= 1 && dex <= DEX_COUNT; }
static uint8_t groupOf(int16_t dex) { return (uint8_t)((dex - 1) / PC_GROUP_SPECIES); }
static int slotOf(int16_t dex, uint8_t i) { return ((dex - 1) % PC_GROUP_SPECIES) * PC_PER_SPECIES + i; }

// One file into io, checked. Its generation, or 0 if absent or not sound.
static uint32_t readRec(uint8_t g, uint32_t parity) {
  char path[20];
  pathFor(g, parity, path);
  long n = pcfsRead(path, io, sizeof(io));
  if (n < (long)(sizeof(PcHeader) + 2)) return 0;
  PcHeader h;
  memcpy(&h, io, sizeof(h));
  if (h.magic != PC_MAGIC || h.group != g || !h.stride || !h.gen || (h.gen & 1) != (parity & 1)) return 0;
  if ((size_t)n != sizeof(PcHeader) + (size_t)h.slots * h.stride + 2) return 0;
  uint16_t want = (uint16_t)(io[n - 2] | io[n - 1] << 8);
  if (crc16(io, (size_t)n - 2) != want) return 0;
  return h.gen;
}

// io (already checked) into `out`, by its stored stride: a shorter record lands
// in the front of a fresh one, the same prefix rule as the party.
static void unpack(PartyMon *out) {
  PcHeader h;
  memcpy(&h, io, sizeof(h));
  for (int k = 0; k < PC_GROUP_SLOTS; k++) {
    out[k] = PartyMon();
    if (k >= h.slots) continue;
    size_t take = h.stride < sizeof(PartyMon) ? h.stride : sizeof(PartyMon);
    memcpy(&out[k], io + sizeof(PcHeader) + (size_t)k * h.stride, take);
  }
}

static void countFrom(uint8_t g, const PartyMon *slots) {
  for (int s = 0; s < PC_GROUP_SPECIES; s++) {
    int dex = g * PC_GROUP_SPECIES + s + 1;
    if (dex > DEX_COUNT) break;
    uint8_t n = 0;
    for (int i = 0; i < PC_PER_SPECIES; i++) if (!slots[s * PC_PER_SPECIES + i].empty()) n++;
    counts[dex] = n;
  }
}

static bool loadGroup(uint8_t g) {
  if (openGroup == g) return true;
  if (!gens[g]) {
    for (auto &m : cache) m = PartyMon();
  } else {
    if (readRec(g, gens[g]) != gens[g]) return false;   // it read at boot; now it does not
    unpack(cache);
  }
  openGroup = g;
  return true;
}

// The open group, written as the next generation and read back. On failure
// the RAM copy is dropped, so nothing claims what the flash does not hold.
static bool writeGroup(uint8_t g) {
  uint32_t gen = gens[g] + 1;
  PcHeader h = { PC_MAGIC, PC_VERSION, g, (uint16_t)sizeof(PartyMon), gen, PC_GROUP_SLOTS, 0 };
  memcpy(io, &h, sizeof(h));
  memcpy(io + sizeof(h), cache, sizeof(cache));
  uint16_t c = crc16(io, PC_REC - 2);
  io[PC_REC - 2] = (uint8_t)(c & 0xFF);
  io[PC_REC - 1] = (uint8_t)(c >> 8);
  char path[20];
  pathFor(g, gen, path);
  bool ok = pcfsWrite(path, io, PC_REC) && readRec(g, gen) == gen;
  if (!ok) { openGroup = -1; return false; }
  gens[g] = gen;
  countFrom(g, cache);
  return true;
}

bool pcBegin() {
  memset(counts, 0, sizeof(counts));
  memset(gens, 0, sizeof(gens));
  openGroup = -1;
  PcfsMount m = pcfsMount();
  avail = m != PCFS_FAILED;
  if (!avail) return false;
  for (uint8_t g = 0; g < PC_GROUPS; g++) {
    uint32_t ga = readRec(g, 1);
    PartyMon *tmp = cache;                 // scratch: nothing is open yet
    uint8_t ca[PC_GROUP_SPECIES] = { 0 };
    if (ga) { unpack(tmp); countFrom(g, tmp); for (int s = 0; s < PC_GROUP_SPECIES && g * 16 + s < DEX_COUNT; s++) ca[s] = counts[g * 16 + s + 1]; }
    uint32_t gb = readRec(g, 0);
    if (gb > ga) { unpack(tmp); countFrom(g, tmp); gens[g] = gb; }
    else if (ga) {
      for (int s = 0; s < PC_GROUP_SPECIES && g * 16 + s < DEX_COUNT; s++) counts[g * 16 + s + 1] = ca[s];
      gens[g] = ga;
    } else {
      for (int s = 0; s < PC_GROUP_SPECIES && g * 16 + s < DEX_COUNT; s++) counts[g * 16 + s + 1] = 0;
    }
  }
  openGroup = -1;
  return true;
}

bool pcAvailable() { return avail; }
uint8_t pcCount(int16_t dex) { return avail && groupOk(dex) ? counts[dex] : 0; }

uint16_t pcTotal() {
  uint16_t n = 0;
  for (int d = 1; d <= DEX_COUNT; d++) n += counts[d];
  return n;
}
uint16_t pcSpeciesHeld() {
  uint16_t n = 0;
  for (int d = 1; d <= DEX_COUNT; d++) n += counts[d] ? 1 : 0;
  return n;
}
int16_t pcSpeciesAt(uint16_t k) {
  for (int d = 1; d <= DEX_COUNT; d++)
    if (counts[d] && k-- == 0) return (int16_t)d;
  return 0;
}

bool pcGet(int16_t dex, uint8_t i, PartyMon &out) {
  if (!avail || !groupOk(dex) || i >= PC_PER_SPECIES || !counts[dex]) return false;
  if (!loadGroup(groupOf(dex))) return false;
  out = cache[slotOf(dex, i)];
  return !out.empty();
}

int pcPut(const PartyMon &m) {
  if (!avail || m.empty() || !groupOk(m.dex)) return PC_FAILED;
  if (counts[m.dex] >= PC_PER_SPECIES) return PC_FULL;
  uint8_t g = groupOf(m.dex);
  if (!loadGroup(g)) return PC_FAILED;
  for (uint8_t i = 0; i < PC_PER_SPECIES; i++) {
    PartyMon &s = cache[slotOf(m.dex, i)];
    if (!s.empty()) continue;
    s = m;
    return writeGroup(g) ? i : PC_FAILED;
  }
  return PC_FULL;
}

bool pcTake(int16_t dex, uint8_t i, PartyMon *out) {
  if (!avail || !groupOk(dex) || i >= PC_PER_SPECIES) return false;
  uint8_t g = groupOf(dex);
  if (!loadGroup(g)) return false;
  PartyMon &s = cache[slotOf(dex, i)];
  if (s.empty()) return false;
  PartyMon was = s;
  s = PartyMon();
  if (!writeGroup(g)) return false;
  if (out) *out = was;
  return true;
}

bool pcReplace(int16_t dex, uint8_t i, const PartyMon &m) {
  if (!avail || !groupOk(dex) || i >= PC_PER_SPECIES || m.dex != dex) return false;
  uint8_t g = groupOf(dex);
  if (!loadGroup(g)) return false;
  cache[slotOf(dex, i)] = m;
  return writeGroup(g);
}

bool pcFind(const CreatureUid &u, int16_t *dex, uint8_t *i) {
  if (!avail || !u.valid()) return false;
  for (uint8_t g = 0; g < PC_GROUPS; g++) {
    bool any = false;
    for (int s = 0; s < PC_GROUP_SPECIES && g * 16 + s < DEX_COUNT; s++) any |= counts[g * 16 + s + 1] != 0;
    if (!any || !loadGroup(g)) continue;
    for (int k = 0; k < PC_GROUP_SLOTS; k++) {
      if (cache[k].empty() || !(cache[k].uid == u)) continue;
      if (dex) *dex = (int16_t)(g * PC_GROUP_SPECIES + k / PC_PER_SPECIES + 1);
      if (i) *i = (uint8_t)(k % PC_PER_SPECIES);
      return true;
    }
  }
  return false;
}

void pcForEach(void (*fn)(const PartyMon &m, void *ctx), void *ctx) {
  if (!avail) return;
  for (uint8_t g = 0; g < PC_GROUPS; g++) {
    if (!gens[g] || !loadGroup(g)) continue;
    for (auto &m : cache) if (!m.empty()) fn(m, ctx);
  }
}

bool pcMigrated() {
  uint8_t b;
  return avail && pcfsRead(PC_MARKER, &b, 1) == 1;
}
bool pcSetMigrated() {
  const uint8_t one = 1;
  return avail && pcfsWrite(PC_MARKER, &one, 1);
}

bool pcWipe() {
  if (!avail) return false;
  bool ok = true;
  char path[20];
  for (uint8_t g = 0; g < PC_GROUPS; g++)
    for (uint32_t p = 0; p < 2; p++) { pathFor(g, p, path); ok &= pcfsRemove(path); }
  ok &= pcfsRemove(PC_MARKER);
  memset(counts, 0, sizeof(counts));
  memset(gens, 0, sizeof(gens));
  openGroup = -1;
  return ok;
}

static void recAt(const uint8_t *recs, uint16_t k, uint16_t stride, PartyMon &out) {
  out = PartyMon();
  memcpy(&out, recs + (size_t)k * stride, stride < sizeof(PartyMon) ? stride : sizeof(PartyMon));
}

bool pcRestoreValid(const uint8_t *recs, uint16_t n, uint16_t stride) {
  if (!stride || stride > 1024) return false;
  static uint8_t seen[DEX_COUNT + 1];
  memset(seen, 0, sizeof(seen));
  for (uint16_t k = 0; k < n; k++) {
    PartyMon m;
    recAt(recs, k, stride, m);
    if (!groupOk(m.dex) || ++seen[m.dex] > PC_PER_SPECIES) return false;
  }
  return true;
}

bool pcRestore(const uint8_t *recs, uint16_t n, uint16_t stride) {
  if (!avail || !pcRestoreValid(recs, n, stride) || !pcWipe()) return false;
  bool ok = true;
  for (uint8_t g = 0; g < PC_GROUPS; g++) {
    for (auto &m : cache) m = PartyMon();
    bool any = false;
    for (uint16_t k = 0; k < n; k++) {
      PartyMon m;
      recAt(recs, k, stride, m);
      if (groupOf(m.dex) != g) continue;
      for (uint8_t i = 0; i < PC_PER_SPECIES; i++)
        if (cache[slotOf(m.dex, i)].empty()) { cache[slotOf(m.dex, i)] = m; break; }
      any = true;
    }
    if (!any) continue;
    openGroup = g;
    ok &= writeGroup(g);
  }
  return ok && pcSetMigrated();
}
