#pragma once
#include "board_config.h"

// =========================================================================
//  Hourglass chronometer — app configuration
// =========================================================================

// ---- Sand ring geometry (the "glass" hugging the screen edges) ----------
#define RING_MARGIN                10   // gap between panel edge and ring
#define RING_THICK                 18   // ring band thickness in px

// One full trip of the sand around the ring == one minute.
// 60 cells per bulb means every cell is exactly one second.
#define RING_CELLS_PER_BULB        60

// ---- Interaction --------------------------------------------------------
// Short tap toggles pause; long press resets to zero.
#define TOUCH_HOLD_RESET_MS        1200
// A settled quarter turn restarts the chronometer, like flipping an hourglass.
#define TILT_RESET_ENABLED         1
// The UI rotates to follow the device so the sand always falls downwards.
// 0 pins the UI upright. (Which quadrant is "up" is SCREEN_ROTATION_OFFSET,
// in board_config.h, because it depends on how the IMU is mounted.)
#define SCREEN_FOLLOWS_DEVICE      1

// ---- Rendering ----------------------------------------------------------
#define FRAME_INTERVAL_MS          25    // ~40 fps
// Shift the centre readout a couple of px every few minutes so a long
// running session cannot burn a static "00:" into the AMOLED.
#define ANTI_BURNIN_PERIOD_MS      240000UL
