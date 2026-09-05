#include "camremote_hid.h"
#include "app_config.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// =========================================================================
//  Report map: a single "Consumer Control" collection with one 16-bit
//  usage-array field (report size 16, count 1) covering the full consumer
//  usage range -- a press sends the 2-byte usage code little-endian
//  (0x00E9 = Volume Increment), a release sends 0x0000. This is the same
//  "array" style consumer-control descriptor used by most USB/BLE media
//  remotes; see docs/HARDWARE.md for how it was sourced.
// =========================================================================
static const uint8_t kReportMap[] = {
  0x05, 0x0C,             // Usage Page (Consumer)
  0x09, 0x01,             // Usage (Consumer Control)
  0xA1, 0x01,             // Collection (Application)
  0x85, HID_REPORT_ID,    //   Report Id
  0x15, 0x00,             //   Logical Minimum (0)
  0x26, 0xFF, 0x03,       //   Logical Maximum (0x03FF)
  0x19, 0x00,             //   Usage Minimum (0)
  0x2A, 0xFF, 0x03,       //   Usage Maximum (0x03FF)
  0x75, 0x10,             //   Report Size (16)
  0x95, 0x01,             //   Report Count (1)
  0x81, 0x00,             //   Input (Data, Array, Absolute)
  0xC0,                   // End Collection
};

static const uint16_t kUsageVolumeUp = 0x00E9;

static NimBLEServer *s_server = nullptr;
static NimBLECharacteristic *s_input = nullptr;
static volatile bool s_connected = false;

namespace {

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
    (void)server; (void)connInfo;
    s_connected = true;
    Serial.println("[Phone] Connected.");
  }
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override {
    (void)server; (void)connInfo; (void)reason;
    s_connected = false;
    Serial.println("[Phone] Disconnected, advertising again.");
  }
};
ServerCallbacks s_serverCallbacks;

void sendUsage(uint16_t usage) {
  if (!s_input) return;
  uint8_t report[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
  s_input->setValue(report, sizeof(report));
  s_input->notify();
}

}  // namespace

void phoneModeBegin() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  // A remote that just presses a volume key needs no pairing dance -- keep
  // this permissive so any phone connects immediately.
  NimBLEDevice::setSecurityAuth(false, false, false);

  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(&s_serverCallbacks);
  s_server->advertiseOnDisconnect(true);

  NimBLEHIDDevice *hid = new NimBLEHIDDevice(s_server);
  hid->setManufacturer(BLE_DEVICE_NAME);
  hid->setPnp(0x02 /*USB_SIG*/, 0x05AC, 0x0001, 0x0100);
  hid->setHidInfo(0x00, 0x01);
  hid->setReportMap((uint8_t *)kReportMap, sizeof(kReportMap));
  s_input = hid->getInputReport(HID_REPORT_ID);

  s_server->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(BLE_DEVICE_NAME);
  adv->setAppearance(GENERIC_HID);
  adv->addServiceUUID(hid->getHidService()->getUUID());
  adv->enableScanResponse(true);
  adv->start();

  Serial.println("[Phone] Advertising as a BLE HID remote.");
}

PhoneLinkState phoneModeState() {
  return s_connected ? PHONE_CONNECTED : PHONE_ADVERTISING;
}

void phoneModeTrigger() {
  if (!s_connected) return;
  sendUsage(kUsageVolumeUp);
  delay(HID_PULSE_MS);
  sendUsage(0x0000);
}
