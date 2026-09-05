#pragma once
#include <Arduino.h>

// =========================================================================
//  Phone remote — BLE HID "Consumer Control" peripheral
//
//  Advertises as a plain BLE HID device (Volume Up is the only usage code
//  it knows) so a phone can pair with it exactly like a commercial
//  Bluetooth selfie remote. phoneModeTrigger() pulses the Volume-Up usage,
//  press then release -- most phones' native camera app treats that the
//  same as the physical volume button, which doubles as the shutter.
// =========================================================================

enum PhoneLinkState : uint8_t {
  PHONE_ADVERTISING,
  PHONE_CONNECTED,
};

void phoneModeBegin();   // brings up the BLE HID stack and starts advertising
PhoneLinkState phoneModeState();
void phoneModeTrigger();   // press + release Volume-Up; no-op unless PHONE_CONNECTED
