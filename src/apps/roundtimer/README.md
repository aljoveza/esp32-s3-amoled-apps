# Round Timer

A countdown timer for a game. Configure the round length once, tap to
start, and it keeps timing rounds on its own — no further interaction
needed unless you want to pause or stop it.

## Flow

```
tap → count down 60s → ALARM → count down 60s → ALARM → ... (forever)
```

Every round is the same configured length. Nothing needs to be pressed
between rounds — the next one starts automatically the moment the alarm
finishes, until you explicitly stop it.

## How to read it

A ring traces the whole screen border and drains as the round counts down —
full at the start, empty at zero. Colour escalates as zero approaches:

- **Calm** (teal) — more than `WARNING_SEC` remaining
- **Warning** (amber) — inside the last `WARNING_SEC` seconds
- **Danger** (red) — inside the last `DANGER_SEC` seconds, ring and digits
  both turn red, and a rising-pitch tick beeps each second

At zero: a two-tone alarm plays, the ring sweeps bright white, "TIME'S UP"
shows briefly, then the next round begins.

The centre shows `MM:SS` remaining, and which round of the session you're
on.

## Controls

**While idle**, the screen is three vertical zones:

| Zone | Effect |
|---|---|
| Left third | Decrease the round length by `ROUND_SECONDS_STEP` |
| Middle third | Start the session |
| Right third | Increase the round length by `ROUND_SECONDS_STEP` |

The big `MM:SS` readout updates live as you tap — that's the round length
you're about to run, not a countdown yet. Clamped to
`ROUND_SECONDS_MIN`..`ROUND_SECONDS_MAX`. **Not persisted** — like every
other piece of state in this app, it resets to `ROUND_SECONDS` on every
power-up, so treat it as a per-session tweak, not a saved setting.

**Once running:**

| Gesture | Effect |
|---|---|
| **Tap** | Pause / resume |
| **Hold ~1.2 s** | Stop — back to idle, session ends |
| **Turn the device** | UI rotates to follow |
| **Power cycle** | Back to idle, length reset to the config default |

## Configuration — `app_config.h`

| Setting | Default | Notes |
|---|---|---|
| `ROUND_SECONDS` | `60` | length of one round |
| `WARNING_SEC` | `15` | amber zone starts this many seconds from zero |
| `DANGER_SEC` | `5` | red/flash/tick zone starts this many seconds from zero |
| `TICK_HZ_BASE` / `TICK_HZ_STEP` | `520` / `90` | tick pitch rises by `TICK_HZ_STEP` for each second closer to zero |
| `ALARM_HZ_1` / `ALARM_HZ_2` | `880` / `660` | the two-tone alarm at zero |
| `ALARM_HOLD_MS` | `1400` | how long "TIME'S UP" holds before the next round starts |
| `TOUCH_HOLD_RESET_MS` | `1200` | hold-to-stop threshold |

If `ROUND_SECONDS` is shorter than `WARNING_SEC`/`DANGER_SEC`, the round
just spends its whole length in the more dramatic zones — harmless, no
special-casing needed.

Sound is optional — silently skipped if the audio hardware never came up,
same as the `pomodoro` app's chime; see `src/common/audio.h`.

## Implementation notes

The ring is the exact same single-loop-drain construction as the
`pomodoro` app's (verified exact tiling there — see that app's README);
unchanged here since it's proven correct. Every part of the display — ring, digits — only repaints when something
it's showing actually changed (colour, value, or fill amount), never
unconditionally every frame; calm, warning and danger all render through
the exact same code path, differing only by which colour `tierColor()`
picks. Two things that used to be danger-zone-specific were tried and
dropped after they caused visible flicker on real hardware: a spinning
highlight around the ring, and a full-screen background flash pulsing red
once a second. Both meant a large-area erase-and-redraw that nothing else
on this board's apps ever does — every other app only ever touches small,
targeted regions, and that turned out to matter. Colour escalation, digit
colour, and the tick beeps carry "very graphic" well enough without either.

The tick beep's timing and the displayed seconds digit use the *same*
ceiling-division formula for converting elapsed microseconds to a whole
second, specifically so the beep never fires up to a second early or late
relative to what's on screen.
