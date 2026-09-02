#include "display_driver.h"

#if BOARD_REVISION_V2
Arduino_CO5300 *gfx = nullptr;
#else
Arduino_SH8601 *gfx = nullptr;
#endif
Adafruit_XCA9554 expander;

static std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = nullptr;
static std::unique_ptr<Arduino_IIC> Touch = nullptr;
static volatile bool s_touchInterruptOccurred = false;

// V2 boards carry a CST816T at 0x15; V1 boards an FT3168 at 0x38. Both are
// probed so one binary covers either revision.
enum TouchKind : uint8_t { TOUCH_NONE = 0, TOUCH_CST816, TOUCH_FT3168 };
static uint8_t s_touchKind = TOUCH_NONE;

static void IRAM_ATTR touchInterruptHandler() {
  s_touchInterruptOccurred = true;
}

// Ground truth for bring-up problems: which chips actually answer on the bus.
void i2cScan(const char *when) {
  Serial.printf("[I2C] scan (%s):", when);
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
      found++;
    }
  }
  if (!found) Serial.print(" (nothing responded)");
  Serial.printf("  [%u device(s)]\n", found);
}

static bool tryTouch(uint8_t kind, uint8_t addr, const char *name) {
  IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
  if (kind == TOUCH_CST816) {
    Touch = std::unique_ptr<Arduino_IIC>(new Arduino_CST816x(
      IIC_Bus, addr, DRIVEBUS_DEFAULT_VALUE, TP_INT, touchInterruptHandler));
  } else {
    Touch = std::unique_ptr<Arduino_IIC>(new Arduino_FT3x68(
      IIC_Bus, addr, DRIVEBUS_DEFAULT_VALUE, TP_INT, touchInterruptHandler));
  }

  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    if (Touch->begin()) {
      s_touchKind = kind;
      Serial.printf("[Touch] %s ready at 0x%02X (attempt %u).\n", name, addr, attempt);
      return true;
    }
    delay(120);
  }
  Serial.printf("[Touch] no %s at 0x%02X.\n", name, addr);
  Touch.reset();
  return false;
}

void initDisplayAndTouch() {
  Wire.begin(IIC_SDA, IIC_SCL);
  i2cScan("after Wire.begin");

  // The XCA9554 expander holds the reset lines for the panel and the touch
  // controller. Pins 0/1/2 only -- that is what the vendor examples drive.
  if (expander.begin(0x20)) {
    expander.pinMode(0, OUTPUT);
    expander.pinMode(1, OUTPUT);
    expander.pinMode(2, OUTPUT);
    expander.digitalWrite(0, LOW);
    expander.digitalWrite(1, LOW);
    expander.digitalWrite(2, LOW);
    delay(20);
    expander.digitalWrite(0, HIGH);
    expander.digitalWrite(1, HIGH);
    expander.digitalWrite(2, HIGH);
    delay(50);
    Serial.println("[Expander] XCA9554 ready, resets released.");
  } else {
    Serial.println("[Expander] XCA9554 NOT found at 0x20 - panel/touch may stay in reset.");
  }
  i2cScan("after reset release");

  // Touch before the panel, which is the vendor bring-up order.
  if (!tryTouch(TOUCH_CST816, CST816T_DEVICE_ADDRESS, "CST816T")) {
    tryTouch(TOUCH_FT3168, FT3168_DEVICE_ADDRESS, "FT3168");
  }

  if (s_touchKind == TOUCH_CST816) {
    // Periodic interrupt mode makes the controller report continuously while a
    // finger is down, which is what press-and-hold needs.
    Touch->IIC_Write_Device_State(
      Touch->Arduino_IIC_Touch::Device::TOUCH_DEVICE_INTERRUPT_MODE,
      Touch->Arduino_IIC_Touch::Device_Mode::TOUCH_DEVICE_INTERRUPT_PERIODIC);
  } else if (s_touchKind == TOUCH_FT3168) {
    // ACTIVE, not MONITOR: in monitor mode the controller drops off the bus
    // between touches and NAKs every read.
    Touch->IIC_Write_Device_State(
      Touch->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
      Touch->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_ACTIVE);
  } else {
    Serial.println("[Touch] disabled - tap/hold will not work.");
  }

  // QSPI panel.
  Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3
  );

#if BOARD_REVISION_V2
  // The CO5300 panel starts 16 columns into the controller's address space;
  // without this offset the whole image is shifted sideways.
  gfx = new Arduino_CO5300(bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT, 16, 0, 0, 0);
  Serial.println("[Display] CO5300 (board revision V2)");
#else
  gfx = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);
  Serial.println("[Display] SH8601 (board revision V1)");
#endif

  if (gfx) {
    if (!gfx->begin()) Serial.println("[Display] gfx->begin() FAILED.");
    gfx->fillScreen(0x0000);              // pure AMOLED black
    gfx->setBrightness(DISPLAY_BRIGHTNESS);
  }
}

TouchPoint readTouchInput() {
  TouchPoint pt = {false, -1, -1};
  if (s_touchKind == TOUCH_NONE || !Touch) return pt;

  // The interrupt only tells us something happened; the finger count is what
  // decides whether a finger is still down, which is what press-and-hold needs.
  s_touchInterruptOccurred = false;

  int32_t fingers = Touch->IIC_Read_Device_Value(
      Touch->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER);
  if (fingers <= 0) return pt;

  pt.x = Touch->IIC_Read_Device_Value(Touch->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
  pt.y = Touch->IIC_Read_Device_Value(Touch->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
  if (pt.x >= 0 && pt.x < LCD_WIDTH && pt.y >= 0 && pt.y < LCD_HEIGHT) {
    pt.isPressed = true;
  }
  return pt;
}

bool touchAvailable() { return s_touchKind != TOUCH_NONE; }

const char *touchModelName() {
  switch (s_touchKind) {
    case TOUCH_CST816:  return "CST816T";
    case TOUCH_FT3168:  return "FT3168";
    default:            return "NONE";
  }
}

void setDisplayBrightness(uint8_t brightness) {
  if (gfx) {
    gfx->setBrightness(brightness);
  }
}
