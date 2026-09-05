#pragma once
#include "board_config.h"

// =========================================================================
//  Camera Remote — app configuration
//
//  Two independent BLE roles, picked once per boot from a mode-select
//  screen (see main.cpp) -- this app never runs both at the same time,
//  since a reliable bonded connection to a GoPro and a HID peripheral
//  advertising for a phone want the radio to itself:
//
//    PHONE mode: advertises as a BLE HID "Consumer Control" device (the
//    same trick real Bluetooth selfie remotes use) and pulses the
//    Volume-Up usage code on tap. Most phones' native camera app treats a
//    hardware volume-key press as the shutter button; third-party camera
//    apps may not listen for it.
//
//    GOPRO mode: connects as a BLE *client* to a GoPro (HERO8 Black or
//    newer -- the "Open GoPro" BLE command set) and writes its native
//    shutter/record command. The camera must already be reachable over BLE
//    -- the first time ever, that means putting it into pairing mode on
//    the camera itself (Settings > Connections > Connect Device); this app
//    can't do that part for you. See docs/HARDWARE.md and this app's
//    README for what was actually verified vs. what still needs it.
// =========================================================================

// ---- Phone / HID ------------------------------------------------------------
#define BLE_DEVICE_NAME             "ESP32 Cam Remote"
#define HID_REPORT_ID               1
#define HID_PULSE_MS                40    // press-then-release gap for the volume key

// ---- GoPro ----------------------------------------------------------------
// GoPro's local BLE name starts with this on every generation checked
// against the Open GoPro docs (HERO8 Black and newer).
#define GOPRO_NAME_PREFIX           "GoPro"
#define GOPRO_SCAN_S                15     // NimBLEScan::start() takes whole seconds
#define GOPRO_CONNECT_TIMEOUT_MS    8000
#define GOPRO_RETRY_DELAY_MS        2000   // pause after a failure before scanning again

// ---- Interaction ------------------------------------------------------------
#define TOUCH_HOLD_BACK_MS          900    // hold anywhere (outside the home corner) = back to mode select
#define FRAME_INTERVAL_MS           40
#define SCREEN_FOLLOWS_DEVICE       1
