#include "calendar_ui.h"
#include "gfx_view.h"
#include <string.h>

// =========================================================================
//  Palette
// =========================================================================
static const uint16_t C_BG      = C565(0, 0, 0);
static const uint16_t C_LABEL   = C565(110, 160, 255);   // "NOW" / "NEXT"
static const uint16_t C_TITLE   = C565(255, 255, 255);
static const uint16_t C_DUR     = C565(150, 150, 150);
static const uint16_t C_EMPTY   = C565(90, 90, 90);
static const uint16_t C_DIVIDER = C565(40, 40, 40);
static const uint16_t C_STATUS  = C565(255, 196, 70);

static const uint16_t C_BATT_GREEN  = C565(90, 210, 130);
static const uint16_t C_BATT_BLUE   = C565(100, 170, 255);
static const uint16_t C_BATT_YELLOW = C565(255, 220, 60);
static const uint16_t C_BATT_RED    = C565(255, 90, 90);

// ---- Text sizing ----------------------------------------------------------
// vDrawText clamps any requested size below 2 up to 2 (see gfx_view.cpp),
// so LABEL_SIZE and TITLE_SIZE render at genuinely different heights while
// DUR_SIZE ends up the same physical size as LABEL_SIZE -- distinguished by
// colour instead, same trick pomodoro_ui.cpp uses for its own hint text.
static const uint8_t LABEL_SIZE = 1;    // -> 16px tall
static const uint8_t TITLE_SIZE = 3;    // -> 24px tall
static const uint8_t DUR_SIZE   = 1;    // -> 16px tall

static const int16_t MARGIN     = 16;
static const int16_t LABEL_H    = 16;
static const int16_t TITLE_H    = 24;
static const int16_t DUR_H      = 16;

// ---- Layout (vertically centred in the space below the battery badge) -----
static int16_t s_contentW;
static int16_t s_nowLabelY, s_nowTitleY1, s_nowTitleY2, s_nowDurY;
static int16_t s_divY;
static int16_t s_nextLabelY, s_nextTitleY1, s_nextTitleY2, s_nextDurY;
static int16_t s_statusY;

static void layoutRecompute() {
  s_contentW = viewW() - 2 * MARGIN;

  // One block = label + 2 title lines + duration, with small gaps.
  const int16_t blockH = LABEL_H + 2 + TITLE_H * 2 + 4 + DUR_H;
  const int16_t gapBetween = 20;
  const int16_t dividerH = 20;
  const int16_t totalH = blockH * 2 + dividerH;

  int16_t top = 40 + (viewH() - 40 - 16 - totalH) / 2;
  if (top < 40) top = 40;

  s_nowLabelY  = top;
  s_nowTitleY1 = s_nowLabelY + LABEL_H + 2;
  s_nowTitleY2 = s_nowTitleY1 + TITLE_H;
  s_nowDurY    = s_nowTitleY2 + TITLE_H + 4;

  s_divY = s_nowDurY + DUR_H + gapBetween / 2;

  s_nextLabelY  = s_divY + dividerH;
  s_nextTitleY1 = s_nextLabelY + LABEL_H + 2;
  s_nextTitleY2 = s_nextTitleY1 + TITLE_H;
  s_nextDurY    = s_nextTitleY2 + TITLE_H + 4;

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
//  Battery badge -- same look as the launcher's.
// =========================================================================
static const int16_t BATT_W = 62;

static uint16_t batteryColor(int8_t pct) {
  if (pct > 80) return C_BATT_GREEN;
  if (pct >= 20) return C_BATT_BLUE;
  if (pct >= 10) return C_BATT_YELLOW;
  return C_BATT_RED;
}

static void drawBatteryBadge(int8_t pct, bool charging) {
  const int16_t right = viewW() - 12, y = 12;
  vFillRect(right - BATT_W, y, BATT_W, 16, C_BG);
  if (pct < 0) return;
  char text[8];
  snprintf(text, sizeof(text), "%d%%%s", pct, charging ? "+" : "");
  int16_t w = vTextWidth(text, 2);
  vDrawText(text, right - w, y, 2, batteryColor(pct));
}

// =========================================================================
//  One "NOW" / "NEXT" block
// =========================================================================
// Redraws just the duration line of a block -- what the 1 Hz tick actually
// changes most of the time, so this is the common-case cheap path.
static void redrawDuration(const char *durPrefix, const char *durSuffix,
                           uint32_t durSec, int16_t durY) {
  vFillRect(MARGIN, durY, s_contentW, DUR_H, C_BG);
  char durText[24];
  formatDuration(durSec, durText, sizeof(durText));
  char line[48];
  snprintf(line, sizeof(line), "%s%s%s", durPrefix, durText, durSuffix);
  vDrawText(line, MARGIN, durY, DUR_SIZE, C_DUR);
}

static void drawBlock(const char *label, bool has, const String &title,
                      const char *durPrefix, const char *durSuffix, uint32_t durSec,
                      const char *emptyText,
                      int16_t labelY, int16_t title1Y, int16_t title2Y, int16_t durY) {
  vFillRect(MARGIN, labelY, s_contentW, LABEL_H, C_BG);
  vDrawText(label, MARGIN, labelY, LABEL_SIZE, C_LABEL);

  vFillRect(MARGIN, title1Y, s_contentW, TITLE_H * 2, C_BG);

  if (!has) {
    vFillRect(MARGIN, durY, s_contentW, DUR_H, C_BG);
    vDrawText(emptyText, MARGIN, title1Y, LABEL_SIZE, C_EMPTY);
    return;
  }

  String line1, line2;
  wrapTwoLines(title, TITLE_SIZE, s_contentW, &line1, &line2);
  vDrawText(line1.c_str(), MARGIN, title1Y, TITLE_SIZE, C_TITLE);
  if (line2.length()) vDrawText(line2.c_str(), MARGIN, title2Y, TITLE_SIZE, C_TITLE);

  redrawDuration(durPrefix, durSuffix, durSec, durY);
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

void calendarUiFullRedraw(const CalendarView &v) {
  vFillScreen(C_BG);
  drawBatteryBadge(v.battPct, v.battCharging);

  if (v.statusLine) {
    int16_t w = vTextWidth(v.statusLine, 2);
    if (w > s_contentW) {
      vDrawText(v.statusLine, MARGIN, s_statusY, 2, C_STATUS);
    } else {
      vDrawText(v.statusLine, (viewW() - w) / 2, s_statusY, 2, C_STATUS);
    }
  } else {
    drawBlock("NOW", v.hasCurrent, v.currentTitle, "started ", " ago", v.elapsedSec,
              "No current event", s_nowLabelY, s_nowTitleY1, s_nowTitleY2, s_nowDurY);

    vFillRect(MARGIN, s_divY, s_contentW, 1, C_DIVIDER);

    drawBlock("NEXT", v.hasNext, v.nextTitle, "in ", "", v.remainingSec,
              "Nothing else today", s_nextLabelY, s_nextTitleY1, s_nextTitleY2, s_nextDurY);
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
    if (v.hasCurrent) redrawDuration("started ", " ago", v.elapsedSec, s_nowDurY);
    if (v.hasNext)    redrawDuration("in ", "", v.remainingSec, s_nextDurY);
  }

  if (v.battPct != s_prevBattPct || v.battCharging != s_prevBattCharging) {
    drawBatteryBadge(v.battPct, v.battCharging);
    s_prevBattPct = v.battPct;
    s_prevBattCharging = v.battCharging;
  }
}
