#!/bin/bash
# Restore Waveshare ESP32-S3-Touch-AMOLED-1.8 (V2) Factory Firmware
echo "⚡ Flashing Factory Xiaozhi RTOS Firmware (V2)..."
echo "If connection fails, hold BOOT, press RESET, release BOOT."
pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 -b 921600 write_flash 0x0 firmware_backup/ESP32-S3-Touch-AMOLED-1.8-V2-FactoryXiaozhi_260601.bin
echo "✅ Finished. Please press RESET on your board to boot."
