#pragma once
#include "board_config.h"

// =========================================================================
//  Round Timer — app configuration
//
//  Press to start a round; it counts down to zero, alarms, and starts the
//  next round automatically -- forever, until you hold to stop. Built for
//  a game: configure once, then it just keeps timing rounds with no further
//  interaction needed.
// =========================================================================
#define ROUND_SECONDS               60    // length of one round, default/on-boot
// While idle, the screen splits into three touch zones so you can dial in a
// round length before starting -- left = -STEP, right = +STEP, middle =
// start. The adjusted value is NOT persisted (like everything else on this
// board): it resets to ROUND_SECONDS above on every power-up.
#define ROUND_SECONDS_STEP          15
#define ROUND_SECONDS_MIN           5
#define ROUND_SECONDS_MAX           1800   // 30 min

// Escalation zones, counted from zero (i.e. "last N seconds"). Whichever
// is bigger should be WARNING_SEC -- if ROUND_SECONDS is shorter than
// these, the round just spends its whole length in the later zones, which
// is fine.
#define WARNING_SEC                 15    // ring turns amber, spinning highlight starts
#define DANGER_SEC                  5     // full-screen flash + tick beep each second

// ---- Sound ----------------------------------------------------------------
// Tick beeps during the danger zone rise in pitch as zero approaches.
#define TICK_HZ_BASE                520
#define TICK_HZ_STEP                90    // added per second closer to zero
#define TICK_MS                     70
// The alarm at zero: two tones, played back to back.
#define ALARM_HZ_1                  880
#define ALARM_HZ_2                  660
#define ALARM_MS                    220
#define ALARM_HOLD_MS               1400  // pause here before the next round starts

// ---- Interaction ------------------------------------------------------------
#define TOUCH_HOLD_RESET_MS         1200  // hold = stop, back to idle
#define SCREEN_FOLLOWS_DEVICE       1

// ---- Ring geometry (same "glass hugging the edges" as the other timers) ---
#define RING_MARGIN                 10
#define RING_THICK                  18

// ---- Rendering --------------------------------------------------------------
#define FRAME_INTERVAL_MS           25    // ~40 fps -- this app needs to feel alive
