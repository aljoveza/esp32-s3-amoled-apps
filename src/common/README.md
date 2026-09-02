# Shared board layer

Everything an app on this board needs before it can draw its first pixel.
Compiled into **every** app (`build_src_filter = +<common/> ...`), so an app
never re-derives any of it.

Nothing here knows what any app does. Board facts live in
`include/board_config.h`; hardware background is in `docs/HARDWARE.md`.

## Modules

### `display_driver.*` — panel + touch bring-up

`initDisplayAndTouch()` does the whole sequence in the order the vendor
examples use, which is *not* the obvious one:

1. `Wire.begin()`, then an **I2C scan** printed to serial
2. XCA9554 expander at `0x20` — pulse pins 0/1/2 low then high to release the
   panel and touch reset lines (pin 6 is deliberately left alone)
3. **touch before the panel**, probing CST816T at `0x15` first and falling
   back to FT3168 at `0x38`, so one binary covers either board revision
4. the QSPI panel last — CO5300 with `col_offset1 = 16` on V2, else SH8601

Exports `gfx`, `readTouchInput()`, `touchAvailable()`, `touchModelName()`,
`setDisplayBrightness()` and `i2cScan()`.

`readTouchInput()` decides "is a finger down" from the **finger count**, not
from the interrupt flag — a stale interrupt otherwise reads as a press and
press-and-hold can never work.

### `motion.*` — orientation from the QMI8658

Reports which way up the board is; it does not decide what that means, so
apps are free to rotate, reset, or ignore it.

* `motionBegin()` — `powerOn()` **before** configuring (see `docs/HARDWARE.md`)
* `motionPoll()` — call from `loop()`; returns `true` once per settled quarter turn
* `motionOrientation()` — quadrant `0..3`, or `-1` while the board lies flat
* `motionAccel()` — last sample in g

Gravity is projected onto the screen plane and snapped to a quadrant. A new
quadrant must hold for `TILT_STABLE_MS`, there is a 32° deadband around the
45° boundaries so an awkwardly-held board cannot flicker, and readings below
`TILT_MIN_G` of in-plane gravity are discarded as meaningless rather than
guessed at.

### `gfx_view.*` — rotated drawing surface

The panel cannot rotate in hardware, so this rotates in software. Draw
against `viewW()` / `viewH()` and use these instead of touching `gfx`
directly, and your app is orientation-agnostic for free:

| | |
|---|---|
| `viewSetRotation(0..3)`, `viewRotation()` | quarter turns, clockwise |
| `viewW()`, `viewH()` | canvas size, swaps with rotation |
| `vFillRect`, `vFillTriangle`, `vFillScreen` | primitives |
| `vDrawText`, `vDrawTextCentered`, `vTextWidth` | 5×7 font, drawn manually because the library cannot rotate text |
| `vSeg7Digit`, `vSeg7Colon` | seven-segment digits from plain rectangles |
| `C565(r,g,b)` | 8-bit RGB → RGB565 |

`vSeg7Digit` takes the previously drawn value and repaints only the segments
that changed, which is what makes a hundredths field affordable at 40 fps.
Text glyphs collapse each font column into vertical runs, ~12 rects per
character instead of 35.
