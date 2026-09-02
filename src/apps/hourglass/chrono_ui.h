#pragma once
#include <Arduino.h>
#include "display_driver.h"
#include "app_config.h"

enum ChronoStatus : uint8_t {
  CH_RUNNING = 0,
  CH_PAUSED,
  CH_HOLD,      // finger down, about to trigger a reset
  CH_RESET,     // brief flash after a manual reset
  CH_FLIPPED    // brief flash after a quarter-turn reset
};

struct ChronoView {
  uint8_t hours;          // clamped to 99
  uint8_t minutes;
  uint8_t seconds;
  uint8_t hundredths;
  float   minuteFraction; // 0..1 position inside the current minute
  uint8_t status;         // ChronoStatus
  bool    running;        // grains only fall while the clock runs
};

void chronoUiInit();

// Quarter turns of the whole UI, 0..3. The panel controller has no hardware
// rotation, so this is applied in software to every primitive.
void chronoUiSetRotation(uint8_t rot);
uint8_t chronoUiRotation();

// Drops the "turn 90 to restart" hint when no IMU answered on the bus.
void chronoUiSetTiltHint(bool enabled);

// Optional one-line hardware status drawn at the bottom of the panel, so the
// bring-up state can be read off the device without a serial connection.
void chronoUiSetDiagLine(const char *text);
void chronoUiFullRedraw(const ChronoView &v);
void chronoUiRender(const ChronoView &v);

// Kicks off the ~420 ms "turn the glass over" sweep: the bottom bulb empties
// back through the neck and the top bulb refills. Purely cosmetic, driven from
// chronoUiRender(), never blocking.
void chronoUiTriggerFlip();
bool chronoUiFlipping();
