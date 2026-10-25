// The files under Bill's PC (v3.61): whole-file read and write, nothing else.
// On the board it is the unused `ffat` partition (pcfs_ffat.cpp); in the
// emulator and the tests an in-memory stand-in with fault injection
// (tools/emu/pcfs_host.cpp). pcstore.cpp is the only caller.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum PcfsMount : uint8_t {
  PCFS_OK,          // mounted what was there
  PCFS_FORMATTED,   // nothing of ours could be there: formatted, empty
  PCFS_FAILED,      // would not mount and may hold a collection: never formatted over
};
// The platform half. pcstore.cpp's pcBegin() decides when a format is safe.
bool pcfsTryMount();   // mount what is there, never formatting
bool pcfsBlank();      // the partition was never written (its first 4 KB erased)
bool pcfsFormat();     // a fresh, empty filesystem; true if it now mounts
// The whole file into buf. Its length, or -1 when absent, unreadable or > cap.
long pcfsRead(const char *path, uint8_t *buf, size_t cap);
// The whole file, replacing it. True only if every byte landed.
bool pcfsWrite(const char *path, const uint8_t *buf, size_t n);
bool pcfsRemove(const char *path);   // true if it is gone (or never was)
