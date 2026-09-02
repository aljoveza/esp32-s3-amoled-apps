#pragma once
#include <Arduino.h>
#include "app_config.h"

enum PomodoroPhase : uint8_t { PH_WORK = 0, PH_BREAK, PH_LONG_BREAK };
enum PomodoroStatus : uint8_t { PM_RUNNING = 0, PM_PAUSED, PM_HOLD, PM_TRANSITION };

struct PomodoroView {
  uint8_t minutes;      // remaining, clamped to 99
  uint8_t seconds;      // remaining
  float   fraction;     // 0..1 of the phase remaining (1 = just started)
  uint8_t phase;        // PomodoroPhase
  uint8_t status;       // PomodoroStatus
  uint8_t setCount;     // work phases completed in the current set, 0..SET_SIZE
};

void pomodoroUiInit();
void pomodoroUiFullRedraw(const PomodoroView &v);
void pomodoroUiRender(const PomodoroView &v);

// A brief bright sweep played at each phase change. Purely cosmetic, never
// blocking; call once when a phase completes.
void pomodoroUiTriggerTransition();

void    pomodoroUiSetRotation(uint8_t rot);
uint8_t pomodoroUiRotation();
