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

// ---- Audio (ES8311 codec + I2S) ------------------------------------------
// Bring the codec up at all. Apps that use audioBeep() should still check
// audioAvailable() and skip the sound gracefully if this hardware is ever
// missing or fails to init -- audio has had far less runtime on this board
// than the display/touch/IMU path, see docs/HARDWARE.md.
#define AUDIO_ENABLED               1
#define AUDIO_SAMPLE_RATE           16000
#define AUDIO_VOLUME                70    // 0..100, ES8311 voice volume

// ---- App switching --------------------------------------------------------
// Reserved corner (view-space, top-left of whatever is currently on screen)
// that any app can be held on to return to the launcher menu. Kept off to
// the side of the screen apps' own tap/hold gestures already use, and small
// enough not to be brushed accidentally by a normal tap near the edge.
#define HOME_CORNER_SIZE            56    // px, square
#define HOME_HOLD_MS                1500

// ---- Diagnostics --------------------------------------------------------
// 1 adds a periodic I2C scan + IMU trace on serial, and a hardware status
// line on the panel. Apps may use this to decide whether to show debug UI.
#define BOARD_DEBUG                0
