#pragma once
#include <Arduino.h>
#include "display_driver.h"

// =========================================================================
//  App switching — OTA-slot boot selection + the shared "hold corner to go
//  home" gesture.
//
//  Every app on this board is its own independent firmware image, living in
//  one of the slots defined in partitions_launcher.csv. Switching apps means
//  telling the bootloader which slot to boot next and rebooting into it --
//  there is no in-process handoff, so app code never needs to know anything
//  about any other app.
//
//  Apps call appSwitchPollHome() from their touch handling and, when it
//  fires, appSwitchGoHome(). The launcher calls appSwitchLaunch() with the
//  partition label of whichever slot the user picked.
// =========================================================================

// True once per corner-hold: pass every touch reading through this before
// your own tap/hold logic runs, and skip your own handling for a touch this
// returns true for or that lands in the reserved corner (see
// appSwitchInHomeCorner) -- otherwise a hold-to-go-home would also register
// as your app's own hold gesture.
bool appSwitchPollHome(TouchPoint pt, uint32_t nowMs);

// True if this touch is inside the reserved corner, regardless of how long
// it's been held. Use this to exclude the corner from your own gestures even
// before the hold threshold fires.
bool appSwitchInHomeCorner(TouchPoint pt);

// Reboots into the "launcher" partition. Never returns.
void appSwitchGoHome();

// Reboots into the named partition (a label from partitions_launcher.csv,
// e.g. "slot1") if it holds a valid app image; returns false and stays put
// otherwise. Never returns on success.
bool appSwitchLaunch(const char *partitionLabel);

// True if the named partition contains a valid app image (checked by
// reading its header, not by trusting a hardcoded list) -- lets the launcher
// grey out a slot nothing has been flashed into yet instead of rebooting
// into blank flash.
bool appSwitchSlotValid(const char *partitionLabel);
