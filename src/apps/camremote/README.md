# Cam Remote

A Bluetooth LE camera trigger with two modes, picked once from a
mode-select screen on boot: **PHONE** (any phone, via a HID trick) or
**GOPRO** (a HERO8 Black-or-newer, via its native BLE command set). Only
one mode runs at a time — see "Why only one mode at a time" below.

> **Verification status:** this app builds clean and every BLE call is
> checked against NimBLE-Arduino's actual header source (not just its
> docs — see `.pio/libdeps/*/NimBLE-Arduino/src/` after a build). What
> **hasn't** been verified on real hardware yet is the two things that
> only a real phone and a real GoPro can confirm: whether your phone's
> camera app actually treats the emulated Volume-Up as a shutter press,
> and whether the GoPro pairs/bonds and accepts the shutter command as
> expected. Flash it, try both modes against your own devices, and treat
> the first attempt as a debugging session, the same way every other piece
> of hardware on this board was brought up — see `docs/HARDWARE.md`.

## PHONE mode

Advertises as a plain BLE HID "Consumer Control" remote (device name
`ESP32 Cam Remote`). Pair it from your phone's Bluetooth settings like any
other Bluetooth accessory; once connected, tapping **SHUTTER** sends a
Volume-Up press-then-release. Most phones' *native* camera app treats a
hardware volume-key press as the shutter button — that's the same trick
commercial Bluetooth selfie remotes use — but third-party camera apps may
not listen for it. No pairing/security dance is required for this mode; it
accepts the first phone that connects.

## GOPRO mode

Scans for a device advertising a name starting with `GoPro`, connects, and
secures the link (the camera silently ignores commands over a plain,
unencrypted connection — see `docs/HARDWARE.md`). Once connected, tapping
the button toggles the camera's native shutter/record command:
**SHUTTER** (blue) to start — a complete trigger in photo mode, "start
recording" in video mode — and **STOP** (red) to end a recording.

**Before the first connection ever, the camera has to be put into BLE
pairing mode on the camera itself**: Settings → Connections → Connect
Device → (Quik App, or whichever BLE-pairing entry your GoPro's menu
shows). This app can only connect to a camera that's already
discoverable — it can't reach into the camera's own menu for you.

If nothing is found or pairing fails, the screen shows why (`SCANNING...`,
`PAIRING...`, `NOT FOUND - RETRYING`) and retries on its own after a couple
of seconds — no need to back out and re-enter the mode.

## Why only one mode at a time

A HID peripheral (advertising, waiting for a phone) and a secured GATT
client (bonded to a GoPro) both want the BLE radio and NimBLE's connection
tracking to themselves. Tearing down one role and standing up the other
mid-run is possible but fiddly to get reliably right; a full reboot into
the freshly-chosen role is simpler and costs about a second, which this
project already treats as a non-issue for switching *apps* entirely (see
`docs/LAUNCHER.md`) — doing the same within one app for a mode change is a
smaller version of the same tradeoff.

## Controls

| Gesture | Effect |
|---|---|
| **Tap a mode row** (mode-select screen) | Enter that mode, bring up BLE for it |
| **Tap the big button** | Trigger (Volume-Up pulse / GoPro shutter toggle) |
| **Hold anywhere** (outside the home corner) | Back to mode select — `esp_restart()`, for a clean radio state |
| **Hold the top-left corner** | Back to the launcher, as in every app |

## Configuration — `app_config.h`

| Setting | Default | Notes |
|---|---|---|
| `BLE_DEVICE_NAME` | `"ESP32 Cam Remote"` | name PHONE mode advertises as |
| `HID_PULSE_MS` | `40` | press-to-release gap for the emulated Volume-Up |
| `GOPRO_NAME_PREFIX` | `"GoPro"` | how a scan result is recognized as a GoPro |
| `GOPRO_SCAN_S` | `15` | scan window per attempt |
| `GOPRO_CONNECT_TIMEOUT_MS` | `8000` | give up on a stuck connect/pair and retry |
| `GOPRO_RETRY_DELAY_MS` | `2000` | pause before re-scanning after a failure |
| `TOUCH_HOLD_BACK_MS` | `900` | hold-to-return-to-mode-select threshold |

## Implementation notes

Uses `h2zero/NimBLE-Arduino` (v2.x, for arduino-esp32 core 3.x/ESP-IDF 5.x
compatibility — the 1.x line doesn't build against this project's core),
pulled in via `lib_deps` on this app's own `platformio.ini` env rather than
vendored, since PlatformIO's registry fetch works fine here and there's no
board-specific patch needed the way the vendored libs under `lib/` have.
Every NimBLE class/method this app calls was checked against the library's
actual installed header source, not just its docs site (which returned
several stale 404s during development) — worth doing again if a future
NimBLE-Arduino upgrade changes an API this app depends on.

GoPro's BLE GATT layout isn't published as a plain UUID table by GoPro
itself; `camremote_gopro.cpp`'s service/characteristic UUIDs come from
cross-checking the community-maintained
[goprowifihack](https://github.com/KonradIT/goprowifihack/blob/master/Bluetooth/bluetooth-api.md)
reference against GoPro's own [Open GoPro](https://gopro.github.io/OpenGoPro/)
tutorials. If a future GoPro firmware update changes this and commands
stop working, that's the first place to re-verify against.
