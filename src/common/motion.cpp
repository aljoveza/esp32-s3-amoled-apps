#include "motion.h"
#include "board_config.h"
#include "pin_config.h"
#include <Wire.h>
#include <math.h>
#include "SensorQMI8658.hpp"

static SensorQMI8658 s_imu;
static bool s_ready = false;

static int8_t   s_committed = -1;   // orientation the chronometer is running in
static int8_t   s_candidate = -1;
static uint32_t s_candidateAt = 0;
static uint32_t s_lastPoll = 0;

static float s_ax = 0, s_ay = 0, s_az = 0;
static bool  s_haveSample = false;

bool motionAvailable() { return s_ready; }

bool motionAccel(float *x, float *y, float *z) {
  if (!s_haveSample) return false;
  if (x) *x = s_ax;
  if (y) *y = s_ay;
  if (z) *z = s_az;
  return true;
}
int8_t motionOrientation() { return s_committed; }

bool motionBegin() {
#if IMU_ENABLED
  if (!s_imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("[Motion] QMI8658 not found - quarter-turn reset disabled.");
    s_ready = false;
    return false;
  }
  // CTRL1 bit 1 is the QMI8658's power-down bit. If it survives the reset the
  // sensor keeps answering on I2C but every output register reads 0x8000, so
  // clear it explicitly before configuring anything.
  s_imu.powerOn();
  // Exactly the vendor example's configuration -- the only combination this
  // board is known to produce valid samples with.
  s_imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                            SensorQMI8658::ACC_ODR_1000Hz,
                            SensorQMI8658::LPF_MODE_0);
  s_imu.enableAccelerometer();
  delay(50);
  s_ready = true;
  Serial.println("[Motion] QMI8658 ready - turn the device 90 to restart.");
  return true;
#else
  s_ready = false;
  return false;
#endif
}

bool motionPoll() {
  if (!s_ready) return false;

  uint32_t now = millis();
  if (now - s_lastPoll < 40) return false;   // 25 Hz is plenty for orientation
  s_lastPoll = now;

  IMUdata a;
  if (!s_imu.getDataReady()) return false;
  if (!s_imu.getAccelerometer(a.x, a.y, a.z)) return false;
  s_ax = a.x; s_ay = a.y; s_az = a.z;
  s_haveSample = true;

#if BOARD_DEBUG
  // Raw counts distinguish a real reading from the 0x8000 "no data" marker.
  static uint32_t lastRaw = 0;
  if (now - lastRaw >= 1000) {
    lastRaw = now;
    int16_t raw[3] = {0, 0, 0};
    s_imu.getAccelRaw(raw);
    Serial.printf("[Motion] raw=%d,%d,%d (0x%04X,0x%04X,0x%04X) status1=0x%02X g=%+.2f,%+.2f,%+.2f\n",
                  raw[0], raw[1], raw[2],
                  (uint16_t)raw[0], (uint16_t)raw[1], (uint16_t)raw[2],
                  s_imu.getStatusRegister(), a.x, a.y, a.z);
  }
#endif

  // Only the two in-plane axes matter. Face up on a desk they read ~0 and the
  // angle is pure noise, so hold the last known orientation instead.
  float planar = sqrtf(a.x * a.x + a.y * a.y);
  if (planar < TILT_MIN_G) {
    s_candidate = -1;
    return false;
  }

#if BOARD_DEBUG
  static uint32_t lastAng = 0;
  if (now - lastAng >= 1000) {
    lastAng = now;
    Serial.printf("[Motion] planar=%.2f angle=%+.0f committed_q=%d\n",
                  planar, atan2f(a.y, a.x) * 180.0f / (float)PI, s_committed);
  }
#endif

  float rel = (atan2f(a.y, a.x) * 180.0f / (float)PI) / 90.0f;
  int nearest = (int)lroundf(rel);
  // Deadband around the 45 degree boundaries so a board held at an awkward
  // angle cannot flicker between two quadrants.
  if (fabsf(rel - nearest) * 90.0f > 32.0f) {
    s_candidate = -1;
    return false;
  }
  int8_t q = (int8_t)(((nearest % 4) + 4) % 4);

  if (s_committed < 0) {          // first fix after boot: adopt it silently
    s_committed = q;
    s_candidate = q;
    return false;
  }
  if (q == s_committed) {
    s_candidate = -1;
    return false;
  }
  if (q != s_candidate) {
    s_candidate = q;
    s_candidateAt = now;
    return false;
  }
  if (now - s_candidateAt < TILT_STABLE_MS) return false;

  s_committed = q;
  s_candidate = -1;
  return true;
}
