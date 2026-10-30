#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

// The party: pets that finished their life and were kept, rather than being
// dissolved into a single Pokedex bit like every previous ending did.
//
// Only the two endings the player CHOOSES bank a pet -- farewell and release.
// A runaway does not: it is the game's one punishing outcome, and letting a
// neglected pet come back as a team member would take the sting out of it.
#define PARTY_SLOTS 6
// The box: storage beyond the six that fight. Deliberately a SEPARATE NVS key
// rather than a bigger party blob -- growing that blob would change its stride
// and the length-based migration in begin() cannot tell a stride change from a
// slot-count change, so an existing party would be read back misaligned. A new
// key is purely additive and cannot corrupt anything.
#define BOX_SLOTS 18
#define MOVE_SLOTS 4    // the same four every trainer gets in the real games

// Who a creature IS, independent of what it currently is. Species, level and
// even the nickname can all change (evolution, renaming) and two creatures can
// share every one of them, so none of that can name a creature -- which a
// trade has to be able to do.
//
//   mac     the board that first had it: its efuse MAC, burned in at the
//           factory and unique per chip. Same bytes linkNowPeerMac() reports
//           for that board, so a creature can be matched to a known rival.
//   salt    drawn at random on EVERY boot. The serial below lives in the save,
//           and the save can go BACKWARDS -- WIPE resets it, IMPORT restores an
//           older one, a torn checkpoint falls back to the previous slot -- and
//           a counter that rewinds hands out numbers creatures already traded
//           away still carry. Every one of those paths goes through a reboot,
//           so a fresh salt per boot keeps the new IDs apart from the old ones.
//   serial  this board's running count of creatures it has minted, from 1.
//           0 means "no ID yet": a record from before IDs existed, which
//           backfillUids() gives one at boot.
//
// It is an identity, NOT a credential: the MAC is public over the air and the
// firmware is open, so nothing is ever authorised by it.
struct CreatureUid {
  uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 };
  uint16_t salt = 0;
  uint32_t serial = 0;

  bool valid() const { return serial != 0; }
  bool operator==(const CreatureUid &o) const {
    return serial == o.serial && salt == o.salt && !memcmp(mac, o.mac, 6);
  }
  bool operator!=(const CreatureUid &o) const { return !(*this == o); }
};
static_assert(sizeof(CreatureUid) == 12, "CreatureUid is on-disk format");

// "AABBCCDDEEFF-SSSS-N", or "-" for a record with no ID. The MAC is in radio
// byte order, as the LAN screen and a router would show it -- NOT the order
// STATS prints `board=` in, which is the efuse word as one hex number and so
// reads byte-reversed. That label keys the installer's backup history, so it
// stays as it is.
inline void uidFormat(const CreatureUid &u, char *out, size_t n) {
  if (!u.valid()) { snprintf(out, n, "-"); return; }
  snprintf(out, n, "%02X%02X%02X%02X%02X%02X-%04X-%lu", u.mac[0], u.mac[1],
           u.mac[2], u.mac[3], u.mac[4], u.mac[5], u.salt, (unsigned long)u.serial);
}

// A stored creature -- one of the six that fight, or one in the box.
//
// It used to be frozen forever: banked at a level and never trained again. It
// still is WHILE STORED, but the record now carries the full care state as
// well, so swapping it in as the focused creature resumes exactly where it left
// off instead of handing back a blanked-out copy. Freezing is now a property of
// where the record lives, not of the record itself.
struct PartyMon {
  int16_t dex = 0;      // Pokedex number, 0 = empty slot
  uint16_t level = 1;   // cached from ageMinutes so lists need not recompute it
  uint16_t medals = 0;  // what it earned in life
  uint8_t ivAtk = 0, ivDef = 0, ivSpe = 0, ivHp = 0;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t shiny = 0;
  char nick[12] = "";
  // Moves travel with the creature. 0 = empty slot (MOVE_TBL[0] is the "-"
  // filler). Appended at the END of the struct on purpose -- Party::begin()
  // migrates older, shorter blobs by length, and that only works if nothing
  // that already exists moves. Everything below obeys the same rule.
  uint8_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };

  // ---- care state, appended in v2.10 --------------------------------------
  // 0 means "this record predates care state": its fields below are merely the
  // initialisers, not something the creature earned, so `level` is the only
  // truth about its age. The default is 0 rather than 1 precisely so that a
  // record the length migration only half-filled reports itself honestly --
  // the migration memcpy's oldStride bytes over a default-constructed record
  // and leaves the tail alone, so anything defaulting to "I have real data"
  // would be a lie the moment the struct grows again.
  uint8_t stateVersion = 0;
  uint8_t fullness = 80, joy = 80, energy = 80, hygiene = 100;
  uint8_t poops = 0, weight = 0, bond = 0;
  uint8_t berryKnown = 0, careMistakes = 0;
  uint8_t evoDeclinedLv = 0, lastLearnLevel = 0;
  // The real age. `level` stays as its cached mirror so every list, the battle
  // squad builder and the box screen keep reading one byte instead of dividing.
  uint32_t ageMinutes = 0;

  // ---- identity, appended in v3.31 ----------------------------------------
  // Zeroed by the length migration on every older record, which is exactly
  // CreatureUid's "no ID yet" -- backfillUids() then gives it one.
  CreatureUid uid;

  // ---- flags, appended in v3.32 --------------------------------------------
  // PMON_TRADE_EVO: received in a trade, and will evolve by trade the next
  // time it is the live pet (Pet::evoInfo()). Set by Pet::markTraded(), cleared
  // by the evolution. PMON_CHEATED (v3.60): a cheat changed it, for good.
  // The three reserved bytes keep the record 4-aligned. reserved[0] is
  // Meltan's walk (v3.83); reserved[1] the PP Ups (v3.92: two bits a move
  // SLOT, 0-3 used on it -- ppUpsAt()); reserved[2] is zero, and
  // tradeMonValid() refuses a record where it is not.
  uint8_t flags = 0;
  uint8_t reserved[3] = { 0, 0, 0 };

  bool empty() const { return dex < 1; }
  bool hasCareState() const { return stateVersion >= 1; }
};
// Pinned so growing the record is a decision, not an accident: CLAUDE.md
// § "sizeof(PartyMon) is load-bearing in four places", and the debugger's
// tools/debugger/tpsave.py decodes this layout by hand. Append, then move both.
static_assert(sizeof(PartyMon) == 64, "PartyMon layout changed -- see the comment above");
#define PMON_TRADE_EVO 0x01
#define PMON_CHEATED   0x02   // a cheat changed this creature (cheats.h); never cleared

class Party {
public:
  PartyMon slots[PARTY_SLOTS];
  PartyMon box[BOX_SLOTS];

  void begin();                 // load from NVS
  // Bill's PC holds the box since v3.61; box[] is now only what did not fit it
  // on migration (more than six of a species), under its own key.
  void loadBox(const char *key);
  bool boxKeyStored(const char *key) { return prefs.isKey(key); }
  uint8_t count() const;
  bool isFull() const { return count() >= PARTY_SLOTS; }
  int firstFree() const;        // index of the first empty slot, -1 if full
  bool add(const PartyMon &m);  // into the first free slot; false if full
  void replaceAt(uint8_t i, const PartyMon &m);
  void releaseAt(uint8_t i);    // free a slot again
  bool save();                  // false if the write did not land
  uint8_t boxCount() const;
  int boxFirstFree() const;
  bool boxAdd(const PartyMon &m);     // into the first free box slot
  void boxReleaseAt(uint8_t i);
  bool boxSave();
  // Swaps a party slot with a box slot. Either may be empty, so this doubles as
  // deposit and withdraw rather than needing three separate operations.
  void swapPartyBox(uint8_t partyIdx, uint8_t boxIdx);

  // combat stats of a party member, same formula as the live pet's
  uint16_t atkOf(const PartyMon &m) const;
  uint16_t defOf(const PartyMon &m) const;
  uint16_t speOf(const PartyMon &m) const;
  uint16_t vitOf(const PartyMon &m) const;
  uint16_t spaOf(const PartyMon &m) const;
  uint16_t spdOf(const PartyMon &m) const;

private:
  Preferences prefs;
  const char *boxKey = "box";
};

extern Party party;
