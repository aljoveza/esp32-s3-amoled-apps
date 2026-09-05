#pragma once
#include <Arduino.h>
#include "camremote_hid.h"
#include "camremote_gopro.h"

// =========================================================================
//  Camera Remote — UI
//
//  Three screens: mode select (two big rows), and one status+trigger
//  screen each for PHONE and GOPRO mode, sharing the same "big button in
//  the middle" layout. Only the parts that actually changed since the last
//  frame are repainted -- state here changes on BLE events, not every
//  frame, so there's no reason to touch pixels that didn't change.
// =========================================================================

enum CamRemoteScreen : uint8_t {
  CR_SCREEN_SELECT,
  CR_SCREEN_PHONE,
  CR_SCREEN_GOPRO,
};

struct CamRemoteView {
  CamRemoteScreen screen;
  PhoneLinkState  phoneState;
  GoproLinkState  goproState;
  const char     *goproName;
  bool            goproRecording;
};

uint8_t camremoteUiRotation();
void camremoteUiSetRotation(uint8_t rot);

void camremoteUiInit();
void camremoteUiFullRedraw(const CamRemoteView &v);
void camremoteUiRender(const CamRemoteView &v);

// Hit-testing against the same layout the renderer draws, in view-space
// coordinates (i.e. already run through vTouchToView).
bool camremoteUiHitPhoneRow(int16_t vx, int16_t vy);   // CR_SCREEN_SELECT only
bool camremoteUiHitGoproRow(int16_t vx, int16_t vy);   // CR_SCREEN_SELECT only
bool camremoteUiHitTrigger(int16_t vx, int16_t vy);    // CR_SCREEN_PHONE / CR_SCREEN_GOPRO
