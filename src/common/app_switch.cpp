#include "app_switch.h"
#include "board_config.h"
#include "gfx_view.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"

bool appSwitchInHomeCorner(TouchPoint pt) {
  if (!pt.isPressed) return false;
  int16_t vx, vy;
  vTouchToView(pt.x, pt.y, &vx, &vy);
  return vx < HOME_CORNER_SIZE && vy < HOME_CORNER_SIZE;
}

static bool     s_holding = false;
static uint32_t s_holdStart = 0;

bool appSwitchPollHome(TouchPoint pt, uint32_t nowMs) {
  if (!appSwitchInHomeCorner(pt)) {
    s_holding = false;
    return false;
  }
  if (!s_holding) {
    s_holding = true;
    s_holdStart = nowMs;
    return false;
  }
  if (nowMs - s_holdStart < HOME_HOLD_MS) return false;
  s_holding = false;   // fires once
  return true;
}

bool appSwitchSlotValid(const char *partitionLabel) {
  const esp_partition_t *p =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, partitionLabel);
  if (!p) return false;
  uint8_t magic = 0;
  if (esp_partition_read(p, 0, &magic, 1) != ESP_OK) return false;
  return magic == ESP_IMAGE_HEADER_MAGIC;   // blank/erased flash reads 0xFF, never this
}

bool appSwitchLaunch(const char *partitionLabel) {
  const esp_partition_t *p =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, partitionLabel);
  if (!p) {
    Serial.printf("[AppSwitch] partition '%s' not found.\n", partitionLabel);
    return false;
  }
  if (!appSwitchSlotValid(partitionLabel)) {
    Serial.printf("[AppSwitch] partition '%s' has no valid app image - refusing to boot into it.\n",
                  partitionLabel);
    return false;
  }
  if (esp_ota_set_boot_partition(p) != ESP_OK) {
    Serial.printf("[AppSwitch] esp_ota_set_boot_partition('%s') failed.\n", partitionLabel);
    return false;
  }
  Serial.printf("[AppSwitch] booting '%s'...\n", partitionLabel);
  Serial.flush();
  delay(30);
  esp_restart();
  return true;   // unreachable
}

void appSwitchGoHome() {
  appSwitchLaunch("launcher");
  // If we get here, "launcher" was missing or invalid -- stay running rather
  // than reboot into nothing.
  Serial.println("[AppSwitch] could not go home; launcher partition missing or invalid.");
}
