// pcfs.h on the board: the `ffat` partition (9.875 MB at 0x610000), which
// nothing else uses -- sprites stream from the SD card. The emulator lists its
// sources by name and builds tools/emu/pcfs_host.cpp instead of this file.
#ifdef ARDUINO_ARCH_ESP32
#include "pcfs.h"
#include <FFat.h>
#include <esp_partition.h>

// Never written = the first 4 KB still erased.
bool pcfsBlank() {
  const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                      ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
  if (!p) return false;
  uint8_t b[256];
  for (uint32_t off = 0; off < 4096; off += sizeof(b)) {
    if (esp_partition_read(p, off, b, sizeof(b)) != ESP_OK) return false;
    for (uint8_t v : b) if (v != 0xFF) return false;
  }
  return true;
}

bool pcfsTryMount() { return FFat.begin(false); }

// FFat.format() reports false when the old contents were not wear-levelled
// (leftovers of other firmware) even though it formats anyway, so the mount
// after it is the answer, not its return value.
bool pcfsFormat() {
  FFat.format();
  return FFat.begin(false);
}

long pcfsRead(const char *path, uint8_t *buf, size_t cap) {
  if (!FFat.exists(path)) return -1;
  File f = FFat.open(path, "r");
  if (!f) return -1;
  size_t n = f.size();
  if (n > cap) { f.close(); return -1; }
  size_t got = f.read(buf, n);
  f.close();
  return got == n ? (long)n : -1;
}

bool pcfsWrite(const char *path, const uint8_t *buf, size_t n) {
  File f = FFat.open(path, "w");
  if (!f) return false;
  size_t w = f.write(buf, n);
  f.flush();
  f.close();
  return w == n;
}

bool pcfsRemove(const char *path) { return !FFat.exists(path) || FFat.remove(path); }
#endif
