#pragma once
#include <Arduino.h>

// =========================================================================
//  GoPro remote — BLE central role
//
//  Scans for a GoPro (HERO8 Black or newer -- the "Open GoPro" BLE command
//  set), connects, secures the link (the camera ignores commands over a
//  plain, unencrypted connection -- see docs/HARDWARE.md), then writes its
//  native shutter/record-start command on goproModeTrigger().
//
//  The camera has to be reachable over BLE already: the first time ever,
//  that means putting it into pairing mode on the camera itself (Settings
//  > Connections > Connect Device). This app can't do that part for you.
// =========================================================================

enum GoproLinkState : uint8_t {
  GOPRO_SCANNING,
  GOPRO_CONNECTING,
  GOPRO_PAIRING,
  GOPRO_CONNECTED,
  GOPRO_FAILED,   // scan/connect/pairing timed out -- retries on its own after a pause
};

void goproModeBegin();                 // brings up BLE and starts scanning
void goproModeUpdate(uint32_t nowMs);  // call every loop() -- drives connect/timeout/retry
GoproLinkState goproModeState();
const char *goproModeDeviceName();     // "" until a camera is found
bool goproModeIsRecording();           // local shutter-toggle state, for the UI's label
void goproModeTrigger();               // toggles shutter/record; no-op unless GOPRO_CONNECTED
