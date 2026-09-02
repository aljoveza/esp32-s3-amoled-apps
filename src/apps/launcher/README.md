# Launcher

Home screen and app picker. Boots first on a fresh chip; tap an app to open
it, or from inside any app hold the top-left corner for 1.5s to come back
here. See [`docs/LAUNCHER.md`](../../../docs/LAUNCHER.md) for how the whole
multi-app setup fits together.

## What it does

Shows one row per entry in `manifest.h`. A row for a slot with nothing
flashed into it yet is shown dim with "(not installed)" and does nothing if
tapped — checked live each frame via `appSwitchSlotValid()`, not assumed
from the manifest.

Tapping a row calls `appSwitchLaunch()`, which reboots the board into that
app's OTA slot. This app never runs alongside another one; every switch is a
reboot (roughly a second or two).

## Adding an app to the menu

1. Give the app a slot in `partitions_launcher.csv` (there are five free:
   `slot1`..`slot5`) and a case in `flash.sh`'s `app_slot()`.
2. Add a line to `APP_MANIFEST` in `manifest.h` with the same slot label.
3. `./flash.sh all` (or just `./flash.sh <app>` if the slot already existed).

Both files have to agree — see `docs/LAUNCHER.md`.
