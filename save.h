#pragma once
#include <stdint.h>
#include <stddef.h>
#include "dex.h"   // DEX_COUNT sizes the largest backup

// A whole-save backup, over the serial console.
//
// A run is weeks of real time living in one NVS partition, and there is now a
// lot in it: the creature, the party, the box, both badge ladders, the Pokedex,
// the streak, the trainer name. Nothing could get it back out.
//
// The backup is KEY-DRIVEN, not a struct of fields. SAVE_FIELDS lists every key
// the firmware stores along with its type, and both directions walk that one
// table through the ordinary Preferences API -- which means the identical code
// runs on the board and in the emulator, so all of it is testable here. An
// explicit struct would have been a second description of the save that drifts
// the moment somebody adds a field; `save_test` compares the table against the
// keys actually present after a save, so forgetting to add one fails a test
// rather than silently dropping it from everyone's backup.

#define SAVE_MAGIC0 'T'
#define SAVE_MAGIC1 'K'
#define SAVE_MAGIC2 'P'
#define SAVE_MAGIC3 'S'
#define SAVE_VERSION 2   // 2 (v3.61): Bill's PC follows the fields; 1 is still read
#define SAVE_HDR 8
// Shared EXPORT/IMPORT ceiling, including checkpoints. 4096 until v3.31, when
// creature IDs took a full save to ~3.4 KB and past save_test's 75% margin.
// Raised, never trimmed (save_test says why). Two static buffers of this size
// live in the sketch. A save from this build still fits an older build's 4096.
#define SAVE_TRANSFER_MAX 6144   // the NVS half; Bill's PC streams past it
// The whole backup at its largest: the NVS half, the PC section's 6-byte head,
// and a PC full to the brim. The console's IMPORT buffer is this big (PSRAM).
#define SAVE_IMPORT_MAX (SAVE_TRANSFER_MAX + 6 + (size_t)DEX_COUNT * 6 * 64)

enum SaveKind : uint8_t {
  SK_U8 = 1, SK_I8, SK_BOOL, SK_U16, SK_I16, SK_U32, SK_BYTES, SK_STR,
};

struct SaveField {
  const char *key;
  uint8_t kind;
};
extern const SaveField SAVE_FIELDS[];
extern const uint16_t SAVE_FIELD_COUNT;

// Serialises the live save. Returns the number of bytes written, or 0 if it
// would not fit in cap.
size_t saveExport(uint8_t *out, size_t cap);
// The same backup, streamed: `bytes` is called once with the total, then `line`
// with each IMPORT data line's hex (48 bytes a line). Returns the total, or 0.
// The console's EXPORT uses it, because a full PC does not fit in RAM.
size_t saveExportStream(void (*bytes)(size_t total), void (*line)(const char *hex));

// Restores one. VALIDATES THE WHOLE BLOB FIRST and only then touches NVS: a
// half-applied restore over a good save would be worse than no backup at all.
// Returns false and changes nothing if the blob is not ours, is the wrong
// version, or fails its checksum. The caller must reload afterwards.
bool saveImport(const uint8_t *in, size_t n);

// Roughly what saveExport needs, for sizing a buffer.
size_t saveExportSize();
