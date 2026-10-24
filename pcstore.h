// Bill's PC (v3.61): up to PC_PER_SPECIES of every species, on the board's
// spare flash (pcfs.h). THE one way in -- nothing else reads or writes the PC.
//
// The species are kept in groups of 16, one record per group: a header, 96
// PartyMon slots and a CRC. Each group alternates between two files, A and B,
// by generation, and the newest VALID one wins -- the petA/petB scheme, because
// FAT has no atomic rename over an existing file: a power cut mid-write leaves
// the other file whole. At boot every group is read once into a count index;
// only the group in use is held whole in RAM.
//
// Every write says whether it LANDED (the trade board needs that, as it does
// for NVS). A PC that would not mount reads as UNAVAILABLE, never as empty.
#pragma once
#include <stdint.h>
#include "party.h"

#define PC_PER_SPECIES 6
#define PC_GROUP_SPECIES 16
enum : int { PC_FULL = -1, PC_FAILED = -2 };

bool pcBegin();                        // mount and index; false = unavailable
bool pcAvailable();
uint8_t pcCount(int16_t dex);          // how many of the species it holds
uint16_t pcTotal();                    // creatures, all species
uint16_t pcSpeciesHeld();              // species with at least one
int16_t pcSpeciesAt(uint16_t k);       // the k-th of those, by dex; 0 if none
bool pcGet(int16_t dex, uint8_t i, PartyMon &out);    // slot i < PC_PER_SPECIES
int pcPut(const PartyMon &m);          // the slot it went to, or PC_FULL / PC_FAILED
bool pcTake(int16_t dex, uint8_t i, PartyMon *out);   // empties the slot
bool pcReplace(int16_t dex, uint8_t i, const PartyMon &m);   // same species only
bool pcFind(const CreatureUid &u, int16_t *dex, uint8_t *i);
// Every creature it holds, group by group (the backup).
void pcForEach(void (*fn)(const PartyMon &m, void *ctx), void *ctx);

// The one-time move of the old NVS box into the PC. The marker lives in the PC
// itself, so "Erase device" (which clears both) cannot leave one without the
// other.
bool pcMigrated();
bool pcSetMigrated();

// Everything gone, marker included: WIPE, and a restore.
bool pcWipe();
// A restore: wipe, then exactly these. `stride` is the record size they were
// written at (a shorter one reads into the front of a fresh record). Fails, and
// writes nothing, when any species would hold more than PC_PER_SPECIES or a
// dex is out of range -- check with pcRestoreValid() first.
bool pcRestoreValid(const uint8_t *recs, uint16_t n, uint16_t stride);
bool pcRestore(const uint8_t *recs, uint16_t n, uint16_t stride);
