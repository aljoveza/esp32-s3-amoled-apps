// =========================================================================
//  CAM REMOTE
//
//  Remote camera trigger over Bluetooth LE, two modes chosen once on the
//  mode-select screen:
//
//    PHONE -- advertises as a BLE HID "Consumer Control" remote and pulses
//    Volume-Up on tap, the same trick real Bluetooth selfie remotes use.
//
//    GOPRO -- connects as a BLE client to a GoPro (HERO8 Black or newer)
//    and writes its native shutter/record command.
//
//  Only one mode runs at a time -- a bonded GoPro link and an advertising
//  HID peripheral both want the radio to themselves, and switching modes
//  is rare enough that a clean restart into the other role is simpler and
//  more reliable than tearing down and rebuilding the BLE stack in place.
//  Tap the big button to trigger; hold anywhere outside the home corner to
//  go back to mode select (which is just `esp_restart()` -- see
//  docs/LAUNCHER.md on why every app here treats a reboot as cheap).
// =========================================================================
#include <Arduino.h>
#include "app_config.h"
#include "display_driver.h"
#include "gfx_view.h"
#include "motion.h"
#include "app_switch.h"
#include "camremote_ui.h"
#include "camremote_hid.h"
#include "camremote_gopro.h"

static CamRemoteScreen s_screen = CR_SCREEN_SELECT;

static CamRemoteView buildView() {
  CamRemoteView v;
  v.screen = s_screen;
  v.phoneState = (s_screen == CR_SCREEN_PHONE) ? phoneModeState() : PHONE_ADVERTISING;
  v.goproState = (s_screen == CR_SCREEN_GOPRO) ? goproModeState() : GOPRO_SCANNING;
  v.goproName = goproModeDeviceName();
  v.goproRecording = goproModeIsRecording();
  return v;
}

// ---- Input state ------------------------------------------------------------
static bool     s_touchDown = false;
static uint32_t s_touchStart = 0;
static bool     s_holdFired = false;
static uint32_t s_lastTouchPoll = 0;
static int16_t  s_downVX = -1, s_downVY = -1;   // view-space point the current touch started at

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  CAM REMOTE");
  Serial.println("==========================================");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  motionBegin();

  camremoteUiInit();
  camremoteUiFullRedraw(buildView());
}

void loop() {
  uint32_t nowMs = millis();

  if (s_screen == CR_SCREEN_GOPRO) goproModeUpdate(nowMs);

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
        vTouchToView(pt.x, pt.y, &s_downVX, &s_downVY);
      } else if (!s_holdFired && nowMs - s_touchStart >= TOUCH_HOLD_BACK_MS) {
        s_holdFired = true;
        if (s_screen != CR_SCREEN_SELECT) {
          Serial.println("[System] Back to mode select (restarting for a clean radio state).");
          esp_restart();
        }
      }
    } else if (s_touchDown) {
      s_touchDown = false;
      if (!s_holdFired && nowMs - s_touchStart >= 40) {
        // readTouchInput() no longer reports a position once isPressed
        // goes false, so hit-test against where the press *started*
        // (captured above), not this now-released sample.
        int16_t vx = s_downVX, vy = s_downVY;
        switch (s_screen) {
          case CR_SCREEN_SELECT:
            if (camremoteUiHitPhoneRow(vx, vy)) {
              s_screen = CR_SCREEN_PHONE;
              phoneModeBegin();
              camremoteUiFullRedraw(buildView());
            } else if (camremoteUiHitGoproRow(vx, vy)) {
              s_screen = CR_SCREEN_GOPRO;
              goproModeBegin();
              camremoteUiFullRedraw(buildView());
            }
            break;
          case CR_SCREEN_PHONE:
            if (camremoteUiHitTrigger(vx, vy)) phoneModeTrigger();
            break;
          case CR_SCREEN_GOPRO:
            if (camremoteUiHitTrigger(vx, vy)) goproModeTrigger();
            break;
        }
      }
    }
  }

  // 2. Orientation.
  motionPoll();
#if SCREEN_FOLLOWS_DEVICE
  {
    int8_t q = motionOrientation();
    if (q >= 0) camremoteUiSetRotation((uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3));
  }
#endif

  // 3. Draw.
  static uint32_t lastFrame = 0;
  if (nowMs - lastFrame >= FRAME_INTERVAL_MS) {
    lastFrame = nowMs;
    camremoteUiRender(buildView());
  }

  delay(2);
}
