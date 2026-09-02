// =========================================================================
//  ⧗  EDGE HOURGLASS CHRONOMETER
//  Waveshare ESP32-S3-Touch-AMOLED-1.8  (368 x 448)
//
//  The chronometer starts at zero on every power-up. The whole screen border
//  is the glass: the top half of the ring is the upper bulb, the bottom half
//  is the lower one, and the left and right edge mid-points are the waist.
//  One complete transfer of the sand takes exactly one minute, and every cell
//  of the ring is one second.
//
//  Tap to pause, hold to reset, or just turn the board a quarter turn -- the
//  IMU sees it and the glass is flipped over.
// =========================================================================
#include <Arduino.h>
#include <esp_timer.h>
#include "app_config.h"
#include "display_driver.h"
#include "chrono_ui.h"
#include "motion.h"

// ---- Clock core ---------------------------------------------------------
// esp_timer_get_time() is a 64-bit microsecond counter, so unlike millis()
// there is nothing to wrap around on a chronometer left running for weeks.
static uint64_t s_originUs = 0;
static uint64_t s_pausedAtUs = 0;
static bool     s_paused = false;

static inline uint64_t nowUs() { return (uint64_t)esp_timer_get_time(); }

static uint64_t elapsedUs() {
  return (s_paused ? s_pausedAtUs : nowUs()) - s_originUs;
}

static void chronoReset() {
  s_originUs = nowUs();
  if (s_paused) s_pausedAtUs = s_originUs;
}

static void chronoTogglePause() {
  if (s_paused) {
    s_originUs += nowUs() - s_pausedAtUs;   // give back the paused interval
    s_paused = false;
  } else {
    s_pausedAtUs = nowUs();
    s_paused = true;
  }
}

// ---- Input state --------------------------------------------------------
static bool     s_touchDown = false;
static uint32_t s_touchStart = 0;
static bool     s_holdFired = false;

static uint32_t s_flashUntil = 0;
static uint8_t  s_flashKind = CH_RESET;
static uint32_t s_lastFrame = 0;
static uint32_t s_lastMinute = 0;
static uint32_t s_lastTouchPoll = 0;

static ChronoView buildView(uint32_t nowMs) {
  uint64_t us = elapsedUs();
  uint64_t total = us / 10000ULL;             // hundredths of a second

  ChronoView v;
  v.hundredths = (uint8_t)(total % 100);
  uint64_t secs = total / 100;
  v.seconds = (uint8_t)(secs % 60);
  uint64_t mins = secs / 60;
  v.minutes = (uint8_t)(mins % 60);
  uint64_t hrs = mins / 60;
  v.hours = (uint8_t)(hrs > 99 ? 99 : hrs);
  v.minuteFraction = (float)(us % 60000000ULL) / 60000000.0f;
  v.running = !s_paused;

  if (nowMs < s_flashUntil)
    v.status = s_flashKind;
  else if (s_touchDown && !s_holdFired && nowMs - s_touchStart > 350)
    v.status = CH_HOLD;
  else
    v.status = s_paused ? CH_PAUSED : CH_RUNNING;

  return v;
}

static void restart(uint8_t flashKind, uint32_t nowMs) {
  chronoReset();
  s_lastMinute = 0;
  s_flashKind = flashKind;
  s_flashUntil = nowMs + 900;
  chronoUiTriggerFlip();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  EDGE HOURGLASS CHRONOMETER");
  Serial.println("==========================================");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  bool imu = motionBegin();

  chronoUiInit();
  chronoUiSetTiltHint(imu);
  chronoReset();

  ChronoView v = buildView(millis());
  chronoUiFullRedraw(v);

  s_lastFrame = millis();
  Serial.println("[System] Running. Tap = pause, hold = reset, quarter turn = restart.");
}

#if BOARD_DEBUG
// Repeats the bring-up picture so a diagnosis never depends on catching the
// boot window on a port that re-enumerates. Set MOTION_DEBUG to 0 to remove.
static void periodicDiagnostics(uint32_t nowMs) {
  static uint32_t last = 0;
  if (nowMs - last < 15000) return;
  last = nowMs;
  i2cScan("periodic");
  Serial.printf("[Diag] touch=%s imu=%s orientation=%d uptime=%lus\n",
                touchAvailable() ? "ok" : "FAILED",
                motionAvailable() ? "ok" : "FAILED",
                motionOrientation(), (unsigned long)(millis() / 1000));
}
#endif

void loop() {
  uint32_t nowMs = millis();
#if BOARD_DEBUG
  periodicDiagnostics(nowMs);
#endif

  // 1. Touch: short tap toggles pause, long press resets. Polling at 50 Hz is
  //    plenty for taps and keeps the shared I2C bus quiet for the IMU.
  if (nowMs - s_lastTouchPoll >= 20) {
    s_lastTouchPoll = nowMs;
    TouchPoint pt = readTouchInput();
    if (pt.isPressed) {
      if (!s_touchDown) {
        s_touchDown = true;
        s_touchStart = nowMs;
        s_holdFired = false;
      } else if (!s_holdFired && nowMs - s_touchStart >= TOUCH_HOLD_RESET_MS) {
        s_holdFired = true;
        restart(CH_RESET, nowMs);
      }
    } else if (s_touchDown) {
      s_touchDown = false;
      if (!s_holdFired && nowMs - s_touchStart >= 40) chronoTogglePause();
    }
  }

  // 2. Orientation. Rotation is applied before the restart repaint so the
  //    flip animation is drawn in the new frame rather than the old one.
  bool turned = motionPoll();

#if SCREEN_FOLLOWS_DEVICE
  // The whole UI turns with the device so the sand always falls downwards.
  // The panel has no hardware rotation; chrono_ui does it in software. A -1
  // orientation means the board is lying flat, in which case the last known
  // rotation is kept rather than guessed at.
  {
    int8_t q = motionOrientation();
    if (q >= 0) chronoUiSetRotation((uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3));
  }
#endif

  // A settled quarter turn also flips the glass and starts over.
  if (turned) restart(CH_FLIPPED, nowMs);

  // 3. Draw.
  if (nowMs - s_lastFrame >= FRAME_INTERVAL_MS) {
    s_lastFrame = nowMs;
    ChronoView v = buildView(nowMs);

#if BOARD_DEBUG
    // Hardware status straight on the panel, so bring-up can be verified
    // without a serial connection. Set MOTION_DEBUG to 0 to remove.
    {
      static char diag[42];
      // Q is the raw orientation quadrant, R the applied screen rotation.
      snprintf(diag, sizeof(diag), "TP:%s IMU:%s Q:%d R:%u",
               touchModelName(), motionAvailable() ? "OK" : "--",
               motionOrientation(), chronoUiRotation());
      chronoUiSetDiagLine(diag);
    }
#endif

    // Every whole minute the sand has finished running through: turn it over.
    uint32_t minute = (uint32_t)(elapsedUs() / 60000000ULL);
    if (!s_paused && minute != s_lastMinute) {
      s_lastMinute = minute;
      chronoUiTriggerFlip();
    }

    chronoUiRender(v);
  }

  delay(2);
}
