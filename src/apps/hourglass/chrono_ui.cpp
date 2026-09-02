#include "chrono_ui.h"
#include "gfx_view.h"
#include <math.h>

// =========================================================================
//  Palette
// =========================================================================
static const uint16_t C_BLACK      = 0x0000;
static const uint16_t C_TRACK      = C565(24, 25, 27);    // empty glass
static const uint16_t C_TRACK_TICK = C565(44, 52, 64);    // 10-second notch
static const uint16_t C_GRAIN      = C565(200, 228, 255); // falling sand
static const uint16_t C_DIGIT_ON   = C565(255, 255, 255);
static const uint16_t C_DIGIT_OFF  = C565(26, 26, 26);
static const uint16_t C_CS_ON      = C565(90, 170, 255);
static const uint16_t C_CS_OFF     = C565(10, 18, 34);
static const uint16_t C_LABEL      = C565(112, 108, 100);
static const uint16_t C_HINT       = C565(68, 66, 62);
static const uint16_t C_NECK       = C565(60, 110, 190);
static const uint16_t C_RUN        = C565(86, 220, 140);
static const uint16_t C_PAUSE      = C565(255, 198, 62);
static const uint16_t C_FLASH      = C565(150, 220, 255);

// Blue "sand": bright near the neck, deep and cool out at each bulb's tip.
static const uint8_t SAND_NECK[3] = {120, 190, 255};
static const uint8_t SAND_FAR[3]  = {20, 60, 150};
static const uint8_t FLIP_NECK[3] = {225, 242, 255};
static const uint8_t FLIP_FAR[3]  = {150, 200, 255};

// =========================================================================
//  Ring geometry
//
//  The band is walked as a path that starts at the top centre of the view,
//  runs right along the top edge, down the right edge, and back to the bottom
//  centre along the bottom edge. The left half is the mirror image, so
//  everything is computed once and drawn twice.
//
//  View mid-height is the waist of the hourglass: the first half of the path
//  is the top bulb, the second half the bottom bulb, and both are exactly the
//  same length -- so a grain leaving the top always has a seat at the bottom.
//
//  These are recomputed on rotation. The path total is the same either way
//  (2*(W+H) minus the corner bookkeeping), so the sand model never changes.
// =========================================================================
static int16_t CX, BW, BH, SEG_TOP, SEG_SIDE, HALF_LEN, NECK_DIST;

static const int RING_CELLS = 2 * RING_CELLS_PER_BULB;      // 1 cell == 1 second

static uint8_t s_cell[RING_CELLS];   // 0 empty, 1 sand, 0xFF unknown
static int     s_topEmpty  = -1;
static bool    s_flipTint  = false;

static uint32_t s_flipStart = 0;
static bool     s_flipping  = false;
static const uint32_t FLIP_MS = 420;

static int  s_grainD[3];
static bool s_grainsUp = false;

static void geometryRecompute() {
  CX        = viewW() / 2;
  BW        = viewW() - 2 * RING_MARGIN;
  BH        = viewH() - 2 * RING_MARGIN;
  SEG_TOP   = BW / 2;
  SEG_SIDE  = BH - 2 * RING_THICK;
  HALF_LEN  = SEG_TOP + SEG_SIDE + SEG_TOP;
  NECK_DIST = HALF_LEN / 2;
}

static inline int cellStart(int i) {
  return (int)(((long)i * HALF_LEN + RING_CELLS / 2) / RING_CELLS);
}

// Paints the slice of the band between two path distances, on both halves.
static void bandFill(int d0, int d1, uint16_t color) {
  if (d1 <= d0) return;
  int a, b;

  a = d0 > 0 ? d0 : 0;
  b = d1 < SEG_TOP ? d1 : SEG_TOP;
  if (b > a) {
    vFillRect(CX + a, RING_MARGIN, b - a, RING_THICK, color);
    vFillRect(CX - b, RING_MARGIN, b - a, RING_THICK, color);
  }

  a = d0 > SEG_TOP ? d0 : SEG_TOP;
  b = d1 < SEG_TOP + SEG_SIDE ? d1 : SEG_TOP + SEG_SIDE;
  if (b > a) {
    int16_t y = RING_MARGIN + RING_THICK + (a - SEG_TOP);
    vFillRect(viewW() - RING_MARGIN - RING_THICK, y, RING_THICK, b - a, color);
    vFillRect(RING_MARGIN, y, RING_THICK, b - a, color);
  }

  a = d0 > SEG_TOP + SEG_SIDE ? d0 : SEG_TOP + SEG_SIDE;
  b = d1 < HALF_LEN ? d1 : HALF_LEN;
  if (b > a) {
    int dd0 = a - (SEG_TOP + SEG_SIDE);
    int dd1 = b - (SEG_TOP + SEG_SIDE);
    vFillRect(viewW() - RING_MARGIN - dd1, viewH() - RING_MARGIN - RING_THICK,
              dd1 - dd0, RING_THICK, color);
    vFillRect(RING_MARGIN + dd0, viewH() - RING_MARGIN - RING_THICK,
              dd1 - dd0, RING_THICK, color);
  }
}

// Centre of the band at a given path distance, for both halves.
static void bandPoint(int d, int16_t *rx, int16_t *lx, int16_t *y) {
  if (d < SEG_TOP) {
    *rx = CX + d;
    *lx = CX - d;
    *y  = RING_MARGIN + RING_THICK / 2;
  } else if (d < SEG_TOP + SEG_SIDE) {
    *rx = viewW() - RING_MARGIN - RING_THICK / 2;
    *lx = RING_MARGIN + RING_THICK / 2;
    *y  = RING_MARGIN + RING_THICK + (d - SEG_TOP);
  } else {
    int dd = d - (SEG_TOP + SEG_SIDE);
    *rx = viewW() - RING_MARGIN - dd;
    *lx = RING_MARGIN + dd;
    *y  = viewH() - RING_MARGIN - RING_THICK / 2;
  }
}

static uint16_t cellColor(int i, bool sand) {
  if (!sand) return (i % 10 == 0) ? C_TRACK_TICK : C_TRACK;

  float k = 1.0f - fabsf((i + 0.5f) - RING_CELLS_PER_BULB) / (float)RING_CELLS_PER_BULB;
  const uint8_t *cn = s_flipTint ? FLIP_NECK : SAND_NECK;
  const uint8_t *cf = s_flipTint ? FLIP_FAR  : SAND_FAR;
  uint8_t r = (uint8_t)(cf[0] + (cn[0] - cf[0]) * k);
  uint8_t g = (uint8_t)(cf[1] + (cn[1] - cf[1]) * k);
  uint8_t b = (uint8_t)(cf[2] + (cn[2] - cf[2]) * k);
  return C565(r, g, b);
}

// Top bulb holds its sand against the neck and empties from the far tip down;
// the bottom bulb piles up from its far tip back towards the neck.
static inline bool cellIsSand(int i, int topEmpty) {
  return (i < RING_CELLS_PER_BULB)
             ? (i >= topEmpty)
             : ((i - RING_CELLS_PER_BULB) >= RING_CELLS_PER_BULB - topEmpty);
}

static void paintCell(int i, int topEmpty) {
  bool sand = cellIsSand(i, topEmpty);
  bandFill(cellStart(i), cellStart(i + 1), cellColor(i, sand));
  s_cell[i] = sand ? 1 : 0;
}

static void ringApply(float p, bool force) {
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  int topEmpty = (int)(RING_CELLS_PER_BULB * p + 0.5f);
  if (topEmpty > RING_CELLS_PER_BULB) topEmpty = RING_CELLS_PER_BULB;

  if (!force && topEmpty == s_topEmpty) return;
  s_topEmpty = topEmpty;

  for (int i = 0; i < RING_CELLS; i++) {
    uint8_t want = cellIsSand(i, topEmpty) ? 1 : 0;
    if (force || s_cell[i] != want) paintCell(i, topEmpty);
  }
}

static void ringRepaintAround(int d) {
  int i0 = (int)(((long)(d - 6) * RING_CELLS) / HALF_LEN) - 1;
  int i1 = (int)(((long)(d + 6) * RING_CELLS) / HALF_LEN) + 1;
  if (i0 < 0) i0 = 0;
  if (i1 > RING_CELLS - 1) i1 = RING_CELLS - 1;
  for (int i = i0; i <= i1; i++) paintCell(i, s_topEmpty);
}

// A thin trickle of sand running from the neck down to the top of the pile.
static void ringGrains(bool running, uint32_t nowMs) {
  if (s_grainsUp) {
    for (int k = 0; k < 3; k++) ringRepaintAround(s_grainD[k]);
    s_grainsUp = false;
  }
  if (!running || s_flipping) return;

  int pileTop = RING_CELLS - s_topEmpty;
  int gap     = cellStart(pileTop) - NECK_DIST;
  if (gap < 16) return;

  for (int k = 0; k < 3; k++) {
    float off = fmodf(nowMs * 0.16f + k * (gap / 3.0f), (float)gap);
    int d = NECK_DIST + (int)off;
    s_grainD[k] = d;
    int16_t rx, lx, y;
    bandPoint(d, &rx, &lx, &y);
    vFillRect(rx - 3, y - 3, 6, 6, C_GRAIN);
    vFillRect(lx - 3, y - 3, 6, 6, C_GRAIN);
  }
  s_grainsUp = true;
}

// =========================================================================
//  Centre readout
//
//  Laid out as one vertical stack centred in the view, so the same code fits
//  both the tall and the wide orientation.
// =========================================================================
static const int16_t BIG_W = 36, BIG_H = 64, BIG_T = 8;
static const int16_t CS_W = 22, CS_H = 36, CS_T = 5;
static const int16_t BIG_DX[6] = {0, 43, 100, 143, 200, 243};
static const int16_t COLON_DX[2] = {89, 189};
static const int16_t BIG_SPAN = 279;   // BIG_DX[5] + BIG_W
static const int16_t CS_SPAN  = 59;    // dot + gap + two digits
static const int16_t STACK_H  = 224;

static int16_t IN_X, IN_Y, IN_W, IN_H;
static int16_t BIG_X, BIG_Y, CS_X, CS_Y;
static int16_t LABEL_Y, STATUS_Y, HINT_Y, HINT2_Y, DIAG_Y;

static void layoutRecompute() {
  IN_X = RING_MARGIN + RING_THICK;
  IN_Y = RING_MARGIN + RING_THICK;
  IN_W = viewW() - 2 * IN_X;
  IN_H = viewH() - 2 * IN_Y;

  int16_t top = viewH() / 2 - STACK_H / 2;
  LABEL_Y  = top;
  BIG_Y    = top + 28;
  CS_Y     = top + 106;
  STATUS_Y = top + 160;
  HINT_Y   = top + 190;
  HINT2_Y  = top + 202;
  DIAG_Y   = top + 216;

  BIG_X = (viewW() - BIG_SPAN) / 2;
  CS_X  = (viewW() - CS_SPAN) / 2;
}

static bool    s_tiltHint = true;
static char    s_diag[42] = {0};
static char    s_diagDrawn[42] = {0};
static int8_t  s_prevBig[6];
static int8_t  s_prevCs[2];
static int8_t  s_prevColon = -1;
static int8_t  s_prevStatus = -1;
static bool    s_centerValid = false;
static int16_t s_offX = 0, s_offY = 0;
static uint8_t s_offStep = 0;
static uint32_t s_offAt = 0;

static void printCentered(const char *s, int16_t y, uint8_t size, uint16_t color) {
  vDrawTextCentered(s, y + s_offY, size, color, s_offX);
}

static void statusText(uint8_t st, const char **txt, uint16_t *col) {
  switch (st) {
    case CH_PAUSED:  *txt = "PAUSED";        *col = C_PAUSE; break;
    case CH_HOLD:    *txt = "HOLD TO RESET"; *col = C_PAUSE; break;
    case CH_RESET:   *txt = "RESET";         *col = C_FLASH; break;
    case CH_FLIPPED: *txt = "FLIPPED";       *col = C_FLASH; break;
    default:         *txt = "RUNNING";       *col = C_RUN;   break;
  }
}

static void drawCenterStatic() {
  vFillRect(IN_X, IN_Y, IN_W, IN_H, C_BLACK);

  // The waist of the glass, marked just inside the band at mid-height.
  int16_t my = viewH() / 2;
  vFillTriangle(IN_X, my - 8, IN_X, my + 8, IN_X + 9, my, C_NECK);
  vFillTriangle(viewW() - IN_X, my - 8, viewW() - IN_X, my + 8,
                viewW() - IN_X - 9, my, C_NECK);

  printCentered("E L A P S E D", LABEL_Y, 1, C_LABEL);
  printCentered("TAP=PAUSE  HOLD=RESET", HINT_Y, 1, C_HINT);
  if (s_tiltHint) printCentered("TURN 90" "\xF8" " TO RESTART", HINT2_Y, 1, C_HINT);

  // Decimal point in front of the hundredths.
  vFillRect(CS_X + s_offX, CS_Y + CS_H - CS_T + s_offY, CS_T, CS_T, C_CS_ON);

  for (int i = 0; i < 6; i++) s_prevBig[i] = -9;
  s_prevCs[0] = s_prevCs[1] = -9;
  s_prevColon = -1;
  s_prevStatus = -1;
  s_diagDrawn[0] = 0;
}

static void drawCenter(const ChronoView &v, bool force) {
  int8_t d[6] = {(int8_t)(v.hours / 10),   (int8_t)(v.hours % 10),
                 (int8_t)(v.minutes / 10), (int8_t)(v.minutes % 10),
                 (int8_t)(v.seconds / 10), (int8_t)(v.seconds % 10)};

  for (int i = 0; i < 6; i++) {
    bool f = force || s_prevBig[i] == -9;
    if (f || d[i] != s_prevBig[i]) {
      vSeg7Digit(BIG_X + BIG_DX[i] + s_offX, BIG_Y + s_offY, BIG_W, BIG_H, BIG_T,
                 d[i], s_prevBig[i], f, C_DIGIT_ON, C_DIGIT_OFF);
      s_prevBig[i] = d[i];
    }
  }

  // Colons breathe once a second, and hold steady while paused.
  int8_t colonOn = (v.status == CH_PAUSED || v.hundredths < 55) ? 1 : 0;
  if (force || colonOn != s_prevColon) {
    s_prevColon = colonOn;
    uint16_t c = colonOn ? C_DIGIT_ON : C_DIGIT_OFF;
    for (int i = 0; i < 2; i++)
      vSeg7Colon(BIG_X + COLON_DX[i] + s_offX, BIG_Y + s_offY, BIG_H, BIG_T, c);
  }

  int8_t cs[2] = {(int8_t)(v.hundredths / 10), (int8_t)(v.hundredths % 10)};
  for (int i = 0; i < 2; i++) {
    bool f = force || s_prevCs[i] == -9;
    if (f || cs[i] != s_prevCs[i]) {
      vSeg7Digit(CS_X + 10 + i * (CS_W + 5) + s_offX, CS_Y + s_offY, CS_W, CS_H, CS_T,
                 cs[i], s_prevCs[i], f, C_CS_ON, C_CS_OFF);
      s_prevCs[i] = cs[i];
    }
  }

  if (s_diag[0] && strcmp(s_diag, s_diagDrawn) != 0) {
    strncpy(s_diagDrawn, s_diag, sizeof(s_diagDrawn) - 1);
    s_diagDrawn[sizeof(s_diagDrawn) - 1] = 0;
    vFillRect(IN_X, DIAG_Y + s_offY - 1, IN_W, 10, C_BLACK);
    printCentered(s_diag, DIAG_Y, 1, C_LABEL);
  }

  if (force || (int8_t)v.status != s_prevStatus) {
    s_prevStatus = (int8_t)v.status;
    const char *txt;
    uint16_t col;
    statusText(v.status, &txt, &col);
    vFillRect(IN_X, STATUS_Y + s_offY - 2, IN_W, 20, C_BLACK);
    printCentered(txt, STATUS_Y, 2, col);
  }
}

// =========================================================================
//  Public API
// =========================================================================
void chronoUiSetTiltHint(bool enabled) { s_tiltHint = enabled; }

void chronoUiSetDiagLine(const char *text) {
  strncpy(s_diag, text ? text : "", sizeof(s_diag) - 1);
  s_diag[sizeof(s_diag) - 1] = 0;
}

uint8_t chronoUiRotation() { return viewRotation(); }

void chronoUiSetRotation(uint8_t rot) {
  rot &= 3;
  if (rot == viewRotation()) return;
  viewSetRotation(rot);
  geometryRecompute();
  layoutRecompute();
  s_centerValid = false;    // forces a full repaint on the next render
}

void chronoUiInit() {
  geometryRecompute();
  layoutRecompute();
  memset(s_cell, 0xFF, sizeof(s_cell));
  s_topEmpty = -1;
  s_flipping = false;
  s_flipTint = false;
  s_grainsUp = false;
  s_centerValid = false;
  s_offX = s_offY = 0;
  s_offStep = 0;
  s_offAt = millis();
  vFillScreen(C_BLACK);
}

void chronoUiFullRedraw(const ChronoView &v) {
  if (!gfx) return;
  vFillScreen(C_BLACK);
  memset(s_cell, 0xFF, sizeof(s_cell));
  s_topEmpty = -1;
  s_grainsUp = false;
  ringApply(v.minuteFraction, true);
  drawCenterStatic();
  drawCenter(v, true);
  s_centerValid = true;
}

void chronoUiTriggerFlip() {
  s_flipStart = millis();
  s_flipping = true;
  s_flipTint = true;
  if (s_grainsUp) {
    for (int k = 0; k < 3; k++) ringRepaintAround(s_grainD[k]);
    s_grainsUp = false;
  }
  ringApply(1.0f, true);   // repaint with the bright flip tint
}

bool chronoUiFlipping() { return s_flipping; }

void chronoUiRender(const ChronoView &v) {
  if (!gfx) return;
  uint32_t now = millis();

  if (!s_centerValid) {
    chronoUiFullRedraw(v);
    return;
  }

  // Nudge the readout around a small box so nothing burns in over a long run.
  if (ANTI_BURNIN_PERIOD_MS && now - s_offAt >= ANTI_BURNIN_PERIOD_MS) {
    s_offAt = now;
    s_offStep = (uint8_t)((s_offStep + 1) & 3);
    s_offX = (s_offStep == 0 || s_offStep == 3) ? -2 : 2;
    s_offY = (s_offStep < 2) ? -2 : 2;
    drawCenterStatic();
    drawCenter(v, true);
  }

  float p = v.minuteFraction;
  if (s_flipping) {
    uint32_t e = now - s_flipStart;
    if (e >= FLIP_MS) {
      s_flipping = false;
      s_flipTint = false;
      ringApply(p, true);            // back to the normal sand colours
    } else {
      float k = e / (float)FLIP_MS;
      p = 1.0f - (k * k * (3.0f - 2.0f * k));   // smoothstep, full -> empty
      ringApply(p, false);
    }
  } else {
    ringApply(p, false);
  }

  ringGrains(v.running, now);
  drawCenter(v, false);
}
