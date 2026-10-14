#pragma once
// SensorLib must be 0.4.1. NOT 0.5.0, which crashes this firmware at boot.
//
// 0.5.0 (2026-09-22) added rewritten drivers compiled into the library while
// keeping the old header-only classes this firmware includes -- under the SAME
// class names (SensorQMI8658, and the PCF85063 RTC's base classes). Two classes
// with one name is an ODR violation: the linker keeps one vtable and the other
// half's code calls through it, so the board boot-looped with
// "InstrFetchProhibited ... PC 0x00010000" inside pedoBegin(), straight after
// "thumbnails loaded". It built cleanly and the emulator, which stubs these
// sensors, could not see it. v3.28 shipped with 0.4.1, the release before.
//
// The new driver's directory is the tell, and refusing to build beats a board
// that cannot start. tools/check_odr.sh catches the general shape at build time.
#if defined(__has_include)
#if __has_include(<sensor/imu/qmi8658/SensorQMI8658.hpp>)
#error "SensorLib 0.5.0+ is installed: it boot-loops this firmware. Install SensorLib 0.4.1 (see sensorlib_guard.h)."
#endif
#endif
