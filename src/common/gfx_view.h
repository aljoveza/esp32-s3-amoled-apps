#pragma once
#include <Arduino.h>
#include "display_driver.h"

// =========================================================================
//  Rotated drawing surface
//
//  The CO5300 cannot rotate in hardware -- its driver's own comment is
//  "CO5300 does not support rotation", and rotations 1/3 only set X/Y mirror
//  bits rather than swapping the axes. So apps draw into a "view" space and
//  this layer maps it onto the panel.
//
//  Draw with viewW()/viewH() as your canvas size and use these primitives
//  instead of calling gfx directly; then the app never needs to know which
//  way up the device is.
// =========================================================================

// Pack 8-bit RGB into RGB565. (Named C565 because Arduino_GFX already
// defines an RGB565 macro of its own.)
#define C565(r, g, b) \
  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// Quarter turns of the whole view, 0..3 clockwise.
void    viewSetRotation(uint8_t rot);
uint8_t viewRotation();

int16_t viewW();
int16_t viewH();

void vFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void vFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t x2, int16_t y2, uint16_t color);
void vFillScreen(uint16_t color);

// Text, drawn from the classic 5x7 font. The library's own text routines
// cannot draw rotated, so glyphs are emitted here. A character cell is
// 6*size wide and 8*size tall.
void vDrawText(const char *s, int16_t x, int16_t y, uint8_t size, uint16_t color);
void vDrawTextCentered(const char *s, int16_t y, uint8_t size, uint16_t color,
                       int16_t xOffset = 0);
int16_t vTextWidth(const char *s, uint8_t size);

// Seven-segment digit, drawn from plain rectangles.
//   val / prev : 0..9, or -1 for a blank digit
//   force      : repaint every segment rather than only the changed ones
// Passing the previously drawn value keeps the redraw cheap enough to update
// a hundredths field at 40 fps.
void vSeg7Digit(int16_t x, int16_t y, int16_t w, int16_t h, int16_t t,
                int8_t val, int8_t prev, bool force,
                uint16_t onColor, uint16_t offColor);

// Two square dots, placed like a digital clock colon.
void vSeg7Colon(int16_t x, int16_t y, int16_t h, int16_t t, uint16_t color);
