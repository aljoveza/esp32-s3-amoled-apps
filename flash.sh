#!/usr/bin/env bash
# =========================================================================
#  Interactive build/flash menu.
#
#  Apps are discovered from platformio.ini, so a new [env:<name>] block
#  shows up here with no changes to this script. The one-line description
#  comes from the first "# " line of the app's README.md.
#
#  Usage:  ./flash.sh              pick from a menu
#          ./flash.sh hourglass    build + flash that app directly
#          ./flash.sh -b boardtest build only, do not flash
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

# --- discover apps -------------------------------------------------------
mapfile -t APPS < <(sed -n 's/^\[env:\(.*\)\]$/\1/p' platformio.ini) 2>/dev/null || \
  APPS=($(sed -n 's/^\[env:\(.*\)\]$/\1/p' platformio.ini))
[ ${#APPS[@]} -eq 0 ] && { echo "${RED}No [env:...] apps found in platformio.ini${OFF}"; exit 1; }

describe() {  # first "# Title" line of the app README, minus the leading hashes
  local f="src/apps/$1/README.md"
  [ -f "$f" ] && sed -n 's/^# *//p' "$f" | head -1 || echo ""
}

find_port() { ls /dev/cu.usbmodem* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1; }

# --- pick the app --------------------------------------------------------
TARGET="${1:-}"
if [ -z "$TARGET" ]; then
  echo
  echo "${BOLD}ESP32-S3 AMOLED 1.8 — apps${OFF}"
  echo
  i=1
  for a in "${APPS[@]}"; do
    printf "  ${BOLD}%d${OFF}) %-12s ${DIM}%s${OFF}\n" "$i" "$a" "$(describe "$a")"
    i=$((i+1))
  done
  echo
  printf "Choose [1-%d], or q to quit: " "${#APPS[@]}"
  read -r choice
  case "$choice" in
    q|Q|"") echo "Nothing to do."; exit 0 ;;
    *[!0-9]*|"") echo "${RED}Not a number.${OFF}"; exit 1 ;;
  esac
  [ "$choice" -ge 1 ] 2>/dev/null && [ "$choice" -le "${#APPS[@]}" ] || {
    echo "${RED}Out of range.${OFF}"; exit 1; }
  TARGET="${APPS[$((choice-1))]}"
fi

# validate an explicitly named target
printf '%s\n' "${APPS[@]}" | grep -qx "$TARGET" || {
  echo "${RED}Unknown app '$TARGET'.${OFF} Known: ${APPS[*]}"
  exit 1
}

echo
echo "${BOLD}==> $TARGET${OFF}  ${DIM}$(describe "$TARGET")${OFF}"

# --- build ---------------------------------------------------------------
echo "${CYN}Building...${OFF}"
if ! pio run -e "$TARGET"; then
  echo "${RED}Build failed — not flashing.${OFF}"
  exit 1
fi

[ "$BUILD_ONLY" = "1" ] && { echo "${GRN}Built (build-only, not flashed).${OFF}"; exit 0; }

# --- flash ---------------------------------------------------------------
PORT="$(find_port)"
if [ -z "$PORT" ]; then
  echo "${YEL}No board found on USB.${OFF}"
  echo "Plug the board in (or reseat the cable) and press Enter to retry, q to quit."
  read -r again
  [ "$again" = "q" ] && exit 1
  PORT="$(find_port)"
  [ -z "$PORT" ] && { echo "${RED}Still no port. Giving up.${OFF}"; exit 1; }
fi

# A serial monitor holding the port blocks esptool, and on the ESP32-S3
# USB-JTAG a tool that exits without releasing RTS parks the chip in reset.
BUSY="$(lsof "$PORT" 2>/dev/null | awk 'NR>1 {print $2}' | head -1)"
if [ -n "$BUSY" ]; then
  echo "${YEL}$PORT is held by PID $BUSY${OFF} ($(ps -o comm= -p "$BUSY" 2>/dev/null))."
  printf "Close it and press Enter, or q to quit: "
  read -r k
  [ "$k" = "q" ] && exit 1
fi

echo "${CYN}Flashing $TARGET to $PORT...${OFF}"
if pio run -e "$TARGET" -t upload --upload-port "$PORT"; then
  echo "${GRN}Done — $TARGET is running.${OFF}"
else
  echo "${RED}Upload failed.${OFF}"
  echo "${DIM}If the board vanished from USB, reseat the cable: on the ESP32-S3"
  echo "USB-JTAG port RTS drives EN, so a tool that exits with RTS asserted"
  echo "leaves the chip held in reset.${OFF}"
  exit 1
fi
