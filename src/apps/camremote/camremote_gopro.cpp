#include "camremote_gopro.h"
#include "app_config.h"
#include <NimBLEDevice.h>

// =========================================================================
//  GoPro's "Command" service is registered under the 16-bit Bluetooth SIG
//  UUID 0xFEA6; the characteristics on it are GoPro's own 128-bit UUIDs.
//  Confirmed against the community-maintained protocol reference (GoPro
//  itself doesn't publish plain UUID tables) -- see docs/HARDWARE.md.
// =========================================================================
static const NimBLEUUID kServiceUUID((uint16_t)0xFEA6);
static const NimBLEUUID kCmdRequestUUID("b5f90072-aa8d-11e3-9046-0002a5d5c51b");
static const NimBLEUUID kCmdResponseUUID("b5f90073-aa8d-11e3-9046-0002a5d5c51b");

// TLV framing: [total length][command ID][param length][param value].
// Command 0x01 is shutter/record; param 1 = start, 0 = stop. Photo mode
// ignores the stop half (a single "start" is a complete photo trigger);
// video mode uses start/stop as record begin/end -- see this app's README.
static const uint8_t kShutterStart[] = {0x03, 0x01, 0x01, 0x01};
static const uint8_t kShutterStop[]  = {0x03, 0x01, 0x01, 0x00};

static GoproLinkState s_state = GOPRO_SCANNING;
static uint32_t s_stateEnteredMs = 0;
static NimBLEClient *s_client = nullptr;
static NimBLERemoteCharacteristic *s_cmdChar = nullptr;
static bool s_recording = false;
static char s_deviceName[32] = "";

// Set by the scan callback (BLE host task), consumed by goproModeUpdate()
// on the main loop -- connecting is not something to do from inside a scan
// callback.
static const NimBLEAdvertisedDevice *volatile s_pendingDevice = nullptr;

static void enterState(GoproLinkState st) {
  s_state = st;
  s_stateEnteredMs = millis();
}

static void startScan() {
  enterState(GOPRO_SCANNING);
  s_deviceName[0] = 0;
  s_cmdChar = nullptr;
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->start(GOPRO_SCAN_S, false);
  Serial.println("[GoPro] Scanning...");
}

namespace {

void onCmdResponse(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t len, bool isNotify) {
  (void)chr; (void)isNotify;
  Serial.printf("[GoPro] Response:");
  for (size_t i = 0; i < len; i++) Serial.printf(" %02X", data[i]);
  Serial.println();
}

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient *client) override {
    Serial.println("[GoPro] Connected, securing link...");
    enterState(GOPRO_PAIRING);
    client->secureConnection();
  }
  void onConnectFail(NimBLEClient *client, int reason) override {
    (void)client;
    Serial.printf("[GoPro] Connect failed (reason %d).\n", reason);
    enterState(GOPRO_FAILED);
  }
  void onAuthenticationComplete(NimBLEConnInfo &connInfo) override {
    if (!connInfo.isEncrypted()) {
      Serial.println("[GoPro] Pairing finished but link isn't encrypted -- giving up.");
      enterState(GOPRO_FAILED);
      return;
    }
    NimBLERemoteService *svc = s_client->getService(kServiceUUID);
    NimBLERemoteCharacteristic *resp = svc ? svc->getCharacteristic(kCmdResponseUUID) : nullptr;
    s_cmdChar = svc ? svc->getCharacteristic(kCmdRequestUUID) : nullptr;
    if (!svc || !s_cmdChar || !resp) {
      Serial.println("[GoPro] Command service/characteristics not found.");
      enterState(GOPRO_FAILED);
      return;
    }
    resp->subscribe(true, onCmdResponse);
    s_recording = false;
    enterState(GOPRO_CONNECTED);
    Serial.println("[GoPro] Ready.");
  }
  void onDisconnect(NimBLEClient *client, int reason) override {
    (void)client;
    Serial.printf("[GoPro] Disconnected (reason %d).\n", reason);
    s_cmdChar = nullptr;
    enterState(GOPRO_FAILED);   // goproModeUpdate() restarts the scan after a short pause
  }
};
ClientCallbacks s_clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *device) override {
    if (!device->haveName()) return;
    if (device->getName().rfind(GOPRO_NAME_PREFIX, 0) != 0) return;   // not a GoPro
    NimBLEDevice::getScan()->stop();
    strncpy(s_deviceName, device->getName().c_str(), sizeof(s_deviceName) - 1);
    s_pendingDevice = device;
  }
};
ScanCallbacks s_scanCallbacks;

}  // namespace

void goproModeBegin() {
  NimBLEDevice::init("");
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);   // "Just Works" -- no screen/keypad to pair with
  NimBLEDevice::setSecurityAuth(true, false, true);            // bond, no MITM, secure connections

  NimBLEDevice::getScan()->setScanCallbacks(&s_scanCallbacks);

  s_client = NimBLEDevice::createClient();
  s_client->setClientCallbacks(&s_clientCallbacks, false);

  startScan();
}

void goproModeUpdate(uint32_t nowMs) {
  if (s_pendingDevice) {
    const NimBLEAdvertisedDevice *dev = s_pendingDevice;
    s_pendingDevice = nullptr;
    Serial.printf("[GoPro] Found '%s', connecting...\n", s_deviceName);
    enterState(GOPRO_CONNECTING);
    if (!s_client->connect(dev)) enterState(GOPRO_FAILED);
    return;
  }

  if ((s_state == GOPRO_CONNECTING || s_state == GOPRO_PAIRING) &&
      nowMs - s_stateEnteredMs > GOPRO_CONNECT_TIMEOUT_MS) {
    Serial.println("[GoPro] Timed out connecting/pairing.");
    s_client->disconnect();
    enterState(GOPRO_FAILED);
  }

  if (s_state == GOPRO_FAILED && nowMs - s_stateEnteredMs > GOPRO_RETRY_DELAY_MS) {
    startScan();
  }
}

GoproLinkState goproModeState() { return s_state; }
const char *goproModeDeviceName() { return s_deviceName; }
bool goproModeIsRecording() { return s_recording; }

void goproModeTrigger() {
  if (s_state != GOPRO_CONNECTED || !s_cmdChar) return;
  s_recording = !s_recording;
  const uint8_t *cmd = s_recording ? kShutterStart : kShutterStop;
  size_t len = s_recording ? sizeof(kShutterStart) : sizeof(kShutterStop);
  s_cmdChar->writeValue(cmd, len, true);
}
