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

## Vendor examples

`examples_reference/arduino/` is the **V1** set (SH8601 + FT3168) and does
**not** match this board. `examples_reference/arduino-v2/` is the one to
trust: CO5300 with `col_offset1 = 16`, and CST816T touch.
