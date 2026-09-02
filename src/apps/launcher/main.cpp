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

static int16_t rowTop(int i, int16_t rowH) { return TITLE_H + i * rowH; }

static void drawRow(int i, int16_t rowH, bool valid, bool pressed) {
  int16_t y = rowTop(i, rowH);
  vFillRect(0, y, viewW(), rowH, pressed ? C_ROW_HIT : C_ROW);
  vDrawTextCentered(APP_MANIFEST[i].label, y + rowH / 2 - 4, 2,
                    valid ? C_LABEL : C_DIM);
  if (!valid) vDrawTextCentered("(not installed)", y + rowH / 2 + 14, 1, C_DIM);
  vFillRect(0, y + rowH - 1, viewW(), 1, C_RULE);
}

static void drawAll(int pressedRow) {
  int16_t rowH = (viewH() - TITLE_H - HINT_H) / APP_MANIFEST_COUNT;
  vFillScreen(C_BG);
  vDrawTextCentered("SELECT APP", 14, 2, C_TITLE);
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

  drawAll(-1);
}

void loop() {
  uint32_t nowMs = millis();

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
