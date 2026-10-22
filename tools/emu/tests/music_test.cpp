// The chiptune (v3.56): the real Game Boy themes as executed by the sequencer
// the board runs (musicplay.cpp), on the synth's two pulse voices and its new
// wave voice. What is measurable is measured here; whether it sounds RIGHT is
// for ears -- `tools/emu/tamapoke-emu --wav out.wav --demo wildbattle`.
#include "../../gbsynth.h"
#include "../../musicplay.h"
#include "../../music.h"     // the tables themselves, for checks on the data
#include "../../audio.h"     // the Music ids only
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// zero crossings per second of a rendered buffer
static int crossings(const std::vector<int16_t> &b) {
  int n = 0;
  for (size_t i = 1; i < b.size(); i++) if ((b[i - 1] < 0) != (b[i] < 0)) n++;
  return n;
}
static int peak(const std::vector<int16_t> &b) {
  int p = 0;
  for (int16_t v : b) p = std::max(p, abs((int)v));
  return p;
}

int main() {
  // --- 1. every moment of the game has a track
  {
    bool all = true;
    for (uint8_t m = 1; m < MUS_COUNT; m++)
      if (musicTrackFor(m) < 0) { printf("      Music id %u has no track\n", m); all = false; }
    ck(all, "every Music id names a track that was generated");
    ck(musicTrackFor(MUS_NONE) < 0, "and silence is no track");
    ck(musicTrackFor(MUS_WILD) != musicTrackFor(MUS_GYM) && musicTrackFor(MUS_FINAL) != musicTrackFor(MUS_GYM),
       "a wild fight, a gym and the champion each have their own theme");
  }

  // --- 2. the data: the engine's tempo, and channels that stay together
  {
    // One step of the wild battle theme is speed 12 at tempo 104: 4.875 frames,
    // 82 ms on average. The first generator used 18 ms a unit (216 ms a step),
    // which put every theme at about 40% speed.
    const MusicTrack &w = MUSIC_TBL[musicTrackByName("wildbattle")];
    std::vector<uint16_t> d;
    for (uint16_t i = 0; i < w.n[0]; i++) d.push_back(w.ch[0][i].ms);
    std::sort(d.begin(), d.end());
    uint16_t median = d[d.size() / 2];
    printf("      wild battle: median event %u ms\n", median);
    ck(median >= 60 && median <= 100, "the wild battle theme moves at the engine's tempo, not a guess");

    bool together = true, threeVoices = true;
    for (uint8_t t = 0; t < MUSIC_TRACKS; t++) {
      const MusicTrack &m = MUSIC_TBL[t];
      uint32_t len[3];
      for (int c = 0; c < 3; c++) {
        len[c] = 0;
        if (m.loop[c] == MUSIC_NO_LOOP) continue;
        for (uint16_t i = m.loop[c]; i < m.n[c]; i++) len[c] += m.ch[c][i].ms;
      }
      uint32_t lo = std::min({len[0], len[1], len[2]}), hi = std::max({len[0], len[1], len[2]});
      if (m.loop[0] != MUSIC_NO_LOOP && hi - lo > 20) {
        printf("      %s: loops of %u / %u / %u ms\n", m.name, len[0], len[1], len[2]);
        together = false;
      }
      bool wave = false;
      for (uint16_t i = 0; i < m.n[2]; i++) if (m.ch[2][i].freq && m.ch[2][i].vol) wave = true;
      if (!wave) { printf("      %s: no bass line\n", m.name); threeVoices = false; }
    }
    ck(together, "every looping track's three channels loop over the same length");
    ck(threeVoices, "and every track has its wave-channel bass");
  }

  // --- 3. the wave voice
  {
    GbSynth s;
    s.wave(1800, MUSIC_WAVES[0], 1, 1000);
    std::vector<int16_t> full(GB_RATE);
    s.render(full.data(), full.size(), 10);
    int want = (int)(gbFreqHz(1800) / 2) * 2;     // an octave below a pulse: two crossings a cycle
    int got = crossings(full);
    printf("      wave at 1800: %d crossings/s, expected about %d\n", got, want);
    ck(abs(got - want) < want / 10, "the wave voice sounds an octave below a pulse on the same register");
    GbSynth h; h.wave(1800, MUSIC_WAVES[0], 2, 1000);
    std::vector<int16_t> half(GB_RATE);
    h.render(half.data(), half.size(), 10);
    ck(peak(half) > 0 && peak(half) * 3 < peak(full) * 2, "output level 2 is quieter than level 1");
    GbSynth z; z.wave(1800, MUSIC_WAVES[0], 0, 1000);
    std::vector<int16_t> mute(GB_RATE);
    z.render(mute.data(), mute.size(), 10);
    ck(peak(mute) == 0, "and level 0 is silent");
  }

  // --- 4. the player: a looping theme plays on past its end, through all voices
  {
    GbSynth s;
    MusicPlayer p;
    uint8_t t = (uint8_t)musicTrackFor(MUS_WILD);
    p.start(t);
    uint32_t loop = musicLoopMs(t), clock = 0;
    const MusicTrack &m = MUSIC_TBL[t];
    uint32_t whole = 0;
    for (uint16_t i = 0; i < m.n[0]; i++) whole += m.ch[0][i].ms;
    std::vector<int16_t> buf;
    while (clock < whole + loop) {                  // to the end and once round again
      uint32_t step = p.step(s, 60);
      size_t at = buf.size();
      buf.resize(at + step * GB_RATE / 1000);
      s.render(buf.data() + at, buf.size() - at, 8);
      p.advance(step);
      clock += step;
    }
    ck(p.playing() && !p.finished(), "a looping theme is still playing after its last note");
    std::vector<int16_t> tail(buf.end() - GB_RATE * 5, buf.end());
    ck(peak(tail) > 2000, "and is still sounding, past the loop point");
    // the same stretch with the bass silenced must differ: the wave voice is in the mix
    GbSynth s2; MusicPlayer p2; p2.start(t);
    std::vector<int16_t> a(GB_RATE * 3), b2(GB_RATE * 3);
    for (uint32_t ms = 0; ms < 3000; ) {
      uint32_t step = p2.step(s2, 60);
      s2.render(a.data() + ms * GB_RATE / 1000, step * GB_RATE / 1000, 8);
      p2.advance(step); ms += step;
      if (ms + 60 > 3000) break;
    }
    GbSynth s3; MusicPlayer p3; p3.start(t);
    for (uint32_t ms = 0; ms < 3000; ) {
      uint32_t step = p3.step(s3, 60);
      s3.silence(3);
      s3.render(b2.data() + ms * GB_RATE / 1000, step * GB_RATE / 1000, 8);
      p3.advance(step); ms += step;
      if (ms + 60 > 3000) break;
    }
    int diff = 0;
    for (size_t i = 0; i < a.size(); i++) if (a[i] != b2[i]) diff++;
    ck(diff > GB_RATE / 2, "the bass line is really in the mix");
    p.stop(s);
    ck(!p.playing(), "and stop() ends it");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
