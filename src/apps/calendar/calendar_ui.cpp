#include "calendar_ui.h"
#include "gfx_view.h"
#include <string.h>

// =========================================================================
//  Palette -- a dark dashboard: two rounded "cards" (NOW in blue, NEXT in
//  violet) floating on a near-black page, rather than plain rows of text.
// =========================================================================
static const uint16_t C_BG         = C565(6, 7, 10);
static const uint16_t C_CARD_NOW   = C565(19, 24, 36);
static const uint16_t C_CARD_NEXT  = C565(24, 20, 34);
static const uint16_t C_ACCENT_NOW  = C565(90, 176, 255);
static const uint16_t C_ACCENT_NEXT = C565(180, 138, 255);
static const uint16_t C_TITLE      = C565(244, 246, 250);
static const uint16_t C_DUR        = C565(150, 158, 176);
static const uint16_t C_EMPTY      = C565(94, 98, 112);
static const uint16_t C_TRACK      = C565(38, 42, 54);
static const uint16_t C_HEADER     = C565(96, 100, 116);
static const uint16_t C_STATUS     = C565(255, 196, 80);

static const uint16_t C_BATT_BG     = C565(20, 22, 28);
static const uint16_t C_BATT_GREEN  = C565(100, 220, 140);
static const uint16_t C_BATT_BLUE   = C565(110, 178, 255);
static const uint16_t C_BATT_YELLOW = C565(255, 210, 70);
static const uint16_t C_BATT_RED    = C565(255, 105, 105);

// ---- Text sizing ----------------------------------------------------------
// vDrawText clamps any requested size below 2 up to 2 (see gfx_view.cpp), so
// LABEL_SIZE/DUR_SIZE end up the same physical height as each other -- the
// hierarchy comes from colour/weight-by-repetition, not those two sizes.
static const uint8_t LABEL_SIZE = 1;    // -> 16px tall
static const uint8_t TITLE_SIZE = 3;    // -> 24px tall
static const uint8_t DUR_SIZE   = 1;    // -> 16px tall

static const int16_t OUTER_MARGIN = 14;
static const int16_t CARD_R       = 16;
static const int16_t CARD_PAD_Y   = 16;
static const int16_t CARD_PAD_L   = 32;   // room for accent bar + live dot + gap
static const int16_t CARD_PAD_R   = 18;
static const int16_t ACCENT_X     = 10;   // accent bar sits at s_cardX + ACCENT_X
static const int16_t ACCENT_W     = 4;
static const int16_t ACCENT_R     = 2;
static const int16_t DOT_X        = 21;   // live dot centre at s_cardX + DOT_X
static const int16_t DOT_R        = 3;

static const int16_t LABEL_H  = 16;
static const int16_t TITLE_H  = 24;
static const int16_t DUR_H    = 16;
static const int16_t PROGRESS_H = 7;
static const int16_t PROGRESS_R = 3;

static const int16_t GAP_HEADER_TITLE = 8;
static const int16_t GAP_TITLE_DUR    = 6;
static const int16_t GAP_DUR_PROGRESS = 12;
static const int16_t GAP_BETWEEN_CARDS = 16;

// =========================================================================
//  Rounded-rect fill -- built from existing gfx_view primitives (a centre
//  rect, two side strips, four corner circles). No dedicated primitive for
//  this in gfx_view.h, and this is purely a cosmetic need of this one app.
// =========================================================================
static void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  if (r * 2 > w) r = w / 2;
  if (r * 2 > h) r = h / 2;
  if (r < 1) { vFillRect(x, y, w, h, color); return; }

  vFillRect(x + r, y, w - 2 * r, h, color);
  vFillRect(x, y + r, r, h - 2 * r, color);
  vFillRect(x + w - r, y + r, r, h - 2 * r, color);
  vFillCircle(x + r, y + r, r, color);
  vFillCircle(x + w - r - 1, y + r, r, color);
  vFillCircle(x + r, y + h - r - 1, r, color);
  vFillCircle(x + w - r - 1, y + h - r - 1, r, color);
}

// =========================================================================
//  Layout -- two fixed-height cards (title area always reserves 2 lines, so
//  the layout never jumps between a 1-line and 2-line title), vertically
//  centred as a block below the header row.
// =========================================================================
static int16_t s_cardX, s_cardW;
static int16_t s_nowCardY, s_nowCardH;
static int16_t s_nextCardY, s_nextCardH;
static int16_t s_statusY;

static int16_t cardContentH(bool withProgress) {
  int16_t h = LABEL_H + GAP_HEADER_TITLE + TITLE_H * 2 + GAP_TITLE_DUR + DUR_H;
  if (withProgress) h += GAP_DUR_PROGRESS + PROGRESS_H;
  return h;
}

static void layoutRecompute() {
  s_cardX = OUTER_MARGIN;
  s_cardW = viewW() - 2 * OUTER_MARGIN;

  s_nowCardH  = cardContentH(true) + 2 * CARD_PAD_Y;
  s_nextCardH = cardContentH(false) + 2 * CARD_PAD_Y;

  int16_t headerBottom = 44;
  int16_t totalH = s_nowCardH + GAP_BETWEEN_CARDS + s_nextCardH;
  int16_t top = headerBottom + (viewH() - headerBottom - 16 - totalH) / 2;
  if (top < headerBottom) top = headerBottom;

  s_nowCardY  = top;
  s_nextCardY = s_nowCardY + s_nowCardH + GAP_BETWEEN_CARDS;

  s_statusY = viewH() / 2 - 8;
}

// =========================================================================
//  Word wrap -- greedy, at most 2 lines, ellipsis if it still doesn't fit.
// =========================================================================
static void wrapTwoLines(const String &text, uint8_t size, int16_t maxWidth,
                         String *line1, String *line2) {
  *line1 = "";
  *line2 = "";
  if (text.length() == 0) return;
  if (vTextWidth(text.c_str(), size) <= maxWidth) { *line1 = text; return; }

  int splitAt = 0;
  for (int i = 1; i <= (int)text.length(); i++) {
    if (i == (int)text.length() || text[i] == ' ') {
      if (vTextWidth(text.substring(0, i).c_str(), size) > maxWidth) break;
      splitAt = i;
      if (i == (int)text.length()) break;
    }
  }
  if (splitAt == 0) {   // first word alone is already too wide
    *line1 = text.substring(0, 1);
    return;
  }

  *line1 = text.substring(0, splitAt);
  String rest = text.substring(splitAt);
  rest.trim();
  if (rest.length() == 0) return;

  if (vTextWidth(rest.c_str(), size) <= maxWidth) { *line2 = rest; return; }

  int cut = rest.length();
  while (cut > 0) {
    String candidate = rest.substring(0, cut) + "...";
    if (vTextWidth(candidate.c_str(), size) <= maxWidth) { *line2 = candidate; return; }
    cut--;
  }
  *line2 = "...";
}

static void formatDuration(uint32_t totalSec, char *out, size_t outLen) {
  if (totalSec < 60) {
    snprintf(out, outLen, "under 1m");
  } else if (totalSec < 3600) {
    snprintf(out, outLen, "%um", (unsigned)(totalSec / 60));
  } else {
    snprintf(out, outLen, "%uh %um", (unsigned)(totalSec / 3600),
             (unsigned)((totalSec % 3600) / 60));
  }
}

// =========================================================================
//  Battery pill -- top-right, a small rounded chip instead of bare text.
// =========================================================================
static const int16_t BATT_W = 62, BATT_H = 22;

static uint16_t batteryColor(int8_t pct) {
  if (pct > 80) return C_BATT_GREEN;
  if (pct >= 20) return C_BATT_BLUE;
  if (pct >= 10) return C_BATT_YELLOW;
  return C_BATT_RED;
}

static void drawBatteryBadge(int8_t pct, bool charging) {
  const int16_t right = viewW() - OUTER_MARGIN, y = 10;
  const int16_t x = right - BATT_W;
  vFillRect(x - 2, y - 2, BATT_W + 4, BATT_H + 4, C_BG);   // clear previous
  if (pct < 0) return;

  fillRoundRect(x, y, BATT_W, BATT_H, 8, C_BATT_BG);
  char text[8];
  snprintf(text, sizeof(text), "%d%%%s", pct, charging ? "+" : "");
  int16_t w = vTextWidth(text, 2);
  vDrawText(text, x + (BATT_W - w) / 2, y + (BATT_H - 16) / 2, 2, batteryColor(pct));
}

// =========================================================================
//  A "NOW" / "NEXT" card
// =========================================================================
static void drawProgressBar(int16_t x, int16_t y, int16_t w, float fraction, uint16_t color) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  fillRoundRect(x, y, w, PROGRESS_H, PROGRESS_R, C_TRACK);
  int16_t fillW = (int16_t)(w * fraction + 0.5f);
  if (fillW > 0) fillRoundRect(x, y, fillW, PROGRESS_H, PROGRESS_R, color);
}

// Redraws just the bits that change every second: duration text, the live
// pulse dot (NOW only), and the progress bar (NOW only).
static void redrawTick(bool isNow, uint16_t cardBg, uint16_t accent, int16_t cardY,
                       const char *durPrefix, const char *durSuffix, uint32_t durSec,
                       bool has, bool dotOn, float progressFraction) {
  int16_t textX = s_cardX + CARD_PAD_L;
  int16_t contentW = s_cardW - CARD_PAD_L - CARD_PAD_R;
  int16_t labelY = cardY + CARD_PAD_Y;
  int16_t title1Y = labelY + LABEL_H + GAP_HEADER_TITLE;
  int16_t title2Y = title1Y + TITLE_H;
  int16_t durY = title2Y + TITLE_H + GAP_TITLE_DUR;

  if (isNow && has) {
    // Live dot -- a slow 2s pulse between full and dim accent brightness.
    // Sits in the gap between the accent bar and the label text.
    int16_t dotX = s_cardX + DOT_X;
    int16_t dotY = labelY + LABEL_H / 2 - 1;
    vFillCircle(dotX, dotY, DOT_R, dotOn ? accent : cardBg);
    vDrawRectOutline(dotX - DOT_R, dotY - DOT_R, DOT_R * 2, DOT_R * 2, accent);
  }

  if (!has) return;

  vFillRect(textX, durY, contentW, DUR_H, cardBg);
  char durText[24];
  formatDuration(durSec, durText, sizeof(durText));
  char line[48];
  snprintf(line, sizeof(line), "%s%s%s", durPrefix, durText, durSuffix);
  vDrawText(line, textX, durY, DUR_SIZE, C_DUR);

  if (isNow) {
    int16_t progY = durY + DUR_H + GAP_DUR_PROGRESS;
    drawProgressBar(textX, progY, contentW, progressFraction, accent);
  }
}

static void drawCard(bool isNow, int16_t cardY, int16_t cardH, uint16_t cardBg, uint16_t accent,
                     const char *label, bool has, const String &title,
                     const char *durPrefix, const char *durSuffix, uint32_t durSec,
                     const char *emptyText, bool dotOn, float progressFraction) {
  fillRoundRect(s_cardX, cardY, s_cardW, cardH, CARD_R, cardBg);

  int16_t textX = s_cardX + CARD_PAD_L;
  int16_t contentW = s_cardW - CARD_PAD_L - CARD_PAD_R;
  int16_t labelY = cardY + CARD_PAD_Y;
  int16_t title1Y = labelY + LABEL_H + GAP_HEADER_TITLE;
  int16_t title2Y = title1Y + TITLE_H;

  // Accent bar down the left edge of the whole content block.
  int16_t barTop = labelY;
  int16_t barBottom = (has ? title2Y + TITLE_H : title1Y + LABEL_H);
  fillRoundRect(s_cardX + ACCENT_X, barTop, ACCENT_W, barBottom - barTop, ACCENT_R, accent);

  vDrawText(label, textX, labelY, LABEL_SIZE, accent);

  if (!has) {
    vDrawText(emptyText, textX, title1Y, LABEL_SIZE, C_EMPTY);
    redrawTick(isNow, cardBg, accent, cardY, durPrefix, durSuffix, durSec, false, dotOn, progressFraction);
    return;
  }

  String line1, line2;
  wrapTwoLines(title, TITLE_SIZE, contentW, &line1, &line2);
  vDrawText(line1.c_str(), textX, title1Y, TITLE_SIZE, C_TITLE);
  if (line2.length()) vDrawText(line2.c_str(), textX, title2Y, TITLE_SIZE, C_TITLE);

  redrawTick(isNow, cardBg, accent, cardY, durPrefix, durSuffix, durSec, true, dotOn, progressFraction);
}

// =========================================================================
//  Public API
// =========================================================================
static bool   s_valid = false;
static bool   s_prevHasStatus = false;
static String s_prevStatus;
static bool   s_prevHasCurrent = false, s_prevHasNext = false;
static String s_prevCurrentTitle, s_prevNextTitle;
static int8_t s_prevBattPct = -2;
static bool   s_prevBattCharging = false;

void calendarUiInit() {
  layoutRecompute();
  s_valid = false;
  vFillScreen(C_BG);
}

void calendarUiSetRotation(uint8_t rot) {
  rot &= 3;
  if (rot == viewRotation()) return;
  viewSetRotation(rot);
  layoutRecompute();
  s_valid = false;   // dimensions may have changed -- force a full redraw
}

void calendarUiFullRedraw(const CalendarView &v) {
  vFillScreen(C_BG);
  vDrawText("CALENDAR", OUTER_MARGIN, 16, LABEL_SIZE, C_HEADER);
  drawBatteryBadge(v.battPct, v.battCharging);

  if (v.statusLine) {
    int16_t w = vTextWidth(v.statusLine, 2);
    int16_t maxW = viewW() - 2 * OUTER_MARGIN;
    int16_t x = (w > maxW) ? OUTER_MARGIN : (viewW() - w) / 2;
    vDrawText(v.statusLine, x, s_statusY, 2, C_STATUS);
  } else {
    bool dotOn = ((millis() / 1000) % 2) == 0;
    float frac = (v.totalSec > 0) ? (float)v.elapsedSec / (float)v.totalSec : 0.0f;

    drawCard(true, s_nowCardY, s_nowCardH, C_CARD_NOW, C_ACCENT_NOW,
             "NOW", v.hasCurrent, v.currentTitle, "started ", " ago", v.elapsedSec,
             "No current event", dotOn, frac);

    drawCard(false, s_nextCardY, s_nextCardH, C_CARD_NEXT, C_ACCENT_NEXT,
             "NEXT", v.hasNext, v.nextTitle, "in ", "", v.remainingSec,
             "Nothing else today", false, 0.0f);
  }

  s_valid = true;
  s_prevHasStatus = v.statusLine != nullptr;
  s_prevStatus = v.statusLine ? v.statusLine : "";
  s_prevHasCurrent = v.hasCurrent;
  s_prevCurrentTitle = v.currentTitle;
  s_prevHasNext = v.hasNext;
  s_prevNextTitle = v.nextTitle;
  s_prevBattPct = v.battPct;
  s_prevBattCharging = v.battCharging;
}

void calendarUiRender(const CalendarView &v) {
  bool hasStatus = v.statusLine != nullptr;
  bool structureChanged =
      !s_valid ||
      hasStatus != s_prevHasStatus ||
      (hasStatus && s_prevStatus != v.statusLine) ||
      v.hasCurrent != s_prevHasCurrent ||
      v.currentTitle != s_prevCurrentTitle ||
      v.hasNext != s_prevHasNext ||
      v.nextTitle != s_prevNextTitle;

  if (structureChanged) {
    calendarUiFullRedraw(v);
    return;
  }

  if (!hasStatus) {
    bool dotOn = ((millis() / 1000) % 2) == 0;
    float frac = (v.totalSec > 0) ? (float)v.elapsedSec / (float)v.totalSec : 0.0f;
    redrawTick(true, C_CARD_NOW, C_ACCENT_NOW, s_nowCardY, "started ", " ago",
              v.elapsedSec, v.hasCurrent, dotOn, frac);
    redrawTick(false, C_CARD_NEXT, C_ACCENT_NEXT, s_nextCardY, "in ", "",
              v.remainingSec, v.hasNext, false, 0.0f);
  }

  if (v.battPct != s_prevBattPct || v.battCharging != s_prevBattCharging) {
    drawBatteryBadge(v.battPct, v.battCharging);
    s_prevBattPct = v.battPct;
    s_prevBattCharging = v.battCharging;
  }
}
