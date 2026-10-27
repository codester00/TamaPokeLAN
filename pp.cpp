// PP book -- see pp.h for the rules and why it is keyed the way it is.
#include "pp.h"
#include <Preferences.h>
#include <string.h>
#include "nvsinfo.h"

static PpEntry gBook[PP_BOOK_CAP];
static uint8_t gCount = 0;     // gBook[0] is the most recently used
static bool gDirty = false;    // recorded since the last write

uint8_t ppMax(uint8_t move) {
  if (!move || move >= MOVE_COUNT) return 0;
  return MOVE_TBL[move].pp ? MOVE_TBL[move].pp : 1;
}

static int find(const CreatureUid &uid) {
  if (!uid.valid()) return -1;
  for (uint8_t i = 0; i < gCount; i++) if (gBook[i].uid == uid) return i;
  return -1;
}

static void save() {
  gDirty = false;
  Preferences p;
  p.begin("tamapoke", false);
  if (gCount) {
    size_t n = sizeof(PpEntry) * gCount;
    if (p.putBytes("pp", gBook, n) != n) logKeyFailure("pp", "pp");
  } else if (p.isKey("pp")) {
    p.remove("pp");
  }
  p.end();
}

void ppLoad() {
  gCount = 0;
  gDirty = false;
  Preferences p;
  p.begin("tamapoke", true);
  size_t n = p.isKey("pp") ? p.getBytesLength("pp") : 0;
  // A blob that is not whole entries, or too big, is not believed: everyone
  // starts full, the generous direction.
  if (n && n % sizeof(PpEntry) == 0 && n <= sizeof(gBook) &&
      p.getBytes("pp", gBook, n) == n)
    gCount = (uint8_t)(n / sizeof(PpEntry));
  p.end();
  // never trust a move index or a count off the disk
  for (uint8_t i = 0; i < gCount; i++)
    for (int s = 0; s < MOVE_SLOTS; s++) {
      if (gBook[i].move[s] >= MOVE_COUNT) gBook[i].move[s] = 0;
      if (gBook[i].used[s] > ppMax(gBook[i].move[s])) gBook[i].used[s] = ppMax(gBook[i].move[s]);
    }
}

uint8_t ppLeft(const CreatureUid &uid, uint8_t move) {
  uint8_t mx = ppMax(move);
  int i = find(uid);
  if (i < 0) return mx;
  for (int s = 0; s < MOVE_SLOTS; s++)
    if (gBook[i].move[s] == move)
      return gBook[i].used[s] >= mx ? 0 : (uint8_t)(mx - gBook[i].used[s]);
  return mx;
}

uint8_t ppLeftSlot(const CreatureUid &uid, uint8_t slot, uint8_t move) {
  int i = find(uid);
  if (i < 0 || slot >= MOVE_SLOTS || gBook[i].move[slot] != move) return ppLeft(uid, move);
  uint8_t mx = ppMax(move);
  return gBook[i].used[slot] >= mx ? 0 : (uint8_t)(mx - gBook[i].used[slot]);
}

void ppRecord(const CreatureUid &uid, const uint8_t moves[MOVE_SLOTS],
              const uint8_t left[MOVE_SLOTS]) {
  if (!uid.valid()) return;
  PpEntry e;
  e.uid = uid;
  bool any = false;
  for (int s = 0; s < MOVE_SLOTS; s++) {
    e.move[s] = moves[s] < MOVE_COUNT ? moves[s] : 0;
    uint8_t mx = ppMax(e.move[s]);
    e.used[s] = (e.move[s] && left[s] < mx) ? (uint8_t)(mx - left[s]) : 0;
    if (e.used[s]) any = true;
  }
  int i = find(uid);
  if (i >= 0) {
    if (!memcmp(&gBook[i], &e, sizeof(e)) && i == 0) return;   // nothing changed
    memmove(&gBook[1], &gBook[0], sizeof(PpEntry) * i);        // to the front
    gBook[0] = e;
    if (!any) {                                                // full again: drop
      memmove(&gBook[0], &gBook[1], sizeof(PpEntry) * (gCount - 1));
      gCount--;
    }
  } else {
    if (!any) return;
    if (gCount == PP_BOOK_CAP) gCount--;   // the least recently used gives way
    memmove(&gBook[1], &gBook[0], sizeof(PpEntry) * gCount);
    gBook[0] = e;
    gCount++;
  }
  gDirty = true;
}

bool ppDirty() { return gDirty; }
void ppFlush() { if (gDirty) save(); }

bool ppRefillAll() {
  if (!gCount) return false;
  gCount = 0;
  save();
  return true;
}

uint8_t ppEntryCount() { return gCount; }
