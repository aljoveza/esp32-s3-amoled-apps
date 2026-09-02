#pragma once

// =========================================================================
//  Board configuration — Waveshare ESP32-S3-Touch-AMOLED-1.8
//
//  Everything here describes the HARDWARE and is shared by every app.
//  Anything that describes a particular app belongs in that app's
//  app_config.h instead.
// =========================================================================

// Board revision.
//   1 = V2  -> CO5300 panel + CST816T touch at 0x15   <-- this board
//   0 = V1  -> SH8601 panel + FT3168  touch at 0x38
// Getting this wrong leaves the screen completely black. See docs/HARDWARE.md.
#define BOARD_REVISION_V2          1

// AMOLED brightness, 0..255.
#define DISPLAY_BRIGHTNESS         200

// ---- IMU (QMI8658) ------------------------------------------------------
// Bring the IMU up at all. 0 skips it entirely.
#define IMU_ENABLED                1
// How long a new orientation must be held before it is accepted, in ms.
#define TILT_STABLE_MS             400
// Minimum in-plane gravity (g) for the orientation to be trusted. Below this
// the board is lying flat and the angle is meaningless, so the last known
// orientation is kept.
#define TILT_MIN_G                 0.42f
// Which orientation quadrant counts as "upright" for this board's IMU
// mounting. Bump by 1 until on-screen text reads the right way up.
#define SCREEN_ROTATION_OFFSET     0

// ---- Diagnostics --------------------------------------------------------
// 1 adds a periodic I2C scan + IMU trace on serial, and a hardware status
// line on the panel. Apps may use this to decide whether to show debug UI.
#define BOARD_DEBUG                0
