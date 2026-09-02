# Agent guide — ESP32-S3 AMOLED 1.8 app collection

This file is for AI coding agents (Claude Code, or anything else) working in
this repo. It's operational guidance — the "read this before you touch
anything" facts — not a restatement of the README. Start here, then follow
the links for depth.

## What this project is

Firmware for a **Waveshare ESP32-S3-Touch-AMOLED-1.8 (V2)** board — 368×448
AMOLED, CST816T touch, QMI8658 IMU, AXP2101 battery PMU, ES8311 audio codec —
built as **several independent single-purpose apps** (chronometer, pomodoro
timer, game round timer, hardware self-test, a launcher) that share one
board bring-up layer and switch between each other **on-device** via OTA
slots, no PC involved after flashing. See [`README.md`](README.md) for the
full app table and [`docs/LAUNCHER.md`](docs/LAUNCHER.md) for how switching
works.

PlatformIO + Arduino framework. No unit test framework, no CI — every claim
of "this works" in this repo means it was verified on the physical board.

## Quick reference

```bash
pio run                    # build the default env (hourglass)
pio run -e <app>           # build one app; -e boardtest -e pomodoro ... for several
pio run -e <app> -t upload # NOT how apps get flashed here — see flash.sh below
./flash.sh                 # interactive menu: build/flash one app, or all
./flash.sh all              # first-time / reset provisioning: bootloader + partitions + every app
./flash.sh <app>            # rebuild + reflash just that app's OTA slot
./flash.sh -b <app>         # build only, don't flash
```

**Always use `flash.sh`, never `pio run -t upload`, to put firmware on the
board.** `-t upload` always targets a fixed address; this project's apps
live at different OTA slot offsets, which `flash.sh` reads out of
`partitions_launcher.csv` for you.

**Reflashing an app's slot does not switch the board to it.** The board
keeps booting whatever it last booted (that state lives in the `otadata`
flash partition, survives power-off). To see a change take effect, either
navigate to the app on-device, or reflash `launcher` and use it to switch,
or `./flash.sh all` to reset boot state to the launcher.

**Changes to `src/apps/launcher/manifest.h` require reflashing `launcher`
itself** — reflashing the changed app's own slot does nothing for the menu
that lists it.

## Repository layout

```
include/board_config.h      shared hardware facts (board revision, IMU/audio thresholds, brightness)
include/pin_config.h        pin map
src/common/                 shared board layer, compiled into every app — see src/common/README.md
src/apps/<name>/            one app: main.cpp, app_config.h, README.md (first `#` line = menu blurb)
docs/HARDWARE.md            hardware bring-up facts and traps, several expensive to (re)discover
docs/LAUNCHER.md            OTA slot switching design
partitions_launcher.csv     6 OTA slots: launcher + slot1..slot5
platformio.ini              one [env:<app>] per app, sharing a base [env]
flash.sh                    the actual build/flash tool (see above)
examples_reference/         vendor examples — arduino/ is V1 (wrong board!), arduino-v2/ is this board
firmware_backup/, restore_factory_v*.sh   stock firmware + restore
```

## Architecture in one paragraph

Every app is a complete, independent Arduino sketch (`setup()`/`loop()`,
file-scope statics, no namespacing) that happens to share `src/common/` for
board bring-up. Apps do not know about each other and cannot affect each
other — the tradeoff for that isolation is a ~1-2s reboot when switching
apps, since switching means "tell the bootloader which OTA slot to boot into
next, then reboot" (`src/common/app_switch.*`), not any kind of in-process
handoff. The `launcher` app is just another app that happens to boot first
and knows how to point the bootloader at the others. Full rationale in
[`docs/LAUNCHER.md`](docs/LAUNCHER.md).

## Critical hardware facts — read before touching display or serial code

Full detail in [`docs/HARDWARE.md`](docs/HARDWARE.md); the headlines:

- **This board is V2** (CO5300 panel, CST816T touch @ I2C `0x15`). The
  vendor examples in `examples_reference/arduino/` are for **V1** (SH8601,
  FT3168 @ `0x38`) and will not work here — use `examples_reference/arduino-v2/`
  as reference instead, and check `BOARD_REVISION_V2` in `include/board_config.h`.
- **The panel cannot draw a fill exactly 1 physical pixel wide** — it
  silently draws nothing, no error. This is why `vFillRect` in
  `src/common/gfx_view.cpp` widens any 1px-wide fill to 2px, and why
  `vDrawText`/`vTextWidth` clamp text to a **minimum size of 2** (true
  size-1 text is unreadable even after the widen-fix, since adjacent glyph
  columns are only 1px apart and bleed together). Don't fight this clamp —
  design layouts assuming 16px-tall text, not 8px.
- **The panel cannot rotate in hardware** (`Arduino_CO5300::setRotation()`
  says so directly). All rotation in this project is done in software via
  `src/common/gfx_view.*` — draw against `viewW()`/`viewH()`, never the raw
  `gfx` object, or your app breaks under rotation.
- **The QMI8658 IMU can report WHO_AM_I correctly while still reading
  `0x8000` (no data) on every axis** if power-down survives reset —
  `motionBegin()` calls `powerOn()` before configuring for exactly this
  reason. Don't "simplify" that order.
- **Audio uses the legacy `driver/i2s.h`, not `ESP_I2S.h`** — the vendor
  example's header isn't present in this project's arduino-esp32 core
  version. `src/common/audio.cpp` is the reference; don't reintroduce
  `ESP_I2S.h`.
- **Serial can knock the board off the USB bus.** RTS drives `EN`, DTR
  drives `IO0` on the USB-Serial-JTAG port. A script that opens the serial
  port without explicitly deasserting both before `open()` will park the
  chip in reset — the USB device vanishes until the cable is reseated.
  `pio device monitor` from a real terminal is fine; scripted serial access
  is not, unless it handles this.

## Adding a new app

Full walkthrough with a minimal `main.cpp` in [`README.md`](README.md#adding-an-app).
In short: create `src/apps/<name>/{main.cpp,app_config.h,README.md}`, add an
`[env:<name>]` block to `platformio.ini`, add the app to **both**
`app_slot()` in `flash.sh` **and** `APP_MANIFEST` in
`src/apps/launcher/manifest.h` (same slot label in both — nothing enforces
they agree, and disagreement means either a phantom launcher row or a build
`flash.sh` can't place). Then `./flash.sh all`.

Config split: **`include/board_config.h`** is hardware facts shared by every
app; each app's own **`app_config.h`** is app-only settings. A board fact
that ends up in an `app_config.h` is misplaced.

## Verification expectations

There's no simulator and no test harness — "done" means confirmed on the
physical board (build succeeds *and* the behavior was actually observed:
readable text, correct colors, gesture/touch response, sound, etc.). When
changing anything visual, treat your own read of the pixel math as a
hypothesis, not a result — this project has been burned by that before (the
1px-fill bug above cost a long debugging session specifically because
size-1 text *looked* like it should work on paper). If you can't flash and
observe the change yourself, say so rather than asserting it works.

Run a full `pio run` across every environment (`pio run -e hourglass -e
boardtest -e pomodoro -e launcher -e roundtimer`, or whatever the current
app list is) before considering a change to shared code (`src/common/`,
`include/`, `platformio.ini`) finished — one app building doesn't mean they
all still do.

## Git conventions used in this repo

- Never commit directly to `main`. Branch first (`git checkout -b
  feat/...`), even for changes the user explicitly asked to "push to main" —
  push the branch, open a PR, merge it.
- When a change bundles multiple unrelated concerns, split it into multiple
  commits grouped by *nature* of the change (e.g. a bug fix separate from
  the new feature it enabled, separate from docs-only cleanup) rather than
  one commit per file or one giant commit. Perfect hunk-level surgical
  separation isn't required — reasonable, file-level grouping by concern is
  the bar.
- Use `gh pr create` / `gh pr merge --merge --delete-branch` rather than
  pushing straight to a protected branch.

## Where to go for more detail

| Question | Read |
|---|---|
| What does each app do, how do I flash one? | [`README.md`](README.md) |
| Why is the board wired up this way, what bit me during bring-up? | [`docs/HARDWARE.md`](docs/HARDWARE.md) |
| How does on-device app switching actually work? | [`docs/LAUNCHER.md`](docs/LAUNCHER.md) |
| What can I call from `src/common/`? | [`src/common/README.md`](src/common/README.md) |
| What does a specific app do / how is it structured? | `src/apps/<name>/README.md` |
