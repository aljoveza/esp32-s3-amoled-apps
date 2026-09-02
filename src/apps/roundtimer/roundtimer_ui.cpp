#include "roundtimer_ui.h"
#include "gfx_view.h"
#include <math.h>

// =========================================================================
//  Palette
// =========================================================================
static const uint16_t C_BG        = C565(0, 0, 0);
static const uint16_t C_TRACK     = C565(26, 25, 24);
static const uint16_t C_CALM      = C565(60, 205, 195);   // teal -- this app's own colour
static const uint16_t C_WARNING   = C565(255, 170, 50);   // amber
static const uint16_t C_DANGER    = C565(255, 70, 60);    // red
static const uint16_t C_FLASH     = C565(255, 250, 240);
static const uint16_t C_DIGIT_ON  = C565(255, 255, 255);
static const uint16_t C_DIGIT_OFF = C565(26, 26, 26);
static const uint16_t C_LABEL     = C565(112, 108, 100);
static const uint16_t C_HINT      = C565(68, 66, 62);
static const uint16_t C_PAUSE     = C565(255, 198, 62);
static const uint16_t C_ARMED     = C565(150, 210, 255);

static uint16_t tierColor(uint16_t secondsRemaining, bool armed) {
  if (armed) return C_ARMED;
  if (secondsRemaining <= DANGER_SEC) return C_DANGER;
  if (secondsRemaining <= WARNING_SEC) return C_WARNING;
  return C_CALM;
}

// =========================================================================
//  Ring geometry -- one loop tracing the whole border, same construction as
//  the pomodoro app's ring (verified exact tiling there; unchanged here).
// =========================================================================
static int16_t TOPLEN, SIDELEN, LOOP_LEN;

static void geometryRecompute() {
  TOPLEN  = viewW() - 2 * RING_MARGIN;
  SIDELEN = viewH() - 2 * RING_MARGIN - 2 * RING_THICK;
  LOOP_LEN = 2 * TOPLEN + 2 * SIDELEN;
}

static void loopFill(int32_t d0, int32_t d1, uint16_t color) {
  if (d1 <= d0) return;
  int32_t a, b;
  int16_t M = RING_MARGIN, T = RING_THICK;

  a = d0 > 0 ? d0 : 0;
  b = d1 < TOPLEN ? d1 : TOPLEN;
  if (b > a) vFillRect(M + a, M, b - a, T, color);

  int32_t base = TOPLEN;
  a = d0 > base ? d0 - base : 0;
  b = (d1 - base) < SIDELEN ? d1 - base : SIDELEN;
  if (d1 > base && b > a) vFillRect(viewW() - M - T, M + T + a, T, b - a, color);

  base = TOPLEN + SIDELEN;
  a = d0 > base ? d0 - base : 0;
  b = (d1 - base) < TOPLEN ? d1 - base : TOPLEN;
  if (d1 > base && b > a) vFillRect(viewW() - M - b, viewH() - M - T, b - a, T, color);

  base = 2 * TOPLEN + SIDELEN;
  a = d0 > base ? d0 - base : 0;
  b = (d1 - base) < SIDELEN ? d1 - base : SIDELEN;
  if (d1 > base && b > a) vFillRect(M, viewH() - M - T - b, T, b - a, color);
}

static int32_t s_filledLen = -1;
static uint16_t s_prevRingColor = 0xFFFF;

static void ringApplyFull(float fraction, uint16_t color) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  int32_t want = (int32_t)(LOOP_LEN * fraction + 0.5f);
  loopFill(0, LOOP_LEN, C_TRACK);
  loopFill(0, want, color);
  s_prevRingColor = color;
  s_filledLen = want;
}

static void ringApplyDelta(float fraction, uint16_t color) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  int32_t want = (int32_t)(LOOP_LEN * fraction + 0.5f);

  // The fill AMOUNT is what's normally diffed, but a colour change with no
  // amount change (e.g. a fresh round starting at the same fill level the
  // alarm hold left the ring at) would otherwise go unpainted -- the ring
  // would sit there in the old colour until something forced a full
  // repaint. Bug found in testing: reported as "won't update unless I turn
  // the device" (rotation is the only thing that forces one).
  if (want == s_filledLen && color == s_prevRingColor) return;
  if (color != s_prevRingColor) {
    loopFill(0, LOOP_LEN, C_TRACK);
    loopFill(0, want, color);
  } else if (want < s_filledLen) {
    loopFill(want, s_filledLen, C_TRACK);
  } else {
    loopFill(s_filledLen, want, color);
  }
  s_filledLen = want;
  s_prevRingColor = color;
}

// =========================================================================
//  Centre readout
// =========================================================================
static const int16_t BIG_W = 44, BIG_H = 80, BIG_T = 10;
static const int16_t BIG_DX[4] = {0, 51, 112, 163};
static const int16_t COLON_DX = 100;
static const int16_t BIG_SPAN = 207;
static const int16_t STACK_H = 160;

static int16_t IN_X, IN_Y, IN_W, IN_H;
static int16_t BIG_X, BIG_Y, LABEL_Y, STATUS_Y, HINT_Y;

static void layoutRecompute() {
  IN_X = RING_MARGIN + RING_THICK;
  IN_Y = RING_MARGIN + RING_THICK;
  IN_W = viewW() - 2 * IN_X;
  IN_H = viewH() - 2 * IN_Y;

  int16_t top = viewH() / 2 - STACK_H / 2;
  LABEL_Y  = top;
  BIG_Y    = top + 24;
  STATUS_Y = top + 116;
  HINT_Y   = top + 142;

  BIG_X = (viewW() - BIG_SPAN) / 2;
}

static int8_t   s_prevBig[4];
static int8_t   s_prevColon = -1;
static int8_t   s_prevState = -1;
static uint32_t s_prevRound = 0xFFFFFFFF;
static bool     s_centerValid = false;

static void drawCenterStatic() {
  vFillRect(IN_X, IN_Y, IN_W, IN_H, C_BG);
  vDrawTextCentered("ROUND TIMER", LABEL_Y, 1, C_LABEL);
  for (int i = 0; i < 4; i++) s_prevBig[i] = -9;
  s_prevColon = -1;
  s_prevState = -1;
  s_prevRound = 0xFFFFFFFF;
}

static uint16_t s_prevDigitColor = 0xFFFF;

static void drawCenter(const RoundTimerView &v, bool force, uint16_t digitColor) {
  // vSeg7Digit only diffs a digit's VALUE, not its colour -- so a colour-only
  // change (a new round starting in a different tier colour, while a given
  // digit's numeric value happens not to change) would otherwise never get
  // repainted, leaving that digit stuck in the old colour. Bug found in
  // testing: reported as needing to rotate the device to fix it (rotation
  // is the only thing that forces every digit to redraw regardless).
  if (digitColor != s_prevDigitColor) force = true;
  s_prevDigitColor = digitColor;

  uint16_t mm = v.secondsRemaining / 60, ss = v.secondsRemaining % 60;
  int8_t d[4] = {(int8_t)(mm / 10), (int8_t)(mm % 10), (int8_t)(ss / 10), (int8_t)(ss % 10)};
  for (int i = 0; i < 4; i++) {
    bool f = force || s_prevBig[i] == -9;
    if (f || d[i] != s_prevBig[i]) {
      vSeg7Digit(BIG_X + BIG_DX[i], BIG_Y, BIG_W, BIG_H, BIG_T,
                d[i], s_prevBig[i], f, digitColor, C_DIGIT_OFF);
      s_prevBig[i] = d[i];
    }
  }
  int8_t colonOn = (v.state == RT_PAUSED || v.state == RT_ARMED || ss % 2 == 0) ? 1 : 0;
  if (force || colonOn != s_prevColon) {
    s_prevColon = colonOn;
    vSeg7Colon(BIG_X + COLON_DX, BIG_Y, BIG_H, BIG_T, colonOn ? digitColor : C_DIGIT_OFF);
  }

  if (force || (int8_t)v.state != s_prevState || v.roundNumber != s_prevRound) {
    s_prevState = (int8_t)v.state;
    s_prevRound = v.roundNumber;
    vFillRect(IN_X, STATUS_Y - 2, IN_W, 20, C_BG);
    char line[24];
    switch (v.state) {
      case RT_ARMED:   snprintf(line, sizeof(line), "READY"); break;
      case RT_PAUSED:  snprintf(line, sizeof(line), "PAUSED"); break;
      case RT_ALARM:   snprintf(line, sizeof(line), "TIME'S UP"); break;
      default:         snprintf(line, sizeof(line), "ROUND %lu", (unsigned long)v.roundNumber); break;
    }
    uint16_t c = (v.state == RT_PAUSED) ? C_PAUSE : (v.state == RT_ALARM) ? C_DANGER : C_LABEL;
    vDrawTextCentered(line, STATUS_Y, 2, c);

    vFillRect(IN_X, HINT_Y - 1, IN_W, 18, C_BG);
    if (v.state == RT_ARMED) {
      char hint[32];
      snprintf(hint, sizeof(hint), "-%us   START   +%us",
              (unsigned)ROUND_SECONDS_STEP, (unsigned)ROUND_SECONDS_STEP);
      vDrawTextCentered(hint, HINT_Y, 1, C_HINT);
    } else {
      vDrawTextCentered("tap=pause  hold=stop", HINT_Y, 1, C_HINT);
    }
  }
}

// =========================================================================
//  Public API
// =========================================================================
uint8_t roundtimerUiRotation() { return viewRotation(); }

void roundtimerUiSetRotation(uint8_t rot) {
  rot &= 3;
  if (rot == viewRotation()) return;
  viewSetRotation(rot);
  geometryRecompute();
  layoutRecompute();
  s_centerValid = false;
}

RTZone roundtimerUiZoneAt(int16_t viewX, int16_t viewY) {
  if (viewX < 0 || viewY < 0 || viewX >= viewW() || viewY >= viewH()) return RT_ZONE_NONE;
  int16_t third = viewW() / 3;
  if (viewX < third) return RT_ZONE_DECREASE;
  if (viewX < 2 * third) return RT_ZONE_START;
  return RT_ZONE_INCREASE;
}

void roundtimerUiInit() {
  geometryRecompute();
  layoutRecompute();
  s_filledLen = -1;
  s_centerValid = false;
  vFillScreen(C_BG);
}

void roundtimerUiFullRedraw(const RoundTimerView &v) {
  vFillScreen(C_BG);
  ringApplyFull(v.fraction, tierColor(v.secondsRemaining, v.state == RT_ARMED));
  drawCenterStatic();
  drawCenter(v, true, tierColor(v.secondsRemaining, v.state == RT_ARMED));
  s_centerValid = true;
}

static uint32_t s_alarmStart = 0;
static bool     s_alarming = false;
static const uint32_t ALARM_SWEEP_MS = 500;

void roundtimerUiTriggerAlarm() {
  s_alarmStart = millis();
  s_alarming = true;
  loopFill(0, LOOP_LEN, C_FLASH);
  s_filledLen = LOOP_LEN;
}

void roundtimerUiRender(const RoundTimerView &v) {
  if (!s_centerValid) {
    roundtimerUiFullRedraw(v);
    return;
  }
  uint32_t now = millis();
  uint16_t color = tierColor(v.secondsRemaining, v.state == RT_ARMED);

  if (s_alarming) {
    if (now - s_alarmStart >= ALARM_SWEEP_MS) {
      s_alarming = false;
      ringApplyFull(v.fraction, color);
    }
    // force=false: the digit-colour-changed check inside drawCenter already
    // forces the first alarm frame (colour just switched to C_FLASH), and
    // the ring stays a steady flash colour for the rest of the hold, so
    // there is nothing left to repaint on the frames in between.
    drawCenter(v, false, C_FLASH);
    return;
  }

  // Calm, warning and danger all render identically -- only tierColor()
  // differs. There WAS a danger-zone-only full-screen background flash
  // here (a periodic red fill behind the digits, meant to read as a
  // pulse). Removed: it forced large-area erase-and-redraw cycles that
  // don't happen anywhere else in this app, and even throttled to twice a
  // second it was visibly flickering on real hardware -- reported as "a
  // red layer on top of the timer... making it flick a lot". Ring colour
  // + digit colour + the tick beeps carry the escalation now, the same
  // small-targeted-redraw pattern every other app on this board already
  // uses without issue.
  ringApplyDelta(v.fraction, color);
  drawCenter(v, false, color);
}
