# On-device app switching

Each app is its own complete firmware image, living in its own slot on the
chip. Switching apps means telling the bootloader which slot to boot next and
rebooting into it — there's no shared process, no in-RAM handoff, and no app
needs to know anything about any other app. This is `src/common/app_switch.*`
plus the `launcher` app, sitting on top of the partition layout in
`partitions_launcher.csv`.

## Why this design, not one combined binary

The alternative — one firmware with every app compiled in, switching between
them as FreeRTOS tasks with no reboot — was considered and rejected for this
project. It would mean refactoring every app off independent Arduino-sketch
style (global `setup()`/`loop()`, file-scope statics) into a namespaced
module a launcher could start and stop, touching code that was already
validated end-to-end on hardware. Keeping every app a fully independent
image costs a ~1-2s reboot on switch; in exchange, nothing about any app
changes to make it "launcher-aware" beyond the corner-hold gesture, and one
app's bug can never affect another's.

## The moving pieces

**`partitions_launcher.csv`** — 6 app slots of 2 MiB each: `launcher`
(always boots first — blank OTA state defaults to slot 0), then `slot1`
through `slot5` for apps. ~3.9 MB of the 16 MB chip is still left
unpartitioned for future growth. (Originally 960 KB per slot — bumped once
a Wi-Fi-enabled experiment came in at 1.07 MB, well past what any prior
app needed; kept at 2 MiB since every app benefits from the headroom.)

**`src/common/app_switch.*`** — the only code that touches OTA state.
`appSwitchLaunch(label)` finds a partition by name, checks it actually holds
a valid app image (reads the header magic byte — never trusts a slot is
populated just because the manifest says it should be), sets it as the boot
target, and reboots. `appSwitchGoHome()` is the same thing hardcoded to
`"launcher"`. `appSwitchPollHome()` / `appSwitchInHomeCorner()` implement the
corner-hold gesture in view-space (via `gfx_view`'s `vTouchToView()`), so it
tracks the top-left of whatever's currently displayed regardless of
rotation — not a fixed physical corner of the panel.

**`src/apps/launcher/manifest.h`** — which app SHOULD be in which slot, for
the menu to display. This is a claim, not a fact: the launcher checks
`appSwitchSlotValid()` before trusting it, so a slot nothing has been
flashed into yet shows dim rather than crashing into blank flash.

**`flash.sh`** — writes each app's `.bin` to the slot offset read straight
out of `partitions_launcher.csv` (so the offsets can't drift from the file
that defines them), via `esptool.py write_flash` directly rather than
PlatformIO's own `-t upload` — that command always targets a fixed 0x10000
regardless of which slot you actually want.

## Two things that must stay in sync by hand

`flash.sh`'s `app_slot()` function and `src/apps/launcher/manifest.h`'s
`APP_MANIFEST` both map app name → slot label. Nothing enforces they agree —
if you add an app to one and not the other, either the launcher shows a slot
that's never flashed, or `flash.sh` won't know where to put a built app.
Update both when adding an app; see the app-adding steps in the root README
and in `src/apps/launcher/README.md`.

## Provisioning a board

**First time, or after `restore_factory_v*.sh`, or if switching stops
behaving:**
```bash
./flash.sh all
```
Writes the bootloader, the partition table, `boot_app0.bin` (resets OTA
state so the chip boots the launcher), and every mapped app — one `esptool`
invocation, all in one shot.

**Updating a single already-provisioned app:**
```bash
./flash.sh hourglass
```
Only writes that app's slot. Faster, and doesn't touch OTA boot state, so
whichever app was actually running keeps running until you switch away from
it (either on-device or by reflashing it specifically).

## Behaviour worth knowing

**The "current" app survives a power cycle**, not just a soft reboot —
`esp_ota_set_boot_partition()` writes into the `otadata` partition, which is
flash, not RAM. Unplug the board mid-pomodoro and it comes back running
pomodoro, not the launcher. There's no true suspend (every boot is a cold
start of that app), but the effect is close to a phone remembering what you
had open.

**A slot with nothing flashed into it is inert, not dangerous.** The
launcher won't boot into one (checked via the image header magic byte before
ever calling `esp_ota_set_boot_partition`), so a partially-provisioned board
just shows some menu rows as "(not installed)" rather than crashing.
