#pragma once
#include <Arduino.h>

// =========================================================================
//  Calendar — display layout.
//
//  A fixed-orientation dashboard (desk display), not a handheld app -- no
//  device-tilt rotation, unlike apps meant to be picked up and turned.
// =========================================================================

struct CalendarView {
  const char *statusLine;   // non-null overrides the agenda with a status
                             // message (connecting, Wi-Fi down, auth error...)

  bool   hasCurrent;
  String currentTitle;
  uint32_t elapsedSec;      // how long the current event has been running

  bool   hasNext;
  String nextTitle;
  uint32_t remainingSec;    // time until the next event starts

  int8_t battPct;           // -1 == unavailable, like battery.h itself
  bool   battCharging;
};

void calendarUiInit();
void calendarUiFullRedraw(const CalendarView &v);
void calendarUiRender(const CalendarView &v);   // redraws only what changed
