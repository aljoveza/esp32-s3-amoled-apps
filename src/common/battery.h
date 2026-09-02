#pragma once
#include <Arduino.h>

// =========================================================================
//  Battery — AXP2101 power management chip (I2C 0x34).
//
//  Never used anywhere in this project before this module -- built
//  defensively like audio.h: batteryBegin() returns false rather than
//  blocking anything if the chip doesn't answer, and every accessor is
//  always safe to call, returning "unavailable" values quietly instead of
//  crashing. lib/XPowersLib (vendored, gitignored like the other
//  third-party libs -- see lib/README) provides the actual driver; nothing
//  outside this file includes it directly.
// =========================================================================

bool batteryBegin();
bool batteryAvailable();

// -1 if unavailable or no battery is connected (this board can also run
// purely on USB power with no battery attached).
int8_t batteryPercent();

// False if unavailable -- distinct from "available but not charging".
bool batteryCharging();
