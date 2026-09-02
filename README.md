# ESP32-S3 AMOLED 1.8 — app collection

Apps for the **Waveshare ESP32-S3-Touch-AMOLED-1.8 (V2)**, 368 × 448 AMOLED,
built on one shared board layer so a new app starts at "draw something"
rather than at "why is the screen black".

Each app is its own firmware image, and the board switches between them
**on-device** — tap a row in the launcher menu, or hold the top-left corner
in any app to come back to it. See [`docs/LAUNCHER.md`](docs/LAUNCHER.md)
for how that works.

```bash
./flash.sh all        # first time: bootloader + partitions + every app
./flash.sh             # after that: menu to update one app, or reflash all
```

## Apps

| App | What it is |
|---|---|
| [`hourglass`](src/apps/hourglass/) | Chronometer whose sand runs around the whole screen border. Starts at zero on power-up; tap to pause, hold to reset, turn the device to restart. |
| [`boardtest`](src/apps/boardtest/) | Hardware self-test. I2C map, touch model + live coordinates, IMU vector, orientation — all on the panel, no serial cable. |
| [`pomodoro`](src/apps/pomodoro/) | Minimal pomodoro timer. Auto-cycling work/break ring, chime + sweep on transition, nothing to configure on-device. |
| [`launcher`](src/apps/launcher/) | Home screen / app picker. Boots first; tap an app to open it. |
| [`roundtimer`](src/apps/roundtimer/) | Game round countdown. Tap to start; rounds repeat automatically, with escalating colour/flash/sound as each one nears zero. |

## Layout

```
├── flash.sh                   build/flash menu, slot-aware
├── platformio.ini             one [env:...] per app, all sharing...
├── partitions_launcher.csv    ...this: 6 OTA app slots, one per firmware
├── include/
│   ├── board_config.h    board revision, brightness, IMU/audio/gesture thresholds  ← shared
│   └── pin_config.h      pin map
├── src/
│   ├── common/           shared board layer, compiled into every app
│   │   ├── display_driver.*   panel + touch bring-up
│   │   ├── motion.*           QMI8658 orientation
│   │   ├── gfx_view.*         software rotation, text, seven-segment
│   │   ├── audio.*            ES8311 codec + I2S tone generator
│   │   └── app_switch.*       OTA slot boot-select + corner-hold gesture
│   └── apps/
│       ├── launcher/     home screen / app picker, boots first
│       ├── hourglass/    app_config.h + main.cpp + its own UI
│       ├── boardtest/
│       ├── pomodoro/
│       └── roundtimer/   game round countdown
├── docs/
│   ├── HARDWARE.md       what this board actually is, and bring-up traps
│   └── LAUNCHER.md       how on-device app switching fits together
├── lib/                  vendor libraries (Arduino_GFX, DriveBus, SensorLib, ES8311…)
├── examples_reference/   Waveshare examples — see the warning below
└── firmware_backup/      factory firmware + restore scripts
```

Config is split deliberately: **`include/board_config.h`** describes the
hardware and is shared; each app's **`app_config.h`** describes only that app.
If you find yourself wanting a board fact in an app config, it belongs in
`board_config.h`.

## Adding an app

```bash
mkdir -p src/apps/myapp
```

1. Add `src/apps/myapp/main.cpp`, `app_config.h`, and a `README.md` whose
   first `# ` line is the one-line description (the menu reads it from there).
2. Copy an env block in `platformio.ini`:

   ```ini
   [env:myapp]
   build_src_filter = +<common/> +<apps/myapp/>
   build_flags = ${env.build_flags} -I src/apps/myapp
   ```

3. Give it a slot: add a case to `app_slot()` in `flash.sh` and an entry to
   `APP_MANIFEST` in `src/apps/launcher/manifest.h` (there are five free —
   `slot1`..`slot5`). Both must name the same slot — see `docs/LAUNCHER.md`.
4. `./flash.sh all` to build and provision everything, or `./flash.sh myapp`
   once its slot exists.

A minimal app:

```cpp
#include <Arduino.h>
#include "app_config.h"
#include "display_driver.h"
#include "gfx_view.h"

void setup() {
  initDisplayAndTouch();               // panel + touch, either revision
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  vFillScreen(C565(0, 0, 0));
  vDrawTextCentered("hello", viewH() / 2, 3, C565(255, 255, 255));
}

void loop() {}
```

See [`src/common/README.md`](src/common/README.md) for the full shared API.

## Flashing

```bash
./flash.sh                # menu
./flash.sh all             # bootloader + partitions + every app (first time / reset)
./flash.sh hourglass      # rebuild + reflash just that app's slot
./flash.sh -b boardtest   # build only, do not flash
```

`flash.sh` writes each app to the OTA slot offset read straight out of
`partitions_launcher.csv` — **not** `pio run -t upload`, which always
targets a fixed address regardless of which slot you actually want. It
refuses to flash a build that failed, finds the port itself, and tells you
which process is holding the port if one is.

## Two warnings worth reading once

**`examples_reference/arduino/` does not match this board.** It is the V1 set
(SH8601 panel, FT3168 touch). This board is V2 — CO5300 panel, CST816T touch.
Use `examples_reference/arduino-v2/`.

**Serial can knock the board off the USB bus.** On the ESP32-S3 USB-JTAG port
RTS drives `EN`, so any tool that opens the port and exits without releasing
it parks the chip in reset and the USB device disappears until you reseat the
cable. `pio device monitor` from a real terminal is fine; from a script it is
not. Details in [`docs/HARDWARE.md`](docs/HARDWARE.md).

## Restoring factory firmware

```bash
./restore_factory_v2.sh    # this board (V2)
./restore_factory_v1.sh    # V1 boards
```
