// The music sequencer -- see musicplay.h.
#include "musicplay.h"
#include "music.h"
#include "audio.h"   // the Music ids only
#include <string.h>

uint8_t musicTrackCount() { return MUSIC_TRACKS; }

const char *musicTrackName(uint8_t track) {
  return track < MUSIC_TRACKS ? MUSIC_TBL[track].name : "";
}

int musicTrackByName(const char *name) {
  for (uint8_t i = 0; i < MUSIC_TRACKS; i++)
    if (!strcmp(MUSIC_TBL[i].name, name)) return i;
  return -1;
}

uint32_t musicLoopMs(uint8_t track) {
  if (track >= MUSIC_TRACKS) return 0;
  const MusicTrack &t = MUSIC_TBL[track];
  if (t.loop[0] == MUSIC_NO_LOOP) return 0;
  uint32_t ms = 0;
  for (uint16_t i = t.loop[0]; i < t.n[0]; i++) ms += t.ch[0][i].ms;
  return ms;
}

// In audio.h's order. Red has no separate Elite Four theme -- they fight to the
// gym leader battle, and only the champion gets the final battle.
static const char *const MUS_FILE[MUS_COUNT] = {
  nullptr,               // MUS_NONE
  "gymleaderbattle",     // MUS_GYM
  "defeatedgymleader",   // MUS_WIN_GYM
  "wildbattle",          // MUS_WILD
  "trainerbattle",       // MUS_TRAINER
  "finalbattle",         // MUS_FINAL
  "defeatedtrainer",     // MUS_WIN_TRAINER
  "defeatedwildmon",     // MUS_WIN_WILD
  "gamecorner",          // MUS_GAMECORNER
  "surfingpikachu",      // MUS_SURF
};

int musicTrackFor(uint8_t mus) {
  if (mus == MUS_NONE || mus >= MUS_COUNT || !MUS_FILE[mus]) return -1;
  return musicTrackByName(MUS_FILE[mus]);
}

void MusicPlayer::start(uint8_t track) {
  track_ = track < MUSIC_TRACKS ? track : NONE;
  done_ = false;
  clock_ = 0;
  for (int c = 0; c < 3; c++) { at_[c] = 0; idx_[c] = 0; chDone_[c] = false; }
}

void MusicPlayer::stop(GbSynth &syn) {
  track_ = NONE;
  done_ = false;
  syn.silence(0);
  syn.silence(1);
  syn.silence(3);
}

uint32_t MusicPlayer::step(GbSynth &syn, uint32_t maxMs) {
  if (track_ == NONE || done_) return maxMs;
  const MusicTrack &t = MUSIC_TBL[track_];
  for (int c = 0; c < 3; c++) {
    if (chDone_[c] || clock_ < at_[c]) continue;
    if (idx_[c] >= t.n[c]) {
      if (t.loop[c] == MUSIC_NO_LOOP) { chDone_[c] = true; continue; }
      idx_[c] = t.loop[c];
    }
    const MusicNote &n = t.ch[c][idx_[c]++];
    // channel 1 and 2 are the pulse voices; channel 3 the wave voice (synth 3)
    if (c < 2) {
      if (n.freq) syn.note((uint8_t)c, n.freq, n.duty, n.vol, n.envDir, n.envPeriod, n.ms);
      else syn.silence((uint8_t)c);
    } else {
      if (n.freq && n.duty < MUSIC_WAVE_COUNT) syn.wave(n.freq, MUSIC_WAVES[n.duty], n.vol, n.ms);
      else syn.silence(3);
    }
    at_[c] += n.ms;
  }
  // A one-shot is over when its melody is.
  if (chDone_[0]) { done_ = true; return maxMs; }
  uint32_t next = UINT32_MAX;
  for (int c = 0; c < 3; c++) if (!chDone_[c] && at_[c] < next) next = at_[c];
  if (next == UINT32_MAX || next <= clock_) return 1;
  uint32_t wait = next - clock_;
  return wait < maxMs ? wait : maxMs;
}
