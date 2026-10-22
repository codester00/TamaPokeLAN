#pragma once
#include <stdint.h>
#include "gbsynth.h"

// The music sequencer (v3.56): walks one of music.h's tracks and drives the
// synth's three music voices -- pulse 1, pulse 2 and the wave channel's bass.
//
// It has no Arduino dependency, like gbsynth.cpp, so the board's audio task,
// the emulator's --wav export and the tests all run THIS code. That is the
// point: the first sequencer lived inside the audio task, where nothing but a
// board could ever exercise it.
//
// This is the only file that includes music.h. Its tables are `static`, so a
// second includer would carry a second copy of them in flash.

uint8_t musicTrackCount();
const char *musicTrackName(uint8_t track);
int musicTrackByName(const char *name);   // -1 if none
// How long one pass of a looping track's loop lasts, in ms (channel 1), or 0
// for a one-shot.
uint32_t musicLoopMs(uint8_t track);
// THE mapping from a moment in the game (audio.h's Music ids) to a track, by
// the track's file name in the disassembly. -1 for MUS_NONE or a name that is
// no longer generated -- music_test fails on the latter.
int musicTrackFor(uint8_t mus);

class MusicPlayer {
 public:
  void start(uint8_t track);     // from the top
  void stop(GbSynth &syn);       // and silence its voices
  bool playing() const { return track_ != NONE; }
  uint8_t track() const { return track_; }
  // Plays every note that is due, then says how many ms until the next one is
  // (at most `maxMs`). The caller renders that long and calls again with the
  // time it rendered: advance(syn, ms) moves the clock, step(syn) starts notes.
  uint32_t step(GbSynth &syn, uint32_t maxMs);
  void advance(uint32_t ms) { clock_ += ms; }
  // One-shots end by themselves; a looping track never does.
  bool finished() const { return track_ != NONE && done_; }

  static const uint8_t NONE = 0xFF;

 private:
  uint8_t track_ = NONE;
  bool done_ = false;
  uint32_t clock_ = 0;
  uint32_t at_[3] = { 0, 0, 0 };
  uint16_t idx_[3] = { 0, 0, 0 };
  bool chDone_[3] = { false, false, false };
};
