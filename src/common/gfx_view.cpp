#include "gfx_view.h"
#include "font/glcdfont.h"   // classic 5x7 bitmap font, drawn manually below

static uint8_t s_rot = 0;

uint8_t viewRotation() { return s_rot; }
void viewSetRotation(uint8_t rot) { s_rot = rot & 3; }

int16_t viewW() { return (s_rot & 1) ? LCD_HEIGHT : LCD_WIDTH; }
int16_t viewH() { return (s_rot & 1) ? LCD_WIDTH : LCD_HEIGHT; }

void vFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!gfx || w <= 0 || h <= 0) return;
  switch (s_rot) {
    case 1:  gfx->fillRect(LCD_WIDTH - y - h, x, h, w, color); break;
    case 2:  gfx->fillRect(LCD_WIDTH - x - w, LCD_HEIGHT - y - h, w, h, color); break;
    case 3:  gfx->fillRect(y, LCD_HEIGHT - x - w, h, w, color); break;
    default: gfx->fillRect(x, y, w, h, color); break;
  }
}

static void vPoint(int16_t x, int16_t y, int16_t *px, int16_t *py) {
  switch (s_rot) {
    case 1:  *px = LCD_WIDTH - 1 - y;  *py = x;                   break;
    case 2:  *px = LCD_WIDTH - 1 - x;  *py = LCD_HEIGHT - 1 - y;  break;
    case 3:  *px = y;                  *py = LCD_HEIGHT - 1 - x;  break;
    default: *px = x;                  *py = y;                   break;
  }
}

void vTouchToView(int16_t panelX, int16_t panelY, int16_t *viewX, int16_t *viewY) {
  // vPoint() maps view -> panel per rotation; this is its inverse.
  switch (s_rot) {
    case 1:  *viewX = panelY;                    *viewY = LCD_WIDTH - 1 - panelX;  break;
    case 2:  *viewX = LCD_WIDTH - 1 - panelX;     *viewY = LCD_HEIGHT - 1 - panelY; break;
    case 3:  *viewX = LCD_HEIGHT - 1 - panelY;    *viewY = panelX;                  break;
    default: *viewX = panelX;                     *viewY = panelY;                  break;
  }
}

void vFillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (!gfx) return;
  int16_t px, py;
  vPoint(x, y, &px, &py);
  gfx->fillCircle(px, py, r, color);   // a circle's shape survives a 90 turn
}

void vFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t x2, int16_t y2, uint16_t color) {
  if (!gfx) return;
  int16_t a0, b0, a1, b1, a2, b2;
  vPoint(x0, y0, &a0, &b0);
  vPoint(x1, y1, &a1, &b1);
  vPoint(x2, y2, &a2, &b2);
  gfx->fillTriangle(a0, b0, a1, b1, a2, b2, color);
}

void vFillScreen(uint16_t color) {
  if (gfx) gfx->fillScreen(color);   // orientation-independent
}

// Each font byte is one column of 8 pixels, so a column collapses into at
// most a few vertical runs instead of eight separate dots.
static void vDrawChar(int16_t x, int16_t y, unsigned char ch, uint8_t size, uint16_t color) {
  for (uint8_t col = 0; col < 5; col++) {
    uint8_t bits = font[(uint16_t)ch * 5 + col];
    uint8_t row = 0;
    while (row < 8) {
      if (!(bits & (1 << row))) { row++; continue; }
      uint8_t run = 0;
      while (row + run < 8 && (bits & (1 << (row + run)))) run++;
      vFillRect(x + col * size, y + row * size, size, run * size, color);
      row += run;
    }
  }
}

void vDrawText(const char *s, int16_t x, int16_t y, uint8_t size, uint16_t color) {
  for (const char *p = s; *p; p++) {
    vDrawChar(x, y, (unsigned char)*p, size, color);
    x += 6 * size;
  }
}

int16_t vTextWidth(const char *s, uint8_t size) {
  return (int16_t)strlen(s) * 6 * size;
}

void vDrawTextCentered(const char *s, int16_t y, uint8_t size, uint16_t color, int16_t xOffset) {
  vDrawText(s, (viewW() - vTextWidth(s, size)) / 2 + xOffset, y, size, color);
}

// ---- Seven segment ------------------------------------------------------
// bit 0..6 -> segments a,b,c,d,e,f,g
static const uint8_t SEG_MASK[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static void segRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t t,
                    uint8_t seg, int16_t *rx, int16_t *ry, int16_t *rw, int16_t *rh) {
  int16_t half = (h - t) / 2;   // centre-line offset; h == 2*half + t
  int16_t vLen = half - t;      // length of a vertical segment
  switch (seg) {
    case 0: *rx = x + t;     *ry = y;            *rw = w - 2 * t; *rh = t;    break; // a
    case 1: *rx = x + w - t; *ry = y + t;        *rw = t;         *rh = vLen; break; // b
    case 2: *rx = x + w - t; *ry = y + half + t; *rw = t;         *rh = vLen; break; // c
    case 3: *rx = x + t;     *ry = y + h - t;    *rw = w - 2 * t; *rh = t;    break; // d
    case 4: *rx = x;         *ry = y + half + t; *rw = t;         *rh = vLen; break; // e
    case 5: *rx = x;         *ry = y + t;        *rw = t;         *rh = vLen; break; // f
    default:*rx = x + t;     *ry = y + half;     *rw = w - 2 * t; *rh = t;    break; // g
  }
}

void vSeg7Digit(int16_t x, int16_t y, int16_t w, int16_t h, int16_t t,
                int8_t val, int8_t prev, bool force,
                uint16_t onColor, uint16_t offColor) {
  uint8_t now  = (val  >= 0 && val  <= 9) ? SEG_MASK[val]  : 0;
  uint8_t was  = (prev >= 0 && prev <= 9) ? SEG_MASK[prev] : 0;
  uint8_t diff = force ? 0x7F : (uint8_t)(now ^ was);
  if (!diff) return;

  for (uint8_t s = 0; s < 7; s++) {
    if (!(diff & (1 << s))) continue;
    int16_t rx, ry, rw, rh;
    segRect(x, y, w, h, t, s, &rx, &ry, &rw, &rh);
    vFillRect(rx, ry, rw, rh, (now & (1 << s)) ? onColor : offColor);
  }
}

void vSeg7Colon(int16_t x, int16_t y, int16_t h, int16_t t, uint16_t color) {
  vFillRect(x, y + h / 3 - t / 2, t, t, color);
  vFillRect(x, y + (2 * h) / 3 - t / 2, t, t, color);
}
