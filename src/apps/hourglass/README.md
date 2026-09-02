# Hourglass chronometer

A count-up chronometer whose sand runs around the entire screen border. Starts
at zero on every power-up.

## How to read it

The whole border is the glass. The **top half of the ring is the upper bulb**,
the **bottom half the lower bulb**, and the **mid-points of the left and right
edges are the waist**.

Sand drains away from the top centre and piles up from the bottom centre —
the level dropping and rising exactly as in a real hourglass — with a thin
trickle of grains falling from the waist down to the top of the pile.

**One full transfer takes exactly one minute.** The ring is 120 cells, 60 per
bulb, so **every cell is one second** and the faint notches are ten-second
marks. On the minute the glass turns itself over with a short bright sweep.

The centre shows `HH:MM:SS` in seven-segment digits, hundredths in amber
below, and the run state under that.

## Controls

| Gesture | Effect |
|---|---|
| **Tap** anywhere | Pause / resume |
| **Hold ~1.2 s** | Reset to zero |
| **Turn the device 90°** | UI rotates to follow, glass flips, clock resets |
| **Power cycle** | Reset to zero |

## Configuration — `app_config.h`

| Setting | Default | Notes |
|---|---|---|
| `RING_MARGIN` / `RING_THICK` | `10` / `18` | ring inset and band thickness, px |
| `RING_CELLS_PER_BULB` | `60` | 60 → one cell per second |
| `TOUCH_HOLD_RESET_MS` | `1200` | long-press reset threshold |
| `TILT_RESET_ENABLED` | `1` | quarter turn restarts the clock |
| `SCREEN_FOLLOWS_DEVICE` | `1` | `0` pins the UI upright |
| `FRAME_INTERVAL_MS` | `25` | ~40 fps |
| `ANTI_BURNIN_PERIOD_MS` | `240000` | nudge the readout 2 px every 4 min |

Which quadrant counts as upright is `SCREEN_ROTATION_OFFSET` in
`include/board_config.h`, since it depends on how the IMU is mounted.

## Implementation notes

**No framebuffer.** The ring is a table of 120 cells that each know their own
rectangle(s) on the band; only cells whose sand state changed are repainted —
normally one per second. Digits repaint only the segments that differ. A full
repaint happens only on rotation or the four-minute anti-burn-in nudge.

**The sand model is orientation-independent.** The path works out to 740 px
with the waist at 370 in *both* orientations — portrait trades 40 px of
top-edge run for 40 px of side run — so the cells, the mass conservation and
the flip animation never change when the device turns.

**Geometry is exact.** The 120 cells tile all 26,640 band pixels with no gaps,
no double-painting and no bleed outside the band, in both orientations, and
top + bottom sand always sums to 60 cells across all 61 states.

**The clock** runs off `esp_timer_get_time()`, a 64-bit microsecond counter,
so there is nothing to wrap around on a chronometer left running for weeks.
Pausing adds the paused interval back to the origin rather than stopping a
counter, so pause/resume never loses or gains time.
