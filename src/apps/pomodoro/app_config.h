#pragma once
#include "board_config.h"

// =========================================================================
//  Pomodoro — app configuration
//
//  Durations are the only thing meant to be tuned here. There is no
//  on-device settings screen by design -- that's a distraction surface in
//  its own right. Edit and reflash.
// =========================================================================
#define WORK_MINUTES               25
#define BREAK_MINUTES              5
#define LONG_BREAK_MINUTES         15
#define SET_SIZE                   4     // work phases before a long break

// ---- Interaction ----------------------------------------------------------
#define TOUCH_HOLD_RESET_MS        1200  // long press = reset current phase
#define SCREEN_FOLLOWS_DEVICE      1

// ---- Ring geometry (same "glass hugging the edges" as the hourglass app) --
#define RING_MARGIN                10
#define RING_THICK                 18

// ---- Phase-change chime -----------------------------------------------
// One short two-note chime, reused for every transition -- deliberately not
// configurable per-phase, to keep this simple. Silently skipped if the
// audio hardware never came up; see src/common/audio.h.
#define CHIME_NOTE1_HZ             880   // A5
#define CHIME_NOTE2_HZ             660   // E5
#define CHIME_NOTE_MS              120

// ---- Rendering --------------------------------------------------------
#define FRAME_INTERVAL_MS          25
#define ANTI_BURNIN_PERIOD_MS      240000UL
