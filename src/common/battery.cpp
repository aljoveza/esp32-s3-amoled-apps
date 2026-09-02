#include "battery.h"
#include "pin_config.h"
#include <Wire.h>
#include "XPowersLib.h"

static XPowersPMU s_pmu;
static bool s_ready = false;

bool batteryAvailable() { return s_ready; }

bool batteryBegin() {
  // Shares the I2C bus Wire.begin() already opened in display_driver.cpp,
  // same pattern as the ES8311 codec.
  if (!s_pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("[Battery] AXP2101 not found.");
    return false;
  }
  s_pmu.enableBattDetection();
  s_pmu.enableBattVoltageMeasure();
  s_ready = true;
  Serial.println("[Battery] AXP2101 ready.");
  return true;
}

int8_t batteryPercent() {
  if (!s_ready || !s_pmu.isBatteryConnect()) return -1;
  int p = s_pmu.getBatteryPercent();
  if (p < 0) return -1;
  if (p > 100) p = 100;
  return (int8_t)p;
}

bool batteryCharging() {
  if (!s_ready) return false;
  return s_pmu.isCharging();
}
