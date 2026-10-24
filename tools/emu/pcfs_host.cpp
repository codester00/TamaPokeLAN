// pcfs.h in the emulator and the tests: files in memory, optionally persisted
// to one file beside the save (`x.nvs` -> `x.nvs.pc`) by main_sdl.cpp, with
// the faults a board can have -- a blank partition, one that will not mount,
// and a write that stops partway (a power cut).
#include "pcfs.h"
#include "pcfs_host.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>

static std::map<std::string, std::vector<uint8_t>> g_files;
static int g_state = PCFS_HOST_BLANK;
static int g_budget = -1;     // successful writes left; -1 = unlimited
static bool g_torn = true;    // a failed write leaves half a file behind
static int g_formats = 0;

std::map<std::string, std::vector<uint8_t>> &pcfsHostFiles() { return g_files; }
void pcfsHostReset(int state) { g_files.clear(); g_state = state; g_budget = -1; g_formats = 0; }
void pcfsHostSetState(int state) { g_state = state; }
void pcfsHostFailWritesAfter(int n, bool torn) { g_budget = n; g_torn = torn; }
void pcfsHostResumeWrites() { g_budget = -1; }
int pcfsHostFormats() { return g_formats; }

PcfsMount pcfsMount() {
  if (g_state == PCFS_HOST_BROKEN) return PCFS_FAILED;
  if (g_state == PCFS_HOST_BLANK) { g_files.clear(); g_state = PCFS_HOST_OK; g_formats++; return PCFS_FORMATTED; }
  return PCFS_OK;
}

long pcfsRead(const char *path, uint8_t *buf, size_t cap) {
  auto it = g_files.find(path);
  if (it == g_files.end() || it->second.size() > cap) return -1;
  for (size_t i = 0; i < it->second.size(); i++) buf[i] = it->second[i];
  return (long)it->second.size();
}

bool pcfsWrite(const char *path, const uint8_t *buf, size_t n) {
  if (g_budget == 0) {
    if (g_torn) g_files[path] = std::vector<uint8_t>(buf, buf + n / 2);
    return false;
  }
  if (g_budget > 0) g_budget--;
  g_files[path] = std::vector<uint8_t>(buf, buf + n);
  return true;
}

bool pcfsRemove(const char *path) { g_files.erase(path); return true; }

// One file holding them all: [u16 name length][name][u32 length][bytes]...
void pcfsHostLoad(const char *file) {
  FILE *f = fopen(file, "rb");
  if (!f) { pcfsHostReset(PCFS_HOST_BLANK); return; }
  pcfsHostReset(PCFS_HOST_OK);
  for (;;) {
    uint16_t nl; uint32_t dl;
    if (fread(&nl, 2, 1, f) != 1) break;
    std::string name(nl, '\0');
    if (fread(&name[0], 1, nl, f) != nl || fread(&dl, 4, 1, f) != 1) break;
    std::vector<uint8_t> d(dl);
    if (dl && fread(d.data(), 1, dl, f) != dl) break;
    g_files[name] = d;
  }
  fclose(f);
}

void pcfsHostSave(const char *file) {
  if (g_state == PCFS_HOST_BLANK) return;
  FILE *f = fopen(file, "wb");
  if (!f) return;
  for (auto &kv : g_files) {
    uint16_t nl = (uint16_t)kv.first.size(); uint32_t dl = (uint32_t)kv.second.size();
    fwrite(&nl, 2, 1, f); fwrite(kv.first.data(), 1, nl, f);
    fwrite(&dl, 4, 1, f); if (dl) fwrite(kv.second.data(), 1, dl, f);
  }
  fclose(f);
}
