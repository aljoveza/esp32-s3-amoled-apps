# ESP32-S3 AMOLED 1.8 — app collection

Apps for the **Waveshare ESP32-S3-Touch-AMOLED-1.8 (V2)**, 368 × 448 AMOLED,
built on one shared board layer so a new app starts at "draw something"
rather than at "why is the screen black".

```bash
./flash.sh            # menu: pick an app, build it, flash it
```

## Apps

| App | What it is |
|---|---|
| [`hourglass`](src/apps/hourglass/) | Chronometer whose sand runs around the whole screen border. Starts at zero on power-up; tap to pause, hold to reset, turn the device to restart. |
| [`boardtest`](src/apps/boardtest/) | Hardware self-test. I2C map, touch model + live coordinates, IMU vector, orientation — all on the panel, no serial cable. |

## Layout

```
├── flash.sh              interactive build/flash menu
├── platformio.ini        one [env:...] per app
├── include/
│   ├── board_config.h    board revision, brightness, IMU thresholds  ← shared
│   └── pin_config.h      pin map
├── src/
│   ├── common/           shared board layer, compiled into every app
│   │   ├── display_driver.*   panel + touch bring-up
│   │   ├── motion.*           QMI8658 orientation
│   │   └── gfx_view.*         software rotation, text, seven-segment
│   └── apps/
│       ├── hourglass/    app_config.h + main.cpp + its own UI
│       └── boardtest/
├── docs/HARDWARE.md      what this board actually is, and four bring-up traps
├── lib/                  vendor libraries (Arduino_GFX, DriveBus, SensorLib…)
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

3. `./flash.sh` picks it up automatically.

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
./flash.sh hourglass      # straight to one app
./flash.sh -b boardtest   # build only, do not flash
pio run -e hourglass -t upload    # plain PlatformIO, if you prefer
```

`flash.sh` refuses to flash a build that failed, finds the port itself, and
tells you which process is holding the port if one is.

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
