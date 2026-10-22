// =========================================================================
//  CALENDAR
//
//  Reads the primary Google Calendar directly over Wi-Fi/HTTPS (no backend
//  server -- see google_calendar.h and SETUP.md) and shows the event
//  running right now plus how long it's been going, and the next event plus
//  how long until it starts. The UI rotates to follow the device, same as
//  pomodoro/hourglass (src/common/motion.h).
//
//  Boots through three phases: connect Wi-Fi, sync time over NTP (this
//  board has no battery-backed RTC), then run -- polling the calendar on
//  an interval and ticking the on-screen durations once a second in
//  between, no network call needed for the tick itself. Once running, a
//  failed poll never blanks the screen back to a status message: the last
//  known agenda just keeps ticking (possibly stale) until the next poll
//  succeeds.
//
//  Plugged in, Wi-Fi stays connected and polls every POLL_INTERVAL_PLUGGED_MS.
//  On battery, the radio is switched off between polls and the backlight
//  dims -- polls are less frequent (POLL_INTERVAL_BATTERY_MS) and each one
//  pays a short reconnect cost, trading freshness for battery life.
// =========================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "app_config.h"
#include "secrets.h"
#include "display_driver.h"
#include "app_switch.h"
#include "battery.h"
#include "motion.h"
#include "google_calendar.h"
#include "calendar_ui.h"

enum Phase : uint8_t { PHASE_WIFI, PHASE_TIME, PHASE_RUNNING };

static Phase       s_phase = PHASE_WIFI;
static uint32_t    s_phaseAttemptAt = 0;
static uint32_t    s_retryAt = 0;
static const char *s_statusLine = "Connecting Wi-Fi...";

// ---- RUNNING-phase polling / battery-aware radio duty-cycling -------------
static uint32_t s_nextPollAt = 0;
static bool     s_radioOff = false;
static bool     s_reconnecting = false;
static uint32_t s_reconnectStart = 0;

static bool     s_hasCurrent = false, s_hasNext = false;
static CalEvent s_current, s_next;

static bool s_brightnessOnBattery = false;   // which brightness level is active

static void wifiStart() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  s_phaseAttemptAt = millis();
}

static void timeStart() {
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
  s_phaseAttemptAt = millis();
}

// A fresh boot's clock reads as 1970 until NTP lands -- anything comfortably
// past this project's start date means the sync actually happened.
static bool timeIsSynced() { return time(nullptr) > 1700000000; }

static void runStartupPhases(uint32_t nowMs) {
  switch (s_phase) {
    case PHASE_WIFI:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Calendar] Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
        s_phase = PHASE_TIME;
        s_retryAt = 0;
        timeStart();
        s_statusLine = "Syncing time...";
      } else if (s_retryAt != 0) {
        // Already timed out once -- just waiting out the backoff, nothing
        // to do each loop tick until it elapses (see the branch below,
        // which used to re-run every tick here and spam WiFi.disconnect()).
        if (nowMs >= s_retryAt) { s_retryAt = 0; wifiStart(); }
      } else if (nowMs - s_phaseAttemptAt >= WIFI_CONNECT_TIMEOUT_MS) {
        Serial.printf("[Calendar] Wi-Fi connect timed out (status=%d), retrying in %lums\n",
                     (int)WiFi.status(), (unsigned long)WIFI_RETRY_BACKOFF_MS);
        WiFi.disconnect(true);
        s_statusLine = "Wi-Fi failed, retrying...";
        s_retryAt = nowMs + WIFI_RETRY_BACKOFF_MS;
      }
      break;

    case PHASE_TIME:
      if (timeIsSynced()) {
        Serial.printf("[Calendar] Time synced: epoch %ld\n", (long)time(nullptr));
        s_phase = PHASE_RUNNING;
        s_nextPollAt = nowMs;   // fetch right away
      } else if (nowMs - s_phaseAttemptAt >= NTP_SYNC_TIMEOUT_MS) {
        Serial.println("[Calendar] NTP sync timed out, retrying");
        s_statusLine = "Time sync failed, retrying...";
        timeStart();
      }
      break;

    case PHASE_RUNNING:
      break;   // handled separately, see runPolling()
  }
}

static void runPolling(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED && !s_radioOff && !s_reconnecting) {
    // Dropped outside of a deliberate radio-off window -- start reconnecting
    // right away rather than waiting for the next scheduled poll.
    wifiStart();
    s_reconnectStart = nowMs;
    s_reconnecting = true;
  }

  if (s_reconnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      s_radioOff = false;
      s_reconnecting = false;
    } else if (nowMs - s_reconnectStart >= WIFI_CONNECT_TIMEOUT_MS) {
      s_reconnecting = false;
      s_radioOff = true;   // still down -- try again on the next scheduled poll
      s_nextPollAt = nowMs + WIFI_RETRY_BACKOFF_MS;
      Serial.println("[Calendar] Wi-Fi reconnect timed out, will retry later");
    }
    return;
  }

  if (nowMs < s_nextPollAt) return;

  if (s_radioOff) {
    wifiStart();
    s_reconnectStart = nowMs;
    s_reconnecting = true;
    return;
  }

  Serial.println("[Calendar] polling agenda...");
  bool ok = gcalFetchAgenda(&s_current, &s_hasCurrent, &s_next, &s_hasNext);
  Serial.printf("[Calendar] poll result: ok=%d hasCurrent=%d hasNext=%d\n",
               ok, s_hasCurrent, s_hasNext);
  if (!ok) Serial.println("[Calendar] agenda fetch failed, keeping last known agenda");

  bool charging = !batteryAvailable() || batteryCharging();
  s_nextPollAt = nowMs + (charging ? POLL_INTERVAL_PLUGGED_MS : POLL_INTERVAL_BATTERY_MS);
  if (!charging) {
    WiFi.disconnect(true);
    s_radioOff = true;
  }
}

static CalendarView buildView() {
  CalendarView v{};
  v.battPct = batteryAvailable() ? batteryPercent() : -1;
  v.battCharging = batteryAvailable() && batteryCharging();

  if (s_phase != PHASE_RUNNING) {
    v.statusLine = s_statusLine;
    return v;
  }

  time_t now = time(nullptr);
  v.hasCurrent = s_hasCurrent;
  if (s_hasCurrent) {
    v.currentTitle = s_current.title;
    v.elapsedSec = (now > s_current.start) ? (uint32_t)(now - s_current.start) : 0;
    v.totalSec = (s_current.end > s_current.start) ? (uint32_t)(s_current.end - s_current.start) : 0;
  }
  v.hasNext = s_hasNext;
  if (s_hasNext) {
    v.nextTitle = s_next.title;
    v.remainingSec = (s_next.start > now) ? (uint32_t)(s_next.start - now) : 0;
  }
  return v;
}

static uint32_t s_lastTouchPoll = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  CALENDAR");
  Serial.println("==========================================");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS_PLUGGED);
  batteryBegin();
  motionBegin();

  calendarUiInit();
  calendarUiFullRedraw(buildView());

  wifiStart();
}

void loop() {
  uint32_t nowMs = millis();

  // Touch: this is a read-only dashboard, so the only gesture is the shared
  // hold-corner-to-go-home one -- nothing app-specific to handle otherwise.
  if (nowMs - s_lastTouchPoll >= 20) {
    s_lastTouchPoll = nowMs;
    TouchPoint pt = readTouchInput();
    if (appSwitchPollHome(pt, nowMs)) appSwitchGoHome();
  }

  if (s_phase == PHASE_RUNNING) runPolling(nowMs);
  else runStartupPhases(nowMs);

  motionPoll();
#if SCREEN_FOLLOWS_DEVICE
  {
    int8_t q = motionOrientation();
    if (q >= 0) calendarUiSetRotation((uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3));
  }
#endif

  bool onBattery = batteryAvailable() && !batteryCharging();
  if (onBattery != s_brightnessOnBattery) {
    s_brightnessOnBattery = onBattery;
    setDisplayBrightness(onBattery ? DISPLAY_BRIGHTNESS_BATTERY : DISPLAY_BRIGHTNESS_PLUGGED);
  }

  static uint32_t lastFrame = 0;
  if (nowMs - lastFrame >= FRAME_INTERVAL_MS) {
    lastFrame = nowMs;
    calendarUiRender(buildView());
  }

  delay(2);
}
