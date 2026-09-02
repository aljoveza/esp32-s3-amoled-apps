#include "pomodoro_ui.h"
#include "gfx_view.h"

// =========================================================================
//  Palette
// =========================================================================
static const uint16_t C_BG        = C565(0, 0, 0);
static const uint16_t C_TRACK     = C565(26, 25, 24);    // drained ring
static const uint16_t C_WORK      = C565(230, 60, 50);   // red
static const uint16_t C_BREAK     = C565(90, 200, 120);  // calm green
static const uint16_t C_LONGBREAK = C565(90, 180, 220);  // calm blue
static const uint16_t C_FLASH     = C565(255, 250, 235); // transition sweep
static const uint16_t C_DIGIT_ON  = C565(255, 255, 255);
static const uint16_t C_DIGIT_OFF = C565(26, 26, 26);
static const uint16_t C_LABEL     = C565(112, 108, 100);
static const uint16_t C_HINT      = C565(68, 66, 62);
static const uint16_t C_PAUSE     = C565(255, 198, 62);
static const uint16_t C_RESET     = C565(120, 214, 255);
static const uint16_t C_DOT_ON    = C565(255, 255, 255);
static const uint16_t C_DOT_OFF   = C565(52, 50, 47);

static uint16_t phaseColor(uint8_t phase) {
  switch (phase) {
    case PH_BREAK:      return C_BREAK;
    case PH_LONG_BREAK:  return C_LONGBREAK;
    default:             return C_WORK;
  }
}

static const char *phaseLabel(uint8_t phase) {
  switch (phase) {
    case PH_BREAK:      return "BREAK";
    case PH_LONG_BREAK:  return "LONG BREAK";
    default:             return "WORK";
  }
}

// =========================================================================
//  Ring geometry — one loop tracing the whole border, starting at the
//  top-left corner and running clockwise. Unlike the hourglass app this is
//  a single loop, not two mirrored bulbs: it drains monotonically from full
//  to empty over the phase, so no cell table is needed, only the boundary.
// =========================================================================
static int16_t TOPLEN, SIDELEN, LOOP_LEN;

static void geometryRecompute() {
  TOPLEN  = viewW() - 2 * RING_MARGIN;
  SIDELEN = viewH() - 2 * RING_MARGIN - 2 * RING_THICK;
  LOOP_LEN = 2 * TOPLEN + 2 * SIDELEN;
}

// Paints the loop between two path distances [d0, d1).
static void loopFill(int32_t d0, int32_t d1, uint16_t color) {
  if (d1 <= d0) return;
  int32_t a, b;
  int16_t M = RING_MARGIN, T = RING_THICK;

  a = d0 > 0 ? d0 : 0;
  b = d1 < TOPLEN ? d1 : TOPLEN;
  if (b > a) vFillRect(M + a, M, b - a, T, color);              // top, L->R

  int32_t base = TOPLEN;
  a = d0 > base ? d0 - base : 0;
  b = (d1 - base) < SIDELEN ? d1 - base : SIDELEN;
  if (d1 > base && b > a)
    vFillRect(viewW() - M - T, M + T + a, T, b - a, color);      // right, T->B

  base = TOPLEN + SIDELEN;
  a = d0 > base ? d0 - base : 0;
  b = (d1 - base) < TOPLEN ? d1 - base : TOPLEN;
  if (d1 > base && b > a)
    vFillRect(viewW() - M - b, viewH() - M - T, b - a, T, color); // bottom, R->L

  base = 2 * TOPLEN + SIDELEN;
  a = d0 > base ? d0 - base : 0;
  b = (d1 - base) < SIDELEN ? d1 - base : SIDELEN;
  if (d1 > base && b > a)
    vFillRect(M, viewH() - M - T - b, T, b - a, color);          // left, B->T
}

static int32_t s_filledLen = -1;   // -1 == not yet drawn
static uint16_t s_fillColor = C_WORK;

static void ringApply(float fraction, uint16_t color, bool force) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  int32_t want = (int32_t)(LOOP_LEN * fraction + 0.5f);

  if (force) {
    loopFill(0, LOOP_LEN, C_TRACK);
    loopFill(0, want, color);
    s_filledLen = want;
    s_fillColor = color;
    return;
  }
  if (want == s_filledLen) return;
  if (want < s_filledLen) loopFill(want, s_filledLen, C_TRACK);   // drained
  else                    loopFill(s_filledLen, want, color);      // grew back (rare: resume after overshoot)
  s_filledLen = want;
}

// =========================================================================
//  Centre readout
// =========================================================================
static const int16_t BIG_W = 40, BIG_H = 76, BIG_T = 9;
static const int16_t BIG_DX[4] = {0, 47, 104, 151};
static const int16_t COLON_DX = 93;
static const int16_t BIG_SPAN = 191;   // BIG_DX[3] + BIG_W
static const int16_t STACK_H = 176;

static int16_t IN_X, IN_Y, IN_W, IN_H;
static int16_t BIG_X, BIG_Y, LABEL_Y, STATUS_Y, DOTS_Y, HINT_Y;

static void layoutRecompute() {
  IN_X = RING_MARGIN + RING_THICK;
  IN_Y = RING_MARGIN + RING_THICK;
  IN_W = viewW() - 2 * IN_X;
  IN_H = viewH() - 2 * IN_Y;

  int16_t top = viewH() / 2 - STACK_H / 2;
  LABEL_Y  = top;
  BIG_Y    = top + 26;
  STATUS_Y = top + 116;
  DOTS_Y   = top + 144;
  HINT_Y   = top + 164;

  BIG_X = (viewW() - BIG_SPAN) / 2;
}

static int8_t  s_prevBig[4];
static int8_t  s_prevColon = -1;
static int8_t  s_prevPhase = -1;
static int8_t  s_prevStatus = -1;
static int8_t  s_prevSetCount = -1;
static bool    s_centerValid = false;
static int16_t s_offX = 0, s_offY = 0;
static uint8_t s_offStep = 0;
static uint32_t s_offAt = 0;

static void drawCenterStatic(const PomodoroView &v) {
  vFillRect(IN_X, IN_Y, IN_W, IN_H, C_BG);
  vDrawTextCentered("POMODORO", LABEL_Y, 1, C_LABEL, s_offX);
  vDrawTextCentered("TAP = PAUSE   HOLD = RESET", HINT_Y, 1, C_HINT, s_offX);

  for (int i = 0; i < 4; i++) s_prevBig[i] = -9;
  s_prevColon = -1;
  s_prevPhase = -1;
  s_prevStatus = -1;
  s_prevSetCount = -1;
  (void)v;
}

static void drawDots(uint8_t setCount) {
  int16_t spacing = 18;
  int16_t startX = viewW() / 2 - spacing * (SET_SIZE - 1) / 2;
  for (uint8_t i = 0; i < SET_SIZE; i++) {
    bool on = i < setCount;
    vFillCircle(startX + i * spacing + s_offX, DOTS_Y + 4 + s_offY, on ? 4 : 3,
                on ? C_DOT_ON : C_DOT_OFF);
  }
}

static void drawCenter(const PomodoroView &v, bool force) {
  if (force || (int8_t)v.phase != s_prevPhase) {
    s_prevPhase = (int8_t)v.phase;
    vFillRect(IN_X, LABEL_Y + 10 + s_offY, IN_W, 12, C_BG);
    vDrawTextCentered(phaseLabel(v.phase), LABEL_Y + 12, 1, phaseColor(v.phase), s_offX);
  }

  int8_t d[4] = {(int8_t)(v.minutes / 10), (int8_t)(v.minutes % 10),
                 (int8_t)(v.seconds / 10), (int8_t)(v.seconds % 10)};
  for (int i = 0; i < 4; i++) {
    bool f = force || s_prevBig[i] == -9;
    if (f || d[i] != s_prevBig[i]) {
      vSeg7Digit(BIG_X + BIG_DX[i] + s_offX, BIG_Y + s_offY, BIG_W, BIG_H, BIG_T,
                 d[i], s_prevBig[i], f, C_DIGIT_ON, C_DIGIT_OFF);
      s_prevBig[i] = d[i];
    }
  }

  int8_t colonOn = (v.status == PM_PAUSED || v.seconds % 2 == 0) ? 1 : 0;
  if (force || colonOn != s_prevColon) {
    s_prevColon = colonOn;
    vSeg7Colon(BIG_X + COLON_DX + s_offX, BIG_Y + s_offY, BIG_H, BIG_T,
              colonOn ? C_DIGIT_ON : C_DIGIT_OFF);
  }

  if (force || (int8_t)v.setCount != s_prevSetCount) {
    s_prevSetCount = (int8_t)v.setCount;
    drawDots(v.setCount);
  }

  if (force || (int8_t)v.status != s_prevStatus) {
    s_prevStatus = (int8_t)v.status;
    vFillRect(IN_X, STATUS_Y + s_offY - 2, IN_W, 16, C_BG);
    if (v.status == PM_PAUSED)
      vDrawTextCentered("PAUSED", STATUS_Y, 2, C_PAUSE, s_offX);
    else if (v.status == PM_HOLD)
      vDrawTextCentered("HOLD TO RESET", STATUS_Y, 1, C_PAUSE, s_offX);
    else if (v.status == PM_TRANSITION)
      vDrawTextCentered("RESET", STATUS_Y, 2, C_RESET, s_offX);
  }
}

// =========================================================================
//  Public API
// =========================================================================
uint8_t pomodoroUiRotation() { return viewRotation(); }

void pomodoroUiSetRotation(uint8_t rot) {
  rot &= 3;
  if (rot == viewRotation()) return;
  viewSetRotation(rot);
  geometryRecompute();
  layoutRecompute();
  s_centerValid = false;
}

void pomodoroUiInit() {
  geometryRecompute();
  layoutRecompute();
  s_filledLen = -1;
  s_centerValid = false;
  s_offX = s_offY = 0;
  s_offStep = 0;
  s_offAt = millis();
  vFillScreen(C_BG);
}

void pomodoroUiFullRedraw(const PomodoroView &v) {
  vFillScreen(C_BG);
  ringApply(v.fraction, phaseColor(v.phase), true);
  drawCenterStatic(v);
  drawCenter(v, true);
  s_centerValid = true;
}

static uint32_t s_transStart = 0;
static bool     s_transitioning = false;
static const uint32_t TRANS_MS = 380;

void pomodoroUiTriggerTransition() {
  s_transStart = millis();
  s_transitioning = true;
  loopFill(0, LOOP_LEN, C_FLASH);
  s_filledLen = LOOP_LEN;
  s_fillColor = C_FLASH;
}

void pomodoroUiRender(const PomodoroView &v) {
  if (!s_centerValid) {
    pomodoroUiFullRedraw(v);
    return;
  }
  uint32_t now = millis();

  if (ANTI_BURNIN_PERIOD_MS && now - s_offAt >= ANTI_BURNIN_PERIOD_MS) {
    s_offAt = now;
    s_offStep = (uint8_t)((s_offStep + 1) & 3);
    s_offX = (s_offStep == 0 || s_offStep == 3) ? -2 : 2;
    s_offY = (s_offStep < 2) ? -2 : 2;
    drawCenterStatic(v);
    drawCenter(v, true);
  }

  if (s_transitioning) {
    if (now - s_transStart >= TRANS_MS) {
      s_transitioning = false;
      ringApply(v.fraction, phaseColor(v.phase), true);   // settle into new phase colour
    }
    // else: hold the flash: the caller's `v` still describes the old phase
    // for one more frame, and force=true above already painted it bright.
  } else {
    ringApply(v.fraction, phaseColor(v.phase), false);
  }

  drawCenter(v, false);
}
