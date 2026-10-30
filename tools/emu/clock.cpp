// The emulator's millis(). Its own translation unit so the headless tests link
// the same clock the SDL binary runs -- a copy in the test would let the test
// pass while --fast still broke touch in the real thing.
#include "Arduino.h"
#include <chrono>

extern volatile bool g_touchDown;   // SDL layer: mouse button state
extern bool wasPressed;             // sketch: a gesture is being tracked

static std::chrono::steady_clock::time_point g_tLast = std::chrono::steady_clock::now();
static uint64_t g_virtUs = 0;      // accumulated in us so the scaling cannot drift
static uint32_t g_timeScale = 1;   // >1 makes in-game minutes pass faster

void emuSetTimeScale(uint32_t s) { g_timeScale = s ? s : 1; }
uint32_t emuTimeScale() { return g_timeScale; }
// Moves the clock on by exactly `ms`, for a headless run that simulates days
// (tools/emu/playsim.cpp). The RTC follows millis(), so the date moves too.
void emuAdvanceMs(uint64_t ms) { g_virtUs += ms * 1000; }
// Time moves ONLY by emuAdvanceMs(): no wall clock at all, so a simulated run
// does the same thing however fast the host is (and with drawing on or off).
static bool g_virtualOnly = false;
void emuVirtualClock(bool on) { g_virtualOnly = on; }
// The same clock without millis()'s 32-bit wrap (49.7 days). The board's RTC
// is its own chip and does not wrap; the emulator's RTC reads this, so a
// simulated run longer than seven weeks keeps its calendar (playsim found it
// going back to the first day). millis() still wraps, as on the board.
uint64_t emuMillis64() { millis(); return g_virtUs / 1000; }

// The sketch times gestures off this same clock -- a tap needs dt < 1500 and a
// swipe dt < 800 -- so scaling it wholesale shrinks the window you have to
// click in to 1500/scale real ms. At --fast 60 that is 25 ms, i.e. touch is
// unusable. So the scale is suspended for as long as a gesture is in flight:
// game time runs fast while you are not touching the panel, and real-time the
// moment you are. Accumulating (rather than scaling total elapsed time) keeps
// it monotonic across a change of rate.
uint32_t millis() {
  if (g_virtualOnly) return (uint32_t)(g_virtUs / 1000);
  auto now = std::chrono::steady_clock::now();
  uint64_t dus = std::chrono::duration_cast<std::chrono::microseconds>(now - g_tLast).count();
  g_tLast = now;
  bool gesturing = g_touchDown || wasPressed;   // press .. release resolved
  g_virtUs += dus * (gesturing ? 1 : g_timeScale);
  return (uint32_t)(g_virtUs / 1000);
}
