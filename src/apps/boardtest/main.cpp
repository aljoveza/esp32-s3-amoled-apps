// =========================================================================
//  BOARD SELF-TEST
//
//  Reports what the board actually IS, on the panel, with no serial cable.
//  Written after a bring-up in which the checked-in code assumed the wrong
//  board revision and the wrong touch controller: every fact that was
//  expensive to discover then is printed here in one screen.
//
//    - which panel driver is compiled in, and the view size
//    - every I2C address that answers, named where we know the chip
//    - which touch controller answered, with live coordinates
//    - live accelerometer vector and the derived orientation quadrant
//
//  Touch the screen and the crosshair follows your finger, which also proves
//  the touch coordinate mapping.
// =========================================================================
#include <Arduino.h>
#include <Wire.h>
#include "app_config.h"
#include "display_driver.h"
#include "gfx_view.h"
#include "motion.h"

static const uint16_t C_BG    = C565(0, 0, 0);
static const uint16_t C_TITLE = C565(255, 255, 255);
static const uint16_t C_KEY   = C565(110, 106, 100);
static const uint16_t C_OK    = C565(86, 220, 140);
static const uint16_t C_BAD   = C565(255, 96, 96);
static const uint16_t C_VAL   = C565(255, 176, 60);
static const uint16_t C_RULE  = C565(48, 46, 44);
static const uint16_t C_CROSS = C565(120, 214, 255);

static char s_addrLine[64];
static uint8_t s_addrCount;

// Names for the chips this board is known to carry, so a scan reads as
// something meaningful rather than a list of numbers.
static const char *chipName(uint8_t addr) {
  switch (addr) {
    case 0x15: return "CST816T touch";
    case 0x18: return "ES8311 audio";
    case 0x20: return "XCA9554 expander";
    case 0x34: return "AXP2101 PMU";
    case 0x38: return "FT3168 touch";
    case 0x51: return "PCF85063 RTC";
    case 0x6A: return "QMI8658 IMU (H)";
    case 0x6B: return "QMI8658 IMU";
    default:   return "unknown";
  }
}

static void scanBus() {
  s_addrLine[0] = 0;
  s_addrCount = 0;
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) continue;
    char one[8];
    snprintf(one, sizeof(one), "%s%02X", s_addrCount ? " " : "", addr);
    strncat(s_addrLine, one, sizeof(s_addrLine) - strlen(s_addrLine) - 1);
    s_addrCount++;
  }
}

static int16_t s_row;
static void row(const char *key, const char *val, uint16_t valColor) {
  int16_t x = 16;
  vFillRect(x, s_row, viewW() - 2 * x, 8, C_BG);
  vDrawText(key, x, s_row, 1, C_KEY);
  vDrawText(val, x + 96, s_row, 1, valColor);
  s_row += 12;
}

static void drawStatic() {
  vFillScreen(C_BG);
  vDrawTextCentered("BOARD SELF-TEST", 14, 2, C_TITLE);
  vFillRect(16, 34, viewW() - 32, 1, C_RULE);
}

// Only the live rows are repainted each frame.
static void drawReport() {
  char buf[64];
  s_row = 46;

  row("PANEL", BOARD_REVISION_V2 ? "CO5300 (V2)" : "SH8601 (V1)", C_VAL);
  snprintf(buf, sizeof(buf), "%dx%d rot %u", viewW(), viewH(), viewRotation());
  row("VIEW", buf, C_VAL);

  snprintf(buf, sizeof(buf), "%u found", s_addrCount);
  row("I2C BUS", buf, s_addrCount ? C_OK : C_BAD);
  row("", s_addrLine, C_VAL);

  // Name each address on its own line so an unexpected chip is obvious.
  for (const char *p = s_addrLine; *p; ) {
    unsigned v = 0;
    if (sscanf(p, "%2x", &v) == 1) {
      snprintf(buf, sizeof(buf), "0x%02X %s", v, chipName((uint8_t)v));
      row("", buf, C_KEY);
    }
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
  }

  s_row += 4;
  row("TOUCH", touchModelName(), touchAvailable() ? C_OK : C_BAD);

  TouchPoint pt = readTouchInput();
  if (pt.isPressed) snprintf(buf, sizeof(buf), "x=%d y=%d", pt.x, pt.y);
  else              snprintf(buf, sizeof(buf), "(touch the screen)");
  row("", buf, pt.isPressed ? C_VAL : C_KEY);

  s_row += 4;
  row("IMU", motionAvailable() ? "QMI8658 ok" : "not found",
      motionAvailable() ? C_OK : C_BAD);

  float ax, ay, az;
  if (motionAccel(&ax, &ay, &az)) {
    snprintf(buf, sizeof(buf), "%+.2f %+.2f %+.2f g", ax, ay, az);
    row("", buf, C_VAL);
  }
  int8_t q = motionOrientation();
  if (q < 0) snprintf(buf, sizeof(buf), "flat (ignored)");
  else       snprintf(buf, sizeof(buf), "quadrant %d", q);
  row("ORIENT", buf, q < 0 ? C_KEY : C_VAL);

  // Crosshair that tracks the finger, which also proves the coordinate map.
  static int16_t lastX = -1, lastY = -1;
  if (lastX >= 0) {
    vFillRect(lastX - 10, lastY - 1, 21, 3, C_BG);
    vFillRect(lastX - 1, lastY - 10, 3, 21, C_BG);
  }
  if (pt.isPressed) {
    lastX = pt.x; lastY = pt.y;
    vFillRect(lastX - 10, lastY - 1, 21, 3, C_CROSS);
    vFillRect(lastX - 1, lastY - 10, 3, 21, C_CROSS);
  } else {
    lastX = lastY = -1;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== BOARD SELF-TEST ===");

  initDisplayAndTouch();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  motionBegin();

  scanBus();
  drawStatic();
}

void loop() {
  static uint32_t lastFrame = 0, lastScan = 0;
  uint32_t now = millis();

  motionPoll();

#if BT_FOLLOW_DEVICE
  int8_t q = motionOrientation();
  if (q >= 0) {
    uint8_t want = (uint8_t)((q + SCREEN_ROTATION_OFFSET) & 3);
    if (want != viewRotation()) {
      viewSetRotation(want);
      drawStatic();
    }
  }
#endif

  if (now - lastScan >= 5000) {   // re-scan so a chip dropping off is visible
    lastScan = now;
    scanBus();
  }

  if (now - lastFrame >= BT_FRAME_MS) {
    lastFrame = now;
    drawReport();
  }
  delay(2);
}
