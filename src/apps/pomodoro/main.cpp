// =========================================================================
//  POMODORO
//
//  Starts a 25-minute WORK phase the moment it boots. No setup screen, no
//  settings menu -- the whole point is one less thing to think about.
//
//  WORK -> BREAK -> WORK -> BREAK -> WORK -> BREAK -> WORK -> LONG BREAK -> repeat
//  (SET_SIZE work phases per set; see app_config.h)
//
//  A single ring traces the whole screen border and drains as the phase
//  counts down. At zero: a short chime, a bright sweep, and the next phase
//  starts immediately -- no interaction needed to keep it running.
//
//  Tap = pause/resume. Hold ~1.2s = reset the current phase to full time.
//  Turn the device = the UI rotates to follow (inherited from src/common).
// =========================================================================
#include <Arduino.h>
#include <esp_timer.h>
#include "app_config.h"
#include "display_driver.h"
#include "gfx_view.h"
#include "motion.h"
#include "audio.h"
#include "app_switch.h"
#include "pomodoro_ui.h"

// ---- Phase timing --------------------------------------------------------
static uint8_t  s_phase = PH_WORK;
static uint8_t  s_setCount = 0;        // completed WORK phases in this set

static uint64_t s_originUs = 0;
static uint64_t s_pausedAtUs = 0;
static bool     s_paused = false;

static inline uint64_t nowUs() { return (uint64_t)esp_timer_get_time(); }

static uint64_t phaseDurationUs(uint8_t phase) {
  uint32_t mins = (phase == PH_BREAK) ? BREAK_MINUTES
                : (phase == PH_LONG_BREAK) ? LONG_BREAK_MINUTES
                : WORK_MINUTES;
  return (uint64_t)mins * 60ULL * 1000000ULL;
}

static inline uint64_t elapsedUs() {
  return (s_paused ? s_pausedAtUs : nowUs()) - s_originUs;
}

static uint32_t s_flashUntil = 0;   // brief "RESET" confirmation

static void phaseReset() {          // back to full time, same phase
  s_originUs = nowUs();
  if (s_paused) s_pausedAtUs = s_originUs;
}

// Manual hold-to-reset needs its own visible confirmation: seconds into a
// 25-minute phase, the countdown jumping back to full is easy to miss
// without one, unlike an automatic phase change where the colour and label
// both change too.
static void manualReset(uint32_t nowMs) {
  phaseReset();
  s_flashUntil = nowMs + 700;
  pomodoroUiTriggerTransition();
}

static void togglePause() {
  if (s_paused) {
    s_originUs += nowUs() - s_pausedAtUs;   // give back the paused interval
    s_paused = false;
  } else {
    s_pausedAtUs = nowUs();
    s_paused = true;
  }
}

// One short two-note chime, the same for every transition. A no-op if the
// audio hardware never came up.
static void chime() {
  audioBeep(CHIME_NOTE1_HZ, CHIME_NOTE_MS);
  audioBeep(CHIME_NOTE2_HZ, CHIME_NOTE_MS);
}

static void advancePhase() {
  if (s_phase == PH_WORK) {
    s_setCount++;
    s_phase = (s_setCount >= SET_SIZE) ? PH_LONG_BREAK : PH_BREAK;
  } else {
    if (s_phase == PH_LONG_BREAK) s_setCount = 0;   // fresh set starts now
    s_phase = PH_WORK;
  }
  s_originUs = nowUs();
  chime();
  pomodoroUiTriggerTransition();
}

// ---- Input state ----------------------------------------------------------
static bool     s_touchDown = false;
static uint32_t s_touchStart = 0;
static bool     s_holdFired = false;
static uint32_t s_lastTouchPoll = 0;

static PomodoroView buildView(uint32_t nowMs) {
  uint64_t duration = phaseDurationUs(s_phase);
  uint64_t elapsed = elapsedUs();
  uint64_t remaining = (elapsed >= duration) ? 0 : (duration - elapsed);
  uint64_t remSec = remaining / 1000000ULL;

  PomodoroView v;
  v.minutes = (uint8_t)min<uint64_t>(remSec / 60, 99);
  v.seconds = (uint8_t)(remSec % 60);
  v.fraction = duration ? (float)remaining / (float)duration : 0.0f;
  v.phase = s_phase;
  v.setCount = s_setCount;

  if (nowMs < s_flashUntil) v.status = PM_TRANSITION;
  else if (s_touchDown && !s_holdFired && nowMs - s_touchStart > 350) v.status = PM_HOLD;
  else v.status = s_paused ? PM_PAUSED : PM_RUNNING;

  return v;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  POMODORO");
  Serial.println("==========================================");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  motionBegin();
  if (!audioBegin()) Serial.println("[Audio] unavailable - running silent.");

  pomodoroUiInit();
  s_originUs = nowUs();

  PomodoroView v = buildView(millis());
  pomodoroUiFullRedraw(v);

  Serial.printf("[System] Running. WORK %u min / BREAK %u min / LONG %u min, set of %u.\n",
               (unsigned)WORK_MINUTES, (unsigned)BREAK_MINUTES,
               (unsigned)LONG_BREAK_MINUTES, (unsigned)SET_SIZE);
}

void loop() {
  uint32_t nowMs = millis();

  // 1. Touch: short tap toggles pause; long press resets the current phase.
  // A hold in the reserved top-left corner goes back to the launcher instead;
  // touches inside it never reach this app's own gesture handling.
  if (nowMs - s_lastTouchPoll >= 20) {
    s_lastTouchPoll = nowMs;
    TouchPoint pt = readTouchInput();
    if (appSwitchPollHome(pt, nowMs)) appSwitchGoHome();

    if (pt.isPressed && !appSwitchInHomeCorner(pt)) {
      if (!s_touchDown) {
        s_touchDown = true;
        s_touchStart = nowMs;
        s_holdFired = false;
      } else if (!s_holdFired && nowMs - s_touchStart >= TOUCH_HOLD_RESET_MS) {
        s_holdFired = true;
        manualReset(nowMs);
      }
    } else if (s_touchDown) {
      s_touchDown = false;
      if (!s_holdFired && nowMs - s_touchStart >= 40) togglePause();
    }
  }

  // 2. Orientation: the UI rotates to follow the device.
  motionPoll();
#if SCREEN_FOLLOWS_DEVICE
  {
    int8_t q = motionOrientation();
    if (q >= 0) pomodoroUiSetRotation((uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3));
  }
#endif

  // 3. Phase countdown -- auto-continues with no interaction required.
  if (!s_paused && elapsedUs() >= phaseDurationUs(s_phase)) {
    advancePhase();
  }

  // 4. Draw.
  static uint32_t lastFrame = 0;
  if (nowMs - lastFrame >= FRAME_INTERVAL_MS) {
    lastFrame = nowMs;
    pomodoroUiRender(buildView(nowMs));
  }

  delay(2);
}
