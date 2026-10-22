#pragma once
#include <Arduino.h>

// =========================================================================
//  Calendar — display layout.
//
//  Rotates to follow the device, same as pomodoro/hourglass -- see
//  calendarUiSetRotation() and SCREEN_FOLLOWS_DEVICE in app_config.h.
// =========================================================================

struct CalendarView {
  const char *statusLine;   // non-null overrides the agenda with a status
                             // message (connecting, Wi-Fi down, auth error...)

  bool   hasCurrent;
  String currentTitle;
  uint32_t elapsedSec;      // how long the current event has been running
  uint32_t totalSec;        // its total duration (end - start), for the progress bar

  bool   hasNext;
  String nextTitle;
  uint32_t remainingSec;    // time until the next event starts

  int8_t battPct;           // -1 == unavailable, like battery.h itself
  bool   battCharging;
};

void calendarUiInit();
void calendarUiSetRotation(uint8_t rot);        // 0..3 quarter turns
void calendarUiFullRedraw(const CalendarView &v);
void calendarUiRender(const CalendarView &v);   // redraws only what changed
