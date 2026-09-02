# Pomodoro

A pomodoro timer with exactly one job: run work/break cycles with as little
to look at, and as little to press, as possible. Starts a 25-minute WORK
phase the moment it boots — no setup screen.

## Flow

```
WORK 25:00 → BREAK 5:00 → WORK → BREAK → WORK → BREAK → WORK → LONG BREAK 15:00 → repeat
```

Four work phases per set (configurable), then a long break, then a fresh set.
Every transition is automatic — once started, it runs forever with **zero
interaction required**.

## How to read it

A single ring traces the whole screen border and drains as the phase counts
down — full at the start of a phase, empty exactly when it ends. Colour
tells you which phase: amber for work, green for a short break, blue for the
long break.

The centre shows `MM:SS` remaining, the phase name above it, and a row of up
to four dots below — one lights up per completed work phase in the current
set.

At every phase change: a short two-note chime, a bright sweep around the
ring, then the next phase starts immediately.

## Controls

| Gesture | Effect |
|---|---|
| **Tap** anywhere | Pause / resume |
| **Hold ~1.2 s** | Reset the *current* phase back to full time |
| **Turn the device** | UI rotates to follow |
| **Power cycle** | Back to WORK, phase 1 of a fresh set |

There is deliberately no way to skip ahead, rename a phase, or change
durations from the device — that's a distraction surface in its own right.
Edit `app_config.h` and reflash instead.

## Configuration — `app_config.h`

| Setting | Default | Notes |
|---|---|---|
| `WORK_MINUTES` | `25` | |
| `BREAK_MINUTES` | `5` | |
| `LONG_BREAK_MINUTES` | `15` | |
| `SET_SIZE` | `4` | work phases before a long break |
| `TOUCH_HOLD_RESET_MS` | `1200` | |
| `CHIME_NOTE1_HZ` / `CHIME_NOTE2_HZ` | `880` / `660` | the two chime notes |
| `CHIME_NOTE_MS` | `120` | length of each note |

Whether the chime hardware is brought up at all is `AUDIO_ENABLED` in
`include/board_config.h` (board-level, not app-level) — see
`src/common/audio.h`. If it fails to initialise, the timer runs exactly the
same, just silently; nothing here depends on the chime succeeding.

## Implementation notes

**No cell table.** Unlike the hourglass app's two mirrored bulbs, this is one
loop that only ever drains in one direction within a phase, so the UI tracks
a single "filled length" and repaints just the boundary that moved each
frame — cheap enough at 40 fps without a per-second lookup table.

**The loop length is 1480 px in both orientations** (portrait trades 120 px
of top/bottom run for 120 px of side run), so, like the hourglass, rotating
never changes how the countdown looks or behaves — verified by walking every
1 px step of the path in both layouts: exact tiling, no gaps, no double
paint, in `scratchpad` during development.

**Timing** runs off `esp_timer_get_time()` (64-bit microseconds), the same
pattern as the hourglass app: pausing adds the paused interval back to the
phase origin rather than stopping a counter, so pause/resume never drifts.
