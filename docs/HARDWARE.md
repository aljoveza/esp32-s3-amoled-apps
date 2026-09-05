# Board hardware notes

**Waveshare ESP32-S3-Touch-AMOLED-1.8 — revision V2.**

Everything here was established on the actual board. It is written down
because most of it cost real debugging time, and because the vendor material
shipped in `examples_reference/` is partly for a *different* revision.

---

## Confirmed I2C map

An `i2cScan()` runs at boot in `src/common/display_driver.cpp`; the
**boardtest** app shows the same thing on screen. This board reports:

| Address | Chip | Notes |
|---|---|---|
| `0x15` | **CST816T** touch | **Not** the FT3168 the V1 examples assume |
| `0x18` | ES8311 audio codec | unused so far |
| `0x20` | XCA9554 IO expander | holds panel + touch reset lines |
| `0x34` | AXP2101 PMU | unused; does not gate the panel |
| `0x51` | PCF85063 RTC | unused so far |
| `0x6B` | QMI8658 IMU | this is `QMI8658_L_SLAVE_ADDRESS` in SensorLib |

If `0x38` ever appears instead of `0x15`, the board is a V1 and
`BOARD_REVISION_V2` must be `0`.

---

## Four faults that had to be fixed during bring-up

The code originally in this repo was configured for a V1 board. Nothing
worked — black screen, dead touch, garbage IMU — until all four were fixed.

| Symptom | Cause | Fix |
|---|---|---|
| Screen completely black | CO5300 panel driven as an SH8601 | `BOARD_REVISION_V2 1` |
| Image would be shifted sideways | CO5300 starts 16 columns into the controller's address space | `col_offset1 = 16` in the constructor |
| Touch never responded | Code probed an FT3168 at `0x38`; this board has a CST816T at `0x15` | `Arduino_CST816x` + periodic interrupt mode |
| IMU read `0x8000` on every axis | CTRL1 bit 1 (power-down) survived the reset, so the chip answered on I2C but published no data | `powerOn()` before configuring |

The IMU one is the sneakiest: `begin()` verifies WHO_AM_I and returns
**true**, so the chip looks perfectly healthy while every output register
reads the `0x8000` "no data" marker.

Two touch power-mode traps, both of which make the controller stop answering
between touches and break press-and-hold:

* FT3168 — do **not** leave it in `TOUCH_POWER_MONITOR`; use `TOUCH_POWER_ACTIVE`.
* CST816T — use `TOUCH_DEVICE_INTERRUPT_PERIODIC`.

---

## Battery (AXP2101) — confirmed working

`0x34` was in the I2C scan from the very first bring-up, named but unused
until now. `src/common/battery.*` brings it up via `lib/XPowersLib`
(`XPowersPMU power; power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA,
IIC_SCL)` — the vendor's own example for this exact board uses the same
call), shown in the `launcher` app's badge and the `boardtest` app's report.

## Audio (ES8311 + I2S) — confirmed working

The codec at `0x18` and the speaker amp (`PA`, GPIO 46) are live and were
brought up successfully from `src/common/audio.*` and confirmed end to end
(a synthesized two-note chime, heard on the actual board).

One thing worth knowing if you touch this code: the Waveshare example uses
`ESP_I2S.h` (`I2SClass` with `setPins()`/`I2S_MODE_STD`), which is **not
present** in the arduino-esp32 core this project's `platform = espressif32`
pulls in (only the older, different `I2SClass` in `libraries/I2S/` is). We
used the legacy ESP-IDF driver (`driver/i2s.h`: `i2s_driver_install`,
`i2s_set_pin`, `i2s_write`) instead, which is present and is what
`src/common/audio.cpp` is built on. Do not `#include <ESP_I2S.h>` expecting
it to be there.

The ES8311 driver itself (`lib/ES8311/`, vendored from the same example) talks
to the codec through `esp32-hal-i2c.h` — the same HAL Arduino's `Wire` uses —
so it shares the bus `Wire.begin()` already opened; no second I2C port needed.

## The panel cannot draw a fill exactly 1 pixel wide

Confirmed on real hardware, isolated with a minimal reproduction: any
`fillRect` call whose PANEL-space width is exactly 1 physical pixel draws
nothing at all -- silently, no error. A 1x1 fill and a 1x8 fill both
vanished; a 4x8 fill drawn right next to them worked fine. Root cause
traced into `Arduino_CO5300::writeAddrWindow()`'s CASET (column address set)
command in the vendored GFX library -- not anything in this project's own
code, and not something to patch in vendored code.

This mattered more than it sounds: `gfx_view.h`'s 5x7 text renderer draws
each glyph column as its own thin vertical fill, and at text size 1 every
column is exactly 1px wide -- so **size-1 text drew nothing on this board,
at all, in every app, the whole time**, and nobody had scrutinized small
hint/label text closely enough to notice small text was silently missing
entirely rather than just being small.

Two-part fix, both in `src/common/gfx_view.cpp`:

1. **`vFillRect`** widens any fill whose *panel-space* width would be
   exactly 1 to 2 pixels instead of silently drawing nothing. (Which of the
   view-space `w`/`h` parameters maps to the panel's width axis depends on
   rotation -- odd rotations swap them, same as `viewW()`/`viewH()` do.)
   Fine for borders, single-pixel accents, anything geometric.
2. **Text specifically still doesn't work below size 2**, even with that
   fix: at size 1, adjacent glyph columns sit only 1px apart, so widening
   every column to 2px makes neighbouring columns fully overlap, turning
   whole characters into solid blobs -- confirmed on hardware, genuinely
   unreadable, not just uglier. `vDrawText`/`vTextWidth` now clamp size to a
   minimum of 2 centrally, so no caller anywhere has to know about this.
   Every app on this board had its size-1 hint text bumped up a size as a
   result -- some layouts needed retuning (wider strings, taller line
   spacing) since a footprint that assumed 8px-tall text now needs 16px.

## The panel cannot rotate

`Arduino_CO5300::setRotation()` carries the comment *"CO5300 does not support
rotation"*. Rotations 1 and 3 only set X/Y mirror bits; they do not swap the
axes. Any app wanting a rotating UI must rotate in software —
`src/common/gfx_view.h` does this for you.

---

## Serial port hazard (this bites hard)

On the ESP32-S3 USB-Serial-JTAG port, **RTS drives `EN`** and **DTR drives
`IO0`**. Any tool that opens the port and exits without releasing those lines
parks the chip in reset: the USB device disappears from the bus entirely and
only a physical cable reseat brings it back.

Two things that caused this repeatedly:

* a Python `serial.Serial(port)` opened with default control lines
* `pio device monitor` in a **non-TTY** shell — miniterm opens the port, then
  crashes on `termios`, leaving the lines asserted

`pio device monitor` from a real terminal is fine. From a script, open the
port with `dtr = False` / `rts = False` set *before* `open()` and restore them
in a `finally`.

---

## Bluetooth LE (`camremote`) — GoPro needs a secured link; there's no Classic BT

**The ESP32-S3's radio is BLE-only** — no Classic Bluetooth (BR/EDR), unlike
the original ESP32. That rules out AVRCP (the profile most cheap "Bluetooth
selfie remote" gadgets for phones actually use for their volume-key trick):
`camremote`'s PHONE mode gets the same effect a different way, advertising
as a plain **BLE HID** "Consumer Control" peripheral and sending the
Volume-Up usage code instead.

**A GoPro (HERO8 Black or newer, the "Open GoPro" BLE command set) ignores
every command sent over a plain, unencrypted connection — no error, it just
does nothing.** This cost other people real debugging time before it cost
us any (see the [ESP32-S3 discussion linked from `camremote`'s
README](../src/apps/camremote/README.md)): connecting alone isn't enough,
the link has to be secured. `camremote_gopro.cpp` calls
`client->secureConnection()` right after connecting and won't try to
subscribe/write anything until `onAuthenticationComplete` reports
`connInfo.isEncrypted()`. GoPro's own BLE GATT layout isn't published as a
plain UUID table anywhere official; the values `camremote` uses (service
`0xFEA6`, command request/response characteristics `b5f90072`/`b5f90073` on
GoPro's own 128-bit UUID base) come from the community-maintained protocol
reference in [KonradIT/goprowifihack](https://github.com/KonradIT/goprowifihack/blob/master/Bluetooth/bluetooth-api.md)
and the [Open GoPro tutorials](https://gopro.github.io/OpenGoPro/), cross-checked
against each other, not from hardware this project owns a GoPro to test
against — see the app's README for what that means for verification.

---

## Vendor examples

`examples_reference/arduino/` is the **V1** set (SH8601 + FT3168) and does
**not** match this board. `examples_reference/arduino-v2/` is the one to
trust: CO5300 with `col_offset1 = 16`, and CST816T touch.
