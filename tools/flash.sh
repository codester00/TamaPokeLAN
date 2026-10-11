#!/bin/bash
# Compile and flash TamaPoke to a connected board.
#
#   bash tools/flash.sh            # find the port, compile, upload
#   bash tools/flash.sh --monitor  # ...then open the serial console
#   bash tools/flash.sh -p /dev/cu.usbmodem1101
#
# The port is found rather than hardcoded: the ESP32-S3 shows up as
# /dev/cu.usbmodem* on macOS and /dev/ttyACM* on Linux, and the number changes
# between plug-ins. The Mac's own Bluetooth and debug consoles are also
# /dev/cu.*, so they are excluded by name -- matching them and trying to flash a
# debug console is the obvious trap. On Windows pass the port: -p COM5.
#
# The upload writes bootloader, partition table, boot_app0 and app at their own
# offsets and never touches 0x9000-0xE000, which is NVS -- the save.
set -e
cd "$(dirname "$0")/.."
# arduino-cli insists the sketch folder is named after the .ino. A checkout
# called anything else (a fork's repo name, say) fails with "main file missing",
# which reads like a broken build rather than a misnamed folder.
if [ "$(basename "$(pwd)")" != "TamaPoke" ]; then
  echo "This folder must be named TamaPoke for arduino-cli (it is '$(basename "$(pwd)")')."
  echo "Rename the checkout, or symlink it:  ln -s \"$(pwd)\" ../TamaPoke"
  exit 1
fi

FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
PORT=""
MONITOR=0
while [ $# -gt 0 ]; do
  case "$1" in
    -p|--port) PORT="$2"; shift 2;;
    --monitor|-m) MONITOR=1; shift;;
    *) echo "unknown option: $1"; exit 2;;
  esac
done

if [ -z "$PORT" ]; then
  # usbmodem/wchusbserial/SLAB are the real adapters; the rest are the Mac's own
  PORT=$(ls /dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/cu.SLAB* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || true)
fi

if [ -z "$PORT" ]; then
  cat <<'EOF'
No board found.

  1. Plug the board in with a USB-C cable that carries DATA. A charge-only
     cable is the single most common reason nothing appears.
  2. Re-run this script.

If it still does not appear, put the board in download mode by hand:
  hold BOOT, tap RESET, release BOOT -- then re-run.

On Windows the port is never guessed: run  arduino-cli board list  and pass
it with -p, e.g.  bash tools/flash.sh -p COM5

To see what this machine can see:  arduino-cli board list
EOF
  exit 1
fi

echo "Board on $PORT"
echo
echo "NOTE: flashing replaces the app partition and leaves NVS alone, so the pet"
echo "      normally survives. If this board has a run you care about, EXPORT it"
echo "      first over the serial console and keep the block -- it is the only"
echo "      way back if anything goes wrong."
echo

echo "Compiling..."
arduino-cli compile --fqbn "$FQBN" .

echo "Uploading..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" .

echo
echo "Done. Firmware v$(grep -o '"[0-9.]*"' TamaPoke.ino | head -1 | tr -d '"') on $PORT"

if [ "$MONITOR" = "1" ]; then
  echo "Opening the console at 115200 -- ctrl-C to leave."
  arduino-cli monitor -p "$PORT" --config baudrate=115200
else
  echo "Serial console:  arduino-cli monitor -p $PORT --config baudrate=115200"
fi
