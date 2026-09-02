# ES8311

Vendored, not written here. Source: Espressif (Apache-2.0), via
`examples_reference/arduino/examples/15_ES8311/`. Drives the ES8311 audio
codec at I2C `0x18` — confirmed present on this board by our own bus scan.

Talks to the codec through `esp32-hal-i2c.h`, the same underlying HAL that
Arduino's `Wire` uses, so it shares the bus `Wire.begin()` already opened in
`display_driver.cpp` rather than needing a second I2C port.

Used by `src/common/audio.*`, which is the API apps should call — nothing
in this project includes `es8311.h` directly outside that file.
