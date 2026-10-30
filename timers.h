#pragma once
// Deadlines against millis(), safe across its wrap (v3.88).
//
// millis() is 32 bits and wraps every 49.7 days. A deadline compared with a
// plain `now > until` misfires when it is set shortly before the wrap: `until`
// lands just past zero, `now` is still near 2^32, and the timer reads as long
// over -- or, the other way round, as running for another 49 days (a farewell
// ceremony that never ends). Comparing the DIFFERENCE as a signed number is
// right on either side of the wrap, for any span under 24.8 days.
//
// Many deadlines use 0 for "no timer". A bare signed difference would read a
// 0 deadline as still running once millis() passes 2^31, so timerRunning()
// keeps that meaning explicitly. Ask these; never compare a deadline directly.
#include <stdint.h>

// `at` has come (now is at or past it). For a deadline that is always set.
inline bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }
// A deadline still ahead; 0 is "no timer" and never runs.
inline bool timerRunning(uint32_t now, uint32_t until) { return until && !timeReached(now, until); }
// What is left of a running timer, 0 once over or unset.
inline uint32_t timeLeft(uint32_t now, uint32_t until) { return timerRunning(now, until) ? until - now : 0; }
