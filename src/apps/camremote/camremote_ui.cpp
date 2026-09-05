#include "camremote_ui.h"
#include "gfx_view.h"

// =========================================================================
//  Palette
// =========================================================================
static const uint16_t C_BG       = C565(0, 0, 0);
static const uint16_t C_TITLE    = C565(255, 255, 255);
static const uint16_t C_ROW      = C565(20, 20, 20);
static const uint16_t C_LABEL    = C565(230, 230, 230);
static const uint16_t C_RULE     = C565(40, 40, 40);
static const uint16_t C_HINT     = C565(70, 68, 66);
static const uint16_t C_STATUS_OK    = C565(90, 210, 130);
static const uint16_t C_STATUS_WAIT  = C565(255, 198, 62);
static const uint16_t C_STATUS_BAD   = C565(255, 96, 96);
static const uint16_t C_BTN_IDLE     = C565(60, 130, 235);   // "SHUTTER" -- calm blue
static const uint16_t C_BTN_ACTIVE   = C565(230, 60, 50);    // "STOP" (GoPro recording) -- red
static const uint16_t C_BTN_DISABLED = C565(40, 40, 42);
static const uint16_t C_BTN_LABEL    = C565(255, 255, 255);

static const int16_t TITLE_H = 44;
static const int16_t HINT_H  = 24;

// ---- Layout, recomputed on rotation ---------------------------------------
static int16_t s_selRowH, s_selRowTop;
static int16_t s_btnX, s_btnY, s_btnW, s_btnH;
static int16_t s_statusY, s_nameY;

static void layoutRecompute() {
  s_selRowTop = TITLE_H;
  s_selRowH   = (viewH() - TITLE_H - HINT_H) / 2;

  s_btnW = viewW() - 80;
  s_btnH = 96;
  s_btnX = (viewW() - s_btnW) / 2;
  s_btnY = viewH() / 2 - s_btnH / 2 + 14;
  s_statusY = TITLE_H + 20;
  s_nameY   = s_btnY - 30;
}

// ---- State the renderer diffs against -------------------------------------
static bool            s_valid = false;
static CamRemoteScreen s_lastScreen = CR_SCREEN_SELECT;
static PhoneLinkState  s_lastPhoneState = PHONE_ADVERTISING;
static GoproLinkState  s_lastGoproState = GOPRO_SCANNING;
static bool            s_lastGoproRecording = false;
static char            s_lastGoproName[32] = "";

uint8_t camremoteUiRotation() { return viewRotation(); }

void camremoteUiSetRotation(uint8_t rot) {
  rot &= 3;
  if (rot == viewRotation()) return;
  viewSetRotation(rot);
  layoutRecompute();
  s_valid = false;
}

void camremoteUiInit() {
  layoutRecompute();
  s_valid = false;
  vFillScreen(C_BG);
}

static void drawSelectStatic() {
  vFillScreen(C_BG);
  vDrawTextCentered("CAM REMOTE", 14, 2, C_TITLE);
  vFillRect(0, TITLE_H - 1, viewW(), 1, C_RULE);

  vFillRect(0, s_selRowTop, viewW(), s_selRowH, C_ROW);
  vDrawTextCentered("PHONE", s_selRowTop + s_selRowH / 2 - 8, 2, C_LABEL);
  vFillRect(0, s_selRowTop + s_selRowH - 1, viewW(), 1, C_RULE);

  vFillRect(0, s_selRowTop + s_selRowH, viewW(), s_selRowH, C_ROW);
  vDrawTextCentered("GOPRO", s_selRowTop + s_selRowH + s_selRowH / 2 - 8, 2, C_LABEL);

  vDrawTextCentered("tap to choose", viewH() - HINT_H + 6, 1, C_HINT);
}

static const char *phoneStatusText(PhoneLinkState st) {
  return st == PHONE_CONNECTED ? "CONNECTED" : "ADVERTISING...";
}
static uint16_t phoneStatusColor(PhoneLinkState st) {
  return st == PHONE_CONNECTED ? C_STATUS_OK : C_STATUS_WAIT;
}

static const char *goproStatusText(GoproLinkState st) {
  switch (st) {
    case GOPRO_SCANNING:   return "SCANNING...";
    case GOPRO_CONNECTING: return "CONNECTING...";
    case GOPRO_PAIRING:    return "PAIRING...";
    case GOPRO_CONNECTED:  return "CONNECTED";
    default:               return "NOT FOUND - RETRYING";
  }
}
static uint16_t goproStatusColor(GoproLinkState st) {
  if (st == GOPRO_CONNECTED) return C_STATUS_OK;
  if (st == GOPRO_FAILED) return C_STATUS_BAD;
  return C_STATUS_WAIT;
}

static void drawButton(const char *label, uint16_t color, bool enabled) {
  vFillRect(s_btnX, s_btnY, s_btnW, s_btnH, enabled ? color : C_BTN_DISABLED);
  vDrawRectOutline(s_btnX, s_btnY, s_btnW, s_btnH, C_RULE);
  int16_t tw = vTextWidth(label, 3);
  vDrawText(label, (viewW() - tw) / 2, s_btnY + s_btnH / 2 - 12, 3, C_BTN_LABEL);
}

static void drawModeStatic(const char *title) {
  vFillScreen(C_BG);
  vDrawTextCentered(title, 14, 2, C_TITLE);
  vFillRect(0, TITLE_H - 1, viewW(), 1, C_RULE);
  vDrawTextCentered("tap=trigger  hold=back", viewH() - HINT_H + 6, 1, C_HINT);
}

void camremoteUiFullRedraw(const CamRemoteView &v) {
  switch (v.screen) {
    case CR_SCREEN_SELECT:
      drawSelectStatic();
      break;
    case CR_SCREEN_PHONE: {
      drawModeStatic("PHONE REMOTE");
      vDrawTextCentered(phoneStatusText(v.phoneState), s_statusY, 1, phoneStatusColor(v.phoneState));
      drawButton("SHUTTER", C_BTN_IDLE, v.phoneState == PHONE_CONNECTED);
      break;
    }
    case CR_SCREEN_GOPRO: {
      drawModeStatic("GOPRO REMOTE");
      vDrawTextCentered(goproStatusText(v.goproState), s_statusY, 1, goproStatusColor(v.goproState));
      if (v.goproState == GOPRO_CONNECTED && v.goproName[0])
        vDrawTextCentered(v.goproName, s_nameY, 1, C_HINT);
      bool connected = v.goproState == GOPRO_CONNECTED;
      drawButton(v.goproRecording ? "STOP" : "SHUTTER",
                 v.goproRecording ? C_BTN_ACTIVE : C_BTN_IDLE, connected);
      break;
    }
  }
  s_lastScreen = v.screen;
  s_lastPhoneState = v.phoneState;
  s_lastGoproState = v.goproState;
  s_lastGoproRecording = v.goproRecording;
  strncpy(s_lastGoproName, v.goproName, sizeof(s_lastGoproName) - 1);
  s_valid = true;
}

void camremoteUiRender(const CamRemoteView &v) {
  if (!s_valid || v.screen != s_lastScreen) {
    camremoteUiFullRedraw(v);
    return;
  }

  if (v.screen == CR_SCREEN_PHONE && v.phoneState != s_lastPhoneState) {
    vFillRect(0, s_statusY - 2, viewW(), 18, C_BG);
    vDrawTextCentered(phoneStatusText(v.phoneState), s_statusY, 1, phoneStatusColor(v.phoneState));
    drawButton("SHUTTER", C_BTN_IDLE, v.phoneState == PHONE_CONNECTED);
    s_lastPhoneState = v.phoneState;
  }

  if (v.screen == CR_SCREEN_GOPRO) {
    bool nameChanged = strncmp(v.goproName, s_lastGoproName, sizeof(s_lastGoproName)) != 0;
    if (v.goproState != s_lastGoproState || nameChanged) {
      vFillRect(0, s_statusY - 2, viewW(), 18, C_BG);
      vDrawTextCentered(goproStatusText(v.goproState), s_statusY, 1, goproStatusColor(v.goproState));
      vFillRect(0, s_nameY - 2, viewW(), 16, C_BG);
      if (v.goproState == GOPRO_CONNECTED && v.goproName[0])
        vDrawTextCentered(v.goproName, s_nameY, 1, C_HINT);
      s_lastGoproState = v.goproState;
      strncpy(s_lastGoproName, v.goproName, sizeof(s_lastGoproName) - 1);
    }
    if (v.goproRecording != s_lastGoproRecording || v.goproState != s_lastGoproState) {
      bool connected = v.goproState == GOPRO_CONNECTED;
      drawButton(v.goproRecording ? "STOP" : "SHUTTER",
                 v.goproRecording ? C_BTN_ACTIVE : C_BTN_IDLE, connected);
      s_lastGoproRecording = v.goproRecording;
    }
  }
}

bool camremoteUiHitPhoneRow(int16_t vx, int16_t vy) {
  (void)vx;
  return vy >= s_selRowTop && vy < s_selRowTop + s_selRowH;
}
bool camremoteUiHitGoproRow(int16_t vx, int16_t vy) {
  (void)vx;
  return vy >= s_selRowTop + s_selRowH && vy < s_selRowTop + 2 * s_selRowH;
}
bool camremoteUiHitTrigger(int16_t vx, int16_t vy) {
  return vx >= s_btnX && vx < s_btnX + s_btnW && vy >= s_btnY && vy < s_btnY + s_btnH;
}
