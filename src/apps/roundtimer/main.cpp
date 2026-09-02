// =========================================================================
//  ROUND TIMER
//
//  A countdown timer for a game: configure the round length once, tap to
//  start, and it keeps timing rounds on its own -- each time one hits zero,
//  it alarms and the next round starts automatically, forever, until you
//  hold to stop.
//
//  Ring drains around the whole border as the round counts down. In the
//  last WARNING_SEC seconds it turns amber with a spinning highlight; in
//  the last DANGER_SEC it turns red, the screen pulses on every second, and
//  a rising-pitch tick plays each second. At zero: a two-tone alarm and a
//  bright sweep, then the next round begins.
//
//  While idle, the screen is three zones: left/right nudge the configured
//  round length up or down, the middle starts. Once running, tap anywhere
//  is pause/resume, and hold anywhere is stop -- back to idle. Adjustments
//  aren't persisted, same as every other piece of state in this app: it's
//  back to the app_config.h default on every power-up.
//
//  Turning the device rotates the UI to follow.
// =========================================================================
#include <Arduino.h>
#include <esp_timer.h>
#include "app_config.h"
#include "display_driver.h"
#include "gfx_view.h"
#include "motion.h"
#include "audio.h"
#include "app_switch.h"
#include "roundtimer_ui.h"

static uint8_t  s_state = RT_ARMED;
static uint32_t s_roundSeconds = ROUND_SECONDS;   // adjustable while idle
static uint64_t s_originUs = 0;
static uint64_t s_pausedAtUs = 0;
static uint32_t s_roundNumber = 0;
static uint32_t s_alarmStartMs = 0;
static int16_t  s_lastTickSecond = -1;

static inline uint64_t nowUs() { return (uint64_t)esp_timer_get_time(); }

static inline uint64_t elapsedUs() {
  return (s_state == RT_PAUSED ? s_pausedAtUs : nowUs()) - s_originUs;
}

static void startRound() {
  s_originUs = nowUs();
  s_roundNumber++;
  s_lastTickSecond = -1;
  s_state = RT_RUNNING;
}

static void togglePause() {
  if (s_state == RT_PAUSED) {
    s_originUs += nowUs() - s_pausedAtUs;
    s_state = RT_RUNNING;
  } else if (s_state == RT_RUNNING) {
    s_pausedAtUs = nowUs();
    s_state = RT_PAUSED;
  }
}

static void stopToIdle() {
  s_state = RT_ARMED;
}

static void adjustRoundSeconds(int32_t delta) {
  int32_t v = (int32_t)s_roundSeconds + delta;
  if (v < ROUND_SECONDS_MIN) v = ROUND_SECONDS_MIN;
  if (v > ROUND_SECONDS_MAX) v = ROUND_SECONDS_MAX;
  s_roundSeconds = (uint32_t)v;
}

static void triggerAlarm() {
  s_state = RT_ALARM;
  s_alarmStartMs = millis();
  roundtimerUiTriggerAlarm();
  audioBeep(ALARM_HZ_1, ALARM_MS);
  audioBeep(ALARM_HZ_2, ALARM_MS);
}

static RoundTimerView buildView() {
  RoundTimerView v;
  v.state = s_state;
  v.roundNumber = s_roundNumber;

  if (s_state == RT_ARMED) {
    v.secondsRemaining = (uint16_t)s_roundSeconds;
    v.fraction = 1.0f;
  } else if (s_state == RT_ALARM) {
    v.secondsRemaining = 0;
    v.fraction = 0.0f;
  } else {
    uint64_t elapsed = elapsedUs();
    uint64_t total = (uint64_t)s_roundSeconds * 1000000ULL;
    uint64_t remaining = (elapsed >= total) ? 0 : (total - elapsed);
    v.secondsRemaining = (uint16_t)((remaining + 999999ULL) / 1000000ULL); // round up
    if (v.secondsRemaining > s_roundSeconds) v.secondsRemaining = (uint16_t)s_roundSeconds;
    v.fraction = (float)remaining / (float)total;
  }
  return v;
}

// ---- Input state ------------------------------------------------------------
static bool     s_touchDown = false;
static uint32_t s_touchStart = 0;
static bool     s_holdFired = false;
static uint32_t s_lastTouchPoll = 0;
static RTZone   s_downZone = RT_ZONE_NONE;   // which idle zone the current touch started in

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  ROUND TIMER");
  Serial.println("==========================================");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  motionBegin();
  if (!audioBegin()) Serial.println("[Audio] unavailable - running silent.");

  roundtimerUiInit();
  roundtimerUiFullRedraw(buildView());

  Serial.printf("[System] Running. %us rounds by default (adjustable while idle).\n",
               (unsigned)ROUND_SECONDS);
}

void loop() {
  uint32_t nowMs = millis();

  // 1. Touch.
  if (nowMs - s_lastTouchPoll >= 20) {
    s_lastTouchPoll = nowMs;
    TouchPoint pt = readTouchInput();
    if (appSwitchPollHome(pt, nowMs)) appSwitchGoHome();

    if (pt.isPressed && !appSwitchInHomeCorner(pt)) {
      if (!s_touchDown) {
        s_touchDown = true;
        s_touchStart = nowMs;
        s_holdFired = false;
        if (s_state == RT_ARMED) {
          int16_t vx, vy;
          vTouchToView(pt.x, pt.y, &vx, &vy);
          s_downZone = roundtimerUiZoneAt(vx, vy);
        } else {
          s_downZone = RT_ZONE_NONE;
        }
      } else if (!s_holdFired && nowMs - s_touchStart >= TOUCH_HOLD_RESET_MS) {
        s_holdFired = true;
        stopToIdle();
      }
    } else if (s_touchDown) {
      s_touchDown = false;
      if (!s_holdFired && nowMs - s_touchStart >= 40) {
        if (s_state == RT_ARMED) {
          switch (s_downZone) {
            case RT_ZONE_DECREASE: adjustRoundSeconds(-(int32_t)ROUND_SECONDS_STEP); break;
            case RT_ZONE_INCREASE: adjustRoundSeconds((int32_t)ROUND_SECONDS_STEP); break;
            case RT_ZONE_START:    startRound(); break;
            default: break;
          }
        } else if (s_state == RT_RUNNING || s_state == RT_PAUSED) {
          togglePause();
        }
        // A tap during RT_ALARM is ignored -- the hold-off is short and the
        // next round is already coming.
      }
    }
  }

  // 2. Orientation.
  motionPoll();
#if SCREEN_FOLLOWS_DEVICE
  {
    int8_t q = motionOrientation();
    if (q >= 0) roundtimerUiSetRotation((uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3));
  }
#endif

  // 3. Round logic.
  if (s_state == RT_RUNNING) {
    uint64_t elapsed = elapsedUs();
    uint64_t total = (uint64_t)s_roundSeconds * 1000000ULL;
    if (elapsed >= total) {
      triggerAlarm();
    } else {
      // Same ceiling-division formula buildView() uses for the displayed
      // digits, so the tick always fires exactly when the shown number
      // changes rather than up to a second early or late.
      uint32_t secsLeft = (uint32_t)((total - elapsed + 999999ULL) / 1000000ULL);
      if (secsLeft <= DANGER_SEC && (int16_t)secsLeft != s_lastTickSecond) {
        s_lastTickSecond = (int16_t)secsLeft;
        uint16_t hz = TICK_HZ_BASE + (DANGER_SEC - secsLeft) * TICK_HZ_STEP;
        audioBeep(hz, TICK_MS);
      }
    }
  } else if (s_state == RT_ALARM) {
    if (nowMs - s_alarmStartMs >= ALARM_HOLD_MS) startRound();
  }

  // 4. Draw.
  static uint32_t lastFrame = 0;
  if (nowMs - lastFrame >= FRAME_INTERVAL_MS) {
    lastFrame = nowMs;
    roundtimerUiRender(buildView());
  }

  delay(2);
}
