#pragma once
#include <Arduino.h>
#include "app_config.h"

enum RTState : uint8_t { RT_ARMED = 0, RT_RUNNING, RT_PAUSED, RT_ALARM };

struct RoundTimerView {
  uint16_t secondsRemaining;
  float    fraction;      // 0..1 of the round remaining, for the ring
  uint8_t  state;         // RTState
  uint32_t roundNumber;   // 1-based, how many rounds this session has run
};

void roundtimerUiInit();
void roundtimerUiFullRedraw(const RoundTimerView &v);
void roundtimerUiRender(const RoundTimerView &v);

// The bright sweep + flash played once when a round hits zero.
void roundtimerUiTriggerAlarm();

void    roundtimerUiSetRotation(uint8_t rot);
uint8_t roundtimerUiRotation();

enum RTZone : int8_t { RT_ZONE_NONE = -1, RT_ZONE_DECREASE = 0, RT_ZONE_START = 1, RT_ZONE_INCREASE = 2 };

// While idle (RT_ARMED), the screen is three vertical thirds: left to
// decrease the configured length, middle to start, right to increase.
// Outside RT_ARMED a tap means something else (pause/resume) and this
// isn't consulted. Takes a VIEW-space point, same convention as the rest
// of gfx_view -- convert with vTouchToView() first.
RTZone roundtimerUiZoneAt(int16_t viewX, int16_t viewY);
