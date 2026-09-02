// =========================================================================
//  LAUNCHER
//
//  Boots first on a fresh chip (blank otadata always defaults to the
//  "launcher" slot). Shows one row per app in manifest.h; tap a row to boot
//  into it. From inside any app, holding the top-left corner for 1.5s comes
//  back here -- see src/common/app_switch.*.
//
//  A row for a slot nothing has been flashed into yet is shown dim and does
//  nothing when tapped, checked live via appSwitchSlotValid() rather than
//  assumed from the manifest.
// =========================================================================
#include <Arduino.h>
#include "app_config.h"
#include "manifest.h"
#include "display_driver.h"
#include "gfx_view.h"
#include "motion.h"
#include "app_switch.h"
#include "battery.h"

static const uint16_t C_BG      = C565(0, 0, 0);
static const uint16_t C_TITLE   = C565(255, 255, 255);
static const uint16_t C_ROW     = C565(20, 20, 20);
static const uint16_t C_ROW_HIT = C565(40, 40, 40);
static const uint16_t C_LABEL   = C565(230, 230, 230);
static const uint16_t C_DIM     = C565(70, 68, 66);
static const uint16_t C_RULE    = C565(40, 40, 40);
static const uint16_t C_HINT    = C565(70, 68, 66);

static const int16_t TITLE_H = 44;
static const int16_t HINT_H  = 24;

static const uint16_t C_BATT_GREEN  = C565(90, 210, 130);   // >80%
static const uint16_t C_BATT_BLUE   = C565(100, 170, 255);  // 20-79%
static const uint16_t C_BATT_YELLOW = C565(255, 220, 60);   // 10-19%
static const uint16_t C_BATT_RED    = C565(255, 90, 90);    // <10%

static uint16_t batteryColor(int8_t pct) {
  if (pct > 80) return C_BATT_GREEN;
  if (pct >= 20) return C_BATT_BLUE;
  if (pct >= 10) return C_BATT_YELLOW;
  return C_BATT_RED;
}

// Just the percentage, colour-coded by charge level -- no icon. A first
// version drew a battery glyph, but text at the size this display can
// actually render legibly (see vDrawText's size clamp in gfx_view.cpp) was
// too wide to fit next to it without overlapping. pct<0 (no PMU found, or
// no battery connected -- this board runs fine on USB alone) draws nothing.
static const int16_t BATT_W = 62;   // reserved width, clears "100%+"

static void drawBatteryBadge(int8_t pct, bool charging) {
  const int16_t right = viewW() - 12, y = 12;
  vFillRect(right - BATT_W, y, BATT_W, 16, C_BG);   // clear any previous reading
  if (pct < 0) return;

  char pctText[8];
  snprintf(pctText, sizeof(pctText), "%d%%%s", pct, charging ? "+" : "");
  int16_t w = vTextWidth(pctText, 2);
  vDrawText(pctText, right - w, y, 2, batteryColor(pct));
}

static int16_t rowTop(int i, int16_t rowH) { return TITLE_H + i * rowH; }

static void drawRow(int i, int16_t rowH, bool valid, bool pressed) {
  int16_t y = rowTop(i, rowH);
  vFillRect(0, y, viewW(), rowH, pressed ? C_ROW_HIT : C_ROW);
  vDrawTextCentered(APP_MANIFEST[i].label, y + rowH / 2 - 4, 2,
                    valid ? C_LABEL : C_DIM);
  if (!valid) vDrawTextCentered("(not installed)", y + rowH / 2 + 14, 1, C_DIM);
  vFillRect(0, y + rowH - 1, viewW(), 1, C_RULE);
}

static int8_t s_battPct = -1;
static bool   s_battCharging = false;

static void drawAll(int pressedRow) {
  int16_t rowH = (viewH() - TITLE_H - HINT_H) / APP_MANIFEST_COUNT;
  vFillScreen(C_BG);
  vDrawTextCentered("SELECT APP", 14, 2, C_TITLE);
  drawBatteryBadge(s_battPct, s_battCharging);
  vFillRect(0, TITLE_H - 1, viewW(), 1, C_RULE);
  for (int i = 0; i < APP_MANIFEST_COUNT; i++)
    drawRow(i, rowH, appSwitchSlotValid(APP_MANIFEST[i].partition), i == pressedRow);
  vDrawTextCentered("tap an app to open it", viewH() - HINT_H + 6, 1, C_HINT);
}

static int8_t   s_downRow = -1;
static int16_t  s_downX = 0, s_downY = 0;
static uint32_t s_lastRotation = 0xFF;

static int rowAt(int16_t vx, int16_t vy) {
  int16_t rowH = (viewH() - TITLE_H - HINT_H) / APP_MANIFEST_COUNT;
  if (vy < TITLE_H || vy >= TITLE_H + rowH * APP_MANIFEST_COUNT) return -1;
  return (vy - TITLE_H) / rowH;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== LAUNCHER ===");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  motionBegin();
  batteryBegin();
  s_battPct = batteryPercent();
  s_battCharging = batteryCharging();

  drawAll(-1);
}

void loop() {
  uint32_t nowMs = millis();

  // Battery doesn't change fast enough to be worth reading every frame --
  // an I2C round trip a few times a minute is plenty, and only the badge
  // itself gets redrawn, not the whole screen.
  static uint32_t lastBattCheck = 0;
  if (batteryAvailable() && nowMs - lastBattCheck >= 5000) {
    lastBattCheck = nowMs;
    int8_t pct = batteryPercent();
    bool charging = batteryCharging();
    if (pct != s_battPct || charging != s_battCharging) {
      s_battPct = pct;
      s_battCharging = charging;
      drawBatteryBadge(s_battPct, s_battCharging);
    }
  }

  motionPoll();
#if SCREEN_FOLLOWS_DEVICE
  {
    int8_t q = motionOrientation();
    if (q >= 0) {
      uint8_t want = (uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3);
      if (want != viewRotation()) {
        viewSetRotation(want);
        drawAll(-1);
      }
    }
  }
#endif

  TouchPoint pt = readTouchInput();
  int16_t vx = -1, vy = -1;
  if (pt.isPressed) vTouchToView(pt.x, pt.y, &vx, &vy);

  if (pt.isPressed) {
    int row = rowAt(vx, vy);
    if (s_downRow == -1 && row >= 0) {
      s_downRow = (int8_t)row;
      s_downX = vx; s_downY = vy;
      drawAll(s_downRow);
    }
  } else if (s_downRow != -1) {
    int row = rowAt(s_downX, s_downY);   // row the press started on
    s_downRow = -1;
    drawAll(-1);
    if (row >= 0 && row < APP_MANIFEST_COUNT) {
      if (appSwitchSlotValid(APP_MANIFEST[row].partition)) {
        appSwitchLaunch(APP_MANIFEST[row].partition);
      } else {
        Serial.printf("[Launcher] '%s' has nothing flashed into it.\n", APP_MANIFEST[row].label);
      }
    }
  }

  delay(FRAME_INTERVAL_MS);
}
