#pragma once

// =========================================================================
//  Which app lives in which OTA slot.
//
//  This is the ONE place the launcher looks to know what to show. It must
//  agree with flash.sh's APP_SLOTS mapping -- that's what actually puts an
//  app's firmware into a given slot. Both are hand-maintained; see
//  docs/LAUNCHER.md before changing either.
//
//  A slot with nothing flashed into it yet is detected at runtime
//  (appSwitchSlotValid) and shown greyed out rather than trusted blindly --
//  this list only says what SHOULD be there, not that it IS.
// =========================================================================

struct AppEntry {
  const char *label;       // shown on the menu row
  const char *partition;   // partition label from partitions_launcher.csv
};

static const AppEntry APP_MANIFEST[] = {
  {"Hourglass",  "slot1"},
  {"Pomodoro",   "slot2"},
  {"Round Timer","slot5"},
  {"Cam Remote", "slot4"},
  {"Board Test", "slot3"},
};
static const int APP_MANIFEST_COUNT = sizeof(APP_MANIFEST) / sizeof(APP_MANIFEST[0]);
