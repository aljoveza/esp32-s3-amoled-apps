# Board self-test

Reports what this board actually **is**, on the panel, with no serial cable
attached. Flash it first when something seems wrong, or when bringing up a new
board.

Written after a bring-up in which the checked-in code assumed the wrong board
revision *and* the wrong touch controller: every fact that was expensive to
discover then is on one screen here.

## What it shows

* **Panel** — which driver is compiled in (CO5300 V2 / SH8601 V1)
* **View** — canvas size and current rotation
* **I2C bus** — every address that answers, each named where the chip is known
* **Touch** — which controller answered, plus live coordinates
* **IMU** — live accelerometer vector in g, and the derived orientation quadrant
* a **crosshair** that follows your finger, which proves the touch coordinate mapping

The bus is re-scanned every 5 seconds, so a chip dropping off the bus is
visible rather than silently cached. The report rotates with the device.

## Reading the result

Expected on a healthy **V2** board:

```
PANEL    CO5300 (V2)
I2C BUS  6 found
         15 18 20 34 51 6B
         0x15 CST816T touch
         0x20 XCA9554 expander
         0x6B QMI8658 IMU
TOUCH    CST816T
IMU      QMI8658 ok
```

* **`0x38` present instead of `0x15`** → V1 board, set `BOARD_REVISION_V2 0`
* **IMU vector stuck at `-2.00,-2.00,-2.00`** → the `0x8000` no-data marker;
  the sensor is powered down (see `docs/HARDWARE.md`)
* **`ORIENT flat (ignored)`** → normal when the board lies on a desk; in-plane
  gravity is below `TILT_MIN_G` so the angle is meaningless
* **screen black** → almost certainly the wrong panel driver
