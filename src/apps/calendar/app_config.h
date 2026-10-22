#pragma once
#include "board_config.h"

// =========================================================================
//  Calendar — app configuration
//
//  Non-secret settings only. Wi-Fi and Google OAuth credentials live in
//  secrets.h (gitignored) instead -- see SETUP.md.
// =========================================================================

// ---- Wi-Fi ----------------------------------------------------------------
#define WIFI_CONNECT_TIMEOUT_MS     15000  // per attempt, before backing off
#define WIFI_RETRY_BACKOFF_MS       5000   // pause between reconnect attempts

// ---- Time sync (NTP) -------------------------------------------------------
// This board has no battery-backed RTC -- boot always starts at epoch 0
// until this succeeds. Values are compared as raw UTC epoch seconds
// throughout the app, so no local timezone setting is needed anywhere.
#define NTP_SERVER_1                "pool.ntp.org"
#define NTP_SERVER_2                "time.google.com"
#define NTP_SYNC_TIMEOUT_MS         15000

// ---- Google Calendar --------------------------------------------------
#define GCAL_CALENDAR_ID            "primary"
#define GCAL_LOOKAHEAD_SECONDS      (18UL * 3600UL)   // how far ahead to fetch
#define GCAL_MAX_EVENTS             5
#define GCAL_TOKEN_REFRESH_MARGIN_S 60   // refresh this many seconds before expiry

// ---- Polling cadence --------------------------------------------------
// Plugged in: stay connected, poll often. On battery: poll less often and
// drop the Wi-Fi radio between polls (see main.cpp) to save power.
#define POLL_INTERVAL_PLUGGED_MS    (60UL * 1000UL)
#define POLL_INTERVAL_BATTERY_MS    (5UL * 60UL * 1000UL)

// ---- Display ------------------------------------------------------------
#define DISPLAY_BRIGHTNESS_PLUGGED  DISPLAY_BRIGHTNESS
#define DISPLAY_BRIGHTNESS_BATTERY  90
#define FRAME_INTERVAL_MS           1000   // duration readout only needs 1 Hz

// ---- Interaction ------------------------------------------------------
// Same "UI rotates to follow" behaviour as pomodoro/hourglass, inherited
// from src/common/motion.h -- turn the board, the dashboard turns with it.
#define SCREEN_FOLLOWS_DEVICE       1
