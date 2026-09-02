#include "audio.h"
#include "board_config.h"
#include "pin_config.h"
#include <Wire.h>
#include <math.h>
#include "driver/i2s.h"
#include "es8311.h"

static bool s_ready = false;
static es8311_handle_t s_codec = nullptr;

bool audioAvailable() { return s_ready; }

bool audioBegin() {
#if !AUDIO_ENABLED
  return false;
#else
  // Speaker amplifier enable. Do this first: some boards pop on power-up,
  // and it's better that happen before the codec is configured than after.
  pinMode(PA, OUTPUT);
  digitalWrite(PA, HIGH);

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = true;         // accurate MCLK -- the codec needs a clean one
  cfg.tx_desc_auto_clear = true;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[Audio] i2s_driver_install failed.");
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_MCK_IO;
  pins.bck_io_num = I2S_BCK_IO;
  pins.ws_io_num = I2S_WS_IO;
  pins.data_out_num = I2S_DO_IO;
  pins.data_in_num = I2S_PIN_NO_CHANGE;   // no mic input needed for a beep

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("[Audio] i2s_set_pin failed.");
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  // The codec talks over the I2C bus display_driver.cpp already opened with
  // Wire.begin() -- es8311.c calls the esp32-hal-i2c HAL directly by port
  // number, the same underlying bus, so no second I2C port is needed.
  s_codec = es8311_create(0, ES8311_ADDRESS_0);
  if (!s_codec) {
    Serial.println("[Audio] ES8311 not found on I2C bus.");
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  es8311_clock_config_t clk = {};
  clk.mclk_inverted = false;
  clk.sclk_inverted = false;
  clk.mclk_from_mclk_pin = true;
  clk.mclk_frequency = AUDIO_SAMPLE_RATE * 256;
  clk.sample_frequency = AUDIO_SAMPLE_RATE;

  bool ok = (es8311_init(s_codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) == ESP_OK) &&
            (es8311_sample_frequency_config(s_codec, clk.mclk_frequency, clk.sample_frequency) == ESP_OK) &&
            (es8311_microphone_config(s_codec, false) == ESP_OK) &&
            (es8311_voice_volume_set(s_codec, AUDIO_VOLUME, nullptr) == ESP_OK);

  if (!ok) {
    Serial.println("[Audio] ES8311 configuration failed.");
    i2s_driver_uninstall(I2S_NUM_0);
    s_codec = nullptr;
    return false;
  }

  s_ready = true;
  Serial.println("[Audio] ES8311 ready.");
  return true;
#endif
}

void audioBeep(uint16_t freqHz, uint16_t durationMs, uint8_t volumePercent) {
  if (!s_ready || freqHz == 0 || durationMs == 0) return;

  const uint32_t totalFrames = (uint32_t)AUDIO_SAMPLE_RATE * durationMs / 1000;
  const uint32_t fadeFrames = min<uint32_t>(totalFrames / 4, AUDIO_SAMPLE_RATE / 100); // ~10 ms, clickless
  const float amp = 32000.0f * (volumePercent > 100 ? 100 : volumePercent) / 100.0f;
  const float step = 2.0f * (float)PI * freqHz / AUDIO_SAMPLE_RATE;

  int16_t chunk[128 * 2];   // stereo frames
  uint32_t frame = 0;
  while (frame < totalFrames) {
    uint32_t n = min<uint32_t>(128, totalFrames - frame);
    for (uint32_t i = 0; i < n; i++, frame++) {
      float env = 1.0f;
      if (frame < fadeFrames) env = (float)frame / fadeFrames;
      else if (frame > totalFrames - fadeFrames) env = (float)(totalFrames - frame) / fadeFrames;
      int16_t s = (int16_t)(amp * env * sinf(step * frame));
      chunk[i * 2] = s;
      chunk[i * 2 + 1] = s;
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, chunk, n * 2 * sizeof(int16_t), &written, portMAX_DELAY);
  }
}
