// core.h
// Shared by every app: the app table type, shared app memory, header bar, buttons.
#pragma once

#include "display.h"
#include "touch.h"
#include "glow.h"

struct App {
  const char *name;
  uint16_t color;
  void (*icon)(uint16_t *fb, int cx, int cy, uint16_t col);
  void (*enter)();
  void (*frame)(uint16_t *fb, float dt, uint32_t now);
  void (*leave)();
};

// Only one app runs at a time, so they share one block of memory for their big arrays.
alignas(8) static uint8_t appMem[10240];
#define APP_STATE(Type, name)                                                      \
  static_assert(sizeof(Type) <= sizeof(appMem), "app state is bigger than appMem"); \
  static Type &name = *reinterpret_cast<Type *>(appMem);

static int fps = 0;

// flash memory layout (calibration is at 0)
static const int EE_BRICKS_HI = 64;
static const int EE_LIGHT = 72;
// Chindi uses 256..767 (see app_chindi.h). EEPROM.begin(1024) in setup().

// ---------- Shared look: dark rounded cards, muted labels, one accent colour per app ----------
static const uint16_t CARD = rgb(13, 15, 24), CARD_EDGE = rgb(34, 38, 56), MUTED = rgb(120, 126, 150);

static void card(uint16_t *fb, int x, int y, int w, int h, uint16_t edge = CARD_EDGE, int r = 8) {
  fillRoundRect(fb, x, y, w, h, r, CARD);
  roundRect(fb, x, y, w, h, r, edge);
}

// Small rounded badge with a coloured dot, e.g. "LIVE". Right edge at rx. Returns its left edge.
static int pill(uint16_t *fb, int rx, int y, const char *label, uint16_t col, int dotLevel = 256) {
  int pw = tinyWidth(label) + 22, px = rx - pw;
  fillRoundRect(fb, px, y, pw, 16, 8, dim4(col));
  fillCircle(fb, px + 9, y + 8, 3, blend(dim(col), col, dotLevel));
  tiny(fb, label, px + 16, y + 4, col);
  return px;
}

// Top bar with the app name and a short accent line. The left 26 px hold the home button.
static void appHeader(uint16_t *fb, const char *title, uint16_t col) {
  shadeRect(fb, 0, 0, W, 23, true);
  hline(fb, 0, 23, W, rgb(26, 29, 42));
  int x = text(fb, SMALL, title, 30, 16, WHITE);
  fillRoundRect(fb, 30, 19, min(x - 30, 24), 2, 1, col);
}

// Button with a label. Lit = pressed or selected.
static void button(uint16_t *fb, int x, int y, int w, int h, const char *label, uint16_t col, bool lit = false) {
  fillRoundRect(fb, x, y, w, h, 8, lit ? dim(col) : CARD);
  roundRect(fb, x, y, w, h, 8, lit ? col : CARD_EDGE);
  uint16_t fg = lit ? WHITE : col;
  int tw = textWidth(SMALL, label);
  if (tw <= w - 8) text(fb, SMALL, label, x + (w - tw) / 2, y + h / 2 + 5, fg);
  else tinyCenter(fb, label, x + w / 2, y + h / 2 - 3, fg);   // long label: small font
}

// Row of n equal buttons along the bottom, y 208..238. Returns the index tapped, or -1.
static const int BAR_Y = 208, BAR_H = 30;
static int toolbar(uint16_t *fb, const char *const *labels, int n, uint16_t col, int lit = -1) {
  shadeRect(fb, 0, BAR_Y - 5, W, H - BAR_Y + 5, true);
  int bw = (W - 4) / n, hit = -1;
  for (int i = 0; i < n; i++) {
    int x = 2 + i * bw;
    bool pressed = T.down && inBox(T.startX, T.startY, x, BAR_Y, bw, BAR_H) && inBox(T.x, T.y, x, BAR_Y, bw, BAR_H);
    button(fb, x + 2, BAR_Y, bw - 4, BAR_H, labels[i], col, pressed || i == lit);
    if (tapIn(x, BAR_Y, bw, BAR_H)) hit = i;
  }
  return hit;
}

static bool inToolbar(float y) { return y >= BAR_Y - 4; }

// near the home button: apps ignore touches here
static bool inHomeCorner(float x, float y) { return x < 40 && y < 40; }
