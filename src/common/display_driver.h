#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"
#include "board_config.h"
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include <Adafruit_XCA9554.h>

#if BOARD_REVISION_V2
#include "display/Arduino_CO5300.h"
extern Arduino_CO5300 *gfx;
#else
#include "display/Arduino_SH8601.h"
extern Arduino_SH8601 *gfx;
#endif

struct TouchPoint {
  bool isPressed;
  int16_t x;
  int16_t y;
};

void initDisplayAndTouch();
bool touchAvailable();
const char *touchModelName();
void i2cScan(const char *when);
TouchPoint readTouchInput();
void setDisplayBrightness(uint8_t brightness);
