// The emulator's pcfs.h stand-in, for tests and main_sdl.cpp.
#pragma once
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
enum { PCFS_HOST_OK, PCFS_HOST_BLANK, PCFS_HOST_BROKEN };
std::map<std::string, std::vector<uint8_t>> &pcfsHostFiles();
void pcfsHostReset(int state);          // forget every file; the partition is `state`
void pcfsHostSetState(int state);       // keep the files, change how a mount goes
void pcfsHostFailWritesAfter(int n, bool torn = true);
void pcfsHostResumeWrites();
int pcfsHostFormats();                  // how many times a mount formatted
void pcfsHostLoad(const char *file);
void pcfsHostSave(const char *file);
