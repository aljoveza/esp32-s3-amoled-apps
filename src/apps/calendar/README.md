# Calendar

A desk dashboard for your primary Google Calendar: the event running right
now and how long it's been going, and the next event and how long until it
starts. The UI rotates to follow the device, same as pomodoro/hourglass.
Talks to Google directly over Wi-Fi/HTTPS — no backend server involved, see
`SETUP.md` for the one-time OAuth setup this needs before it can ever show
real data.

> **Verification status:** this app builds against the ESP32 Arduino core's
> `WiFi.h`/`WiFiClientSecure`/`HTTPClient` and `bblanchon/ArduinoJson`, and
> its RFC 3339 date parsing and Google API request/response shapes were
> checked against Google's own Calendar API v3 docs and OAuth token
> endpoint docs. What **hasn't** been verified yet is a real end-to-end run
> against a live Google account and this physical board — flash it after
> completing `SETUP.md` and treat the first run as a debugging session
> (Wi-Fi join, NTP sync, token refresh, and the actual agenda parsing all
> need to be watched over serial the first time), same as every other piece
> of hardware/network code in this project — see `docs/HARDWARE.md`'s spirit
> even though this isn't hardware.

## What it shows

- **NOW** — the title of whatever's running right now on your primary
  calendar, and "started Xm ago" / "started Xh Ym ago".
- **NEXT** — the title of the next event, and "in Xm" / "in Xh Ym" — time
  until it *starts*, not time left in the current one.
- "No current event" / "Nothing else today" when there's nothing to show
  for either slot.
- A battery percentage badge, top-right, when the board's PMU reports one
  (same look as the launcher's).

All-day events are intentionally skipped for both NOW and NEXT — durations
don't mean much against a full-day block. Recurring events are expanded by
the Calendar API itself (`singleEvents=true`), so a recurring meeting shows
like any other.

## Setup

**Requires a one-time Google Cloud / OAuth setup before this will show
anything but "Connecting Wi-Fi..."** — see [`SETUP.md`](SETUP.md). Once
that's done:

1. `cp src/apps/calendar/secrets.h.example src/apps/calendar/secrets.h`
2. Fill in your Wi-Fi SSID/password and the `GOOGLE_CLIENT_ID` /
   `GOOGLE_CLIENT_SECRET` / `GOOGLE_REFRESH_TOKEN` from `SETUP.md`.
3. `./flash.sh calendar` (or `./flash.sh all` the first time, since adding
   this app also changed `partitions_launcher.csv`).

`secrets.h` is gitignored — it never gets committed.

## Behavior on battery vs. plugged in

Plugged in (or no battery detected), Wi-Fi stays connected and the agenda
is re-fetched every `POLL_INTERVAL_PLUGGED_MS` (default 60s). On battery,
the radio is switched off between polls, the backlight dims a step, and
polls happen only every `POLL_INTERVAL_BATTERY_MS` (default 5 min) — each
one pays a short reconnect cost. Either way, a failed poll never blanks the
screen: the last known agenda keeps its duration counters ticking (possibly
stale) until the next poll succeeds.

## Configuration — `app_config.h`

| Setting | Default | Notes |
|---|---|---|
| `POLL_INTERVAL_PLUGGED_MS` | 60s | agenda re-fetch interval while charging |
| `POLL_INTERVAL_BATTERY_MS` | 5min | agenda re-fetch interval on battery |
| `GCAL_LOOKAHEAD_SECONDS` | 18h | how far ahead "next event" is searched for |
| `GCAL_MAX_EVENTS` | 5 | events fetched per poll, ordered by start time |
| `DISPLAY_BRIGHTNESS_BATTERY` | 90 | dimmer level used only on battery |

## Implementation notes

`google_calendar.cpp` owns everything Google-facing: refreshing the access
token from the stored refresh token, fetching the agenda, and a small
hand-rolled RFC 3339 → UTC epoch parser (no date library in this repo, and
`strptime` isn't reliably available on this core) — comparisons are all raw
UTC epoch seconds, so **no local timezone configuration exists anywhere in
this app**; it only ever displays durations, never an absolute time. TLS is
pinned to Google Trust Services' `GTS Root R1` root (embedded in
`google_calendar.cpp`, public PKI material).

`calendar_ui.cpp` redraws only the duration line each second in the common
case (a full redraw only happens when a title, status message, or
current/next-event presence actually changes) — an AMOLED panel plus a
literal full-screen flash every second would be a visibly worse dashboard
than the extra bookkeeping costs.
