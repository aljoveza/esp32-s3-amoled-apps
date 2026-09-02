#!/usr/bin/env bash
# =========================================================================
#  Build/flash menu for the multi-app launcher setup.
#
#  Every app is its own firmware image living in its own OTA slot (see
#  partitions_launcher.csv); switching which one runs is done ON THE DEVICE,
#  from the launcher's menu or the corner-hold gesture -- see
#  docs/LAUNCHER.md. This script is only for getting firmware ONTO the board
#  in the first place, or updating one app in place.
#
#  Usage:  ./flash.sh                 menu
#          ./flash.sh all             flash bootloader+partitions+every app
#                                      (use this once, first time, or after
#                                      restoring factory firmware)
#          ./flash.sh hourglass       build + flash just that app's slot
#          ./flash.sh -b hourglass    build only, do not flash
#
#  The app -> slot mapping below MUST match src/apps/launcher/manifest.h --
#  see docs/LAUNCHER.md before changing either.
# =========================================================================
set -u
cd "$(dirname "$0")"

BOLD=$'\033[1m'; DIM=$'\033[2m'; RED=$'\033[31m'; GRN=$'\033[32m'
YEL=$'\033[33m'; CYN=$'\033[36m'; OFF=$'\033[0m'

BUILD_ONLY=0
[ "${1:-}" = "-b" ] && { BUILD_ONLY=1; shift; }

command -v pio >/dev/null 2>&1 || {
  echo "${RED}pio not found on PATH.${OFF} Install PlatformIO, or add ~/.local/bin to PATH."
  exit 1
}

PART_CSV="partitions_launcher.csv"
BOOT_APP0="$(find "$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions" -name boot_app0.bin 2>/dev/null | head -1)"

# ---- app <-> slot mapping (must match src/apps/launcher/manifest.h) -----
app_slot() {
  case "$1" in
    launcher)  echo "launcher" ;;
    hourglass) echo "slot1" ;;
    pomodoro)  echo "slot2" ;;
    boardtest) echo "slot3" ;;
    roundtimer) echo "slot5" ;;
    *)         echo "" ;;
  esac
}

# Reads the offset for a partition label straight out of the CSV, so this
# script and the CSV can never quietly drift apart.
slot_offset() {
  awk -F',' -v n="$1" '
    { gsub(/^[ \t]+|[ \t]+$/, "", $1) }
    $1==n { gsub(/^[ \t]+|[ \t]+$/, "", $4); print $4; exit }
  ' "$PART_CSV"
}

# ---- discover apps from platformio.ini -----------------------------------
mapfile -t APPS < <(sed -n 's/^\[env:\(.*\)\]$/\1/p' platformio.ini) 2>/dev/null || \
  APPS=($(sed -n 's/^\[env:\(.*\)\]$/\1/p' platformio.ini))
[ ${#APPS[@]} -eq 0 ] && { echo "${RED}No [env:...] apps found in platformio.ini${OFF}"; exit 1; }

describe() {
  local f="src/apps/$1/README.md"
  [ -f "$f" ] && sed -n 's/^# *//p' "$f" | head -1 || echo ""
}

find_port() { ls /dev/cu.usbmodem* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1; }

esptool_run() {  # esptool_run <port> <offset1> <file1> [<offset2> <file2> ...]
  local port="$1"; shift
  pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 --port "$port" \
    --baud 460800 --before default_reset --after hard_reset \
    write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB "$@"
}

ensure_port() {
  PORT="$(find_port)"
  if [ -z "$PORT" ]; then
    echo "${YEL}No board found on USB.${OFF}"
    echo "Plug the board in (or reseat the cable) and press Enter to retry, q to quit."
    read -r again
    [ "$again" = "q" ] && exit 1
    PORT="$(find_port)"
    [ -z "$PORT" ] && { echo "${RED}Still no port. Giving up.${OFF}"; exit 1; }
  fi
  local busy; busy="$(lsof "$PORT" 2>/dev/null | awk 'NR>1 {print $2}' | head -1)"
  if [ -n "$busy" ]; then
    echo "${YEL}$PORT is held by PID $busy${OFF} ($(ps -o comm= -p "$busy" 2>/dev/null))."
    printf "Close it and press Enter, or q to quit: "
    read -r k
    [ "$k" = "q" ] && exit 1
  fi
}

flash_one() {  # flash_one <app>
  local app="$1" slot; slot="$(app_slot "$app")"
  if [ -z "$slot" ]; then
    echo "${RED}'$app' has no slot assignment.${OFF} Add it to app_slot() in this script"
    echo "and to APP_MANIFEST in src/apps/launcher/manifest.h -- see docs/LAUNCHER.md."
    return 1
  fi
  local off; off="$(slot_offset "$slot")"
  if [ -z "$off" ]; then
    echo "${RED}Slot '$slot' not found in $PART_CSV.${OFF}"
    return 1
  fi

  echo "${CYN}Building $app...${OFF}"
  pio run -e "$app" || { echo "${RED}Build failed - not flashing.${OFF}"; return 1; }
  [ "$BUILD_ONLY" = "1" ] && { echo "${GRN}Built (build-only, not flashed).${OFF}"; return 0; }

  ensure_port
  echo "${CYN}Flashing $app into slot '$slot' ($off) on $PORT...${OFF}"
  if esptool_run "$PORT" "$off" ".pio/build/$app/firmware.bin"; then
    echo "${GRN}Done — $app is in slot '$slot'.${OFF}"
    [ "$app" != "launcher" ] && echo "${DIM}From the launcher menu, or hold the top-left corner in another app, to open it.${OFF}"
  else
    echo "${RED}Upload failed.${OFF}"
    echo "${DIM}If the board vanished from USB, reseat the cable: on the ESP32-S3 USB-JTAG"
    echo "port RTS drives EN, so a tool that exits with it asserted leaves the chip in reset.${OFF}"
    return 1
  fi
}

flash_all() {
  echo "${BOLD}Full provision: bootloader + partition table + every app.${OFF}"
  echo "${DIM}This is safe to re-run any time; it's the right first step on a fresh board,"
  echo "after ./restore_factory_v*.sh, or if app switching stops behaving.${OFF}"
  echo

  local args=()
  echo "${CYN}Building launcher...${OFF}"
  pio run -e launcher || { echo "${RED}launcher build failed.${OFF}"; return 1; }

  local built=(launcher)
  for app in "${APPS[@]}"; do
    [ "$app" = "launcher" ] && continue
    local slot; slot="$(app_slot "$app")"
    [ -z "$slot" ] && { echo "${YEL}Skipping '$app': no slot assignment.${OFF}"; continue; }
    echo "${CYN}Building $app...${OFF}"
    if pio run -e "$app"; then built+=("$app"); else echo "${RED}$app build failed - skipping.${OFF}"; fi
  done

  [ "$BUILD_ONLY" = "1" ] && { echo "${GRN}Built (build-only, not flashed): ${built[*]}${OFF}"; return 0; }

  ensure_port
  args=("0x0" ".pio/build/launcher/bootloader.bin" "0x8000" ".pio/build/launcher/partitions.bin")
  if [ -n "$BOOT_APP0" ]; then
    args+=("0xe000" "$BOOT_APP0")   # resets OTA boot selection to slot 0 (launcher)
  else
    echo "${YEL}boot_app0.bin not found - otadata will keep whatever it already has.${OFF}"
  fi
  for app in "${built[@]}"; do
    local slot; slot="$(app_slot "$app")"
    local off; off="$(slot_offset "$slot")"
    args+=("$off" ".pio/build/$app/firmware.bin")
  done

  echo "${CYN}Writing bootloader, partition table, and ${#built[@]} app(s) to $PORT...${OFF}"
  if esptool_run "$PORT" "${args[@]}"; then
    echo "${GRN}Done. The board will boot into the launcher.${OFF}"
  else
    echo "${RED}Flash failed.${OFF}"
    echo "${DIM}Reseat the cable if the board vanished from USB (see README).${OFF}"
    return 1
  fi
}

# ---- pick a target --------------------------------------------------------
TARGET="${1:-}"
if [ -z "$TARGET" ]; then
  echo
  echo "${BOLD}ESP32-S3 AMOLED 1.8 — launcher setup${OFF}"
  echo
  echo "  ${BOLD}0${OFF}) Flash EVERYTHING  ${DIM}bootloader + partitions + every app — first time / reset${OFF}"
  i=1
  for a in "${APPS[@]}"; do
    printf "  ${BOLD}%d${OFF}) %-12s ${DIM}%s${OFF}\n" "$i" "$a" "$(describe "$a")"
    i=$((i+1))
  done
  echo
  printf "Choose [0-%d], or q to quit: " "${#APPS[@]}"
  read -r choice
  case "$choice" in
    q|Q|"") echo "Nothing to do."; exit 0 ;;
    0) flash_all; exit $? ;;
    *[!0-9]*) echo "${RED}Not a number.${OFF}"; exit 1 ;;
  esac
  [ "$choice" -ge 1 ] 2>/dev/null && [ "$choice" -le "${#APPS[@]}" ] || {
    echo "${RED}Out of range.${OFF}"; exit 1; }
  TARGET="${APPS[$((choice-1))]}"
elif [ "$TARGET" = "all" ]; then
  flash_all; exit $?
fi

printf '%s\n' "${APPS[@]}" | grep -qx "$TARGET" || {
  echo "${RED}Unknown app '$TARGET'.${OFF} Known: ${APPS[*]} (or 'all')"
  exit 1
}

echo
echo "${BOLD}==> $TARGET${OFF}  ${DIM}$(describe "$TARGET")${OFF}"
flash_one "$TARGET"
