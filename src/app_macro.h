// app_macro.h
// Macro pad: the Pico acts as a USB keyboard. Each button sends a media key or a shortcut.
// No PC software needed. Shortcuts are for Windows.
#pragma once

#include <Keyboard.h>
#include "core.h"

namespace macro {

// USB consumer (media) key codes
static const uint16_t MEDIA_NEXT = 0xB5, MEDIA_PREV = 0xB6, MEDIA_PLAY = 0xCD;
static const uint16_t MEDIA_MUTE = 0xE2, MEDIA_VOL_UP = 0xE9, MEDIA_VOL_DOWN = 0xEA;

enum Icon { IC_TEXT, IC_PREV, IC_PLAY, IC_NEXT, IC_MUTE, IC_VOLDN, IC_VOLUP };

struct Key {
  const char *label;
  const char *hint;
  Icon icon;
  uint16_t media;            // media key, or 0
  uint8_t keys[3];           // shortcut keys pressed together (0 = none)
  bool repeat;               // repeats while held
};

static const Key KEYS[12] = {
  {"Prev", "track", IC_PREV, MEDIA_PREV, {0}, false},
  {"Play", "pause", IC_PLAY, MEDIA_PLAY, {0}, false},
  {"Next", "track", IC_NEXT, MEDIA_NEXT, {0}, false},
  {"Mute", "sound", IC_MUTE, MEDIA_MUTE, {0}, false},
  {"Vol -", "hold", IC_VOLDN, MEDIA_VOL_DOWN, {0}, true},
  {"Vol +", "hold", IC_VOLUP, MEDIA_VOL_UP, {0}, true},
  {"Snip", "Win+Shift+S", IC_TEXT, 0, {KEY_LEFT_GUI, KEY_LEFT_SHIFT, 's'}, false},
  {"Lock", "Win+L", IC_TEXT, 0, {KEY_LEFT_GUI, 'l', 0}, false},
  {"Copy", "Ctrl+C", IC_TEXT, 0, {KEY_LEFT_CTRL, 'c', 0}, false},
  {"Paste", "Ctrl+V", IC_TEXT, 0, {KEY_LEFT_CTRL, 'v', 0}, false},
  {"Undo", "Ctrl+Z", IC_TEXT, 0, {KEY_LEFT_CTRL, 'z', 0}, false},
  {"Tasks", "Task Manager", IC_TEXT, 0, {KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_ESC}, false},
};

static const int COLS = 4, ROWS = 3;
static const int KX = 4, KY = 28, KW = 78, KH = 68;   // grid origin and cell size
static uint32_t flashUntil[12];
static int held = -1;
static uint32_t nextRepeat = 0;
static int sent = 0;

static void send(const Key &k) {
  if (k.media) {
    Keyboard.consumerPress(k.media);
    delay(8);
    Keyboard.consumerRelease();
  } else {
    for (int i = 0; i < 3; i++)
      if (k.keys[i]) Keyboard.press(k.keys[i]);
    delay(8);
    Keyboard.releaseAll();
  }
  sent++;
}

static void speaker(uint16_t *fb, int cx, int cy, uint16_t col) {
  fillRect(fb, cx - 12, cy - 4, 6, 8, col);
  fillTriangle(fb, cx - 7, cy, cx + 1, cy - 9, cx + 1, cy + 9, col);
}

static void drawIcon(uint16_t *fb, Icon ic, int cx, int cy, uint16_t col) {
  switch (ic) {
    case IC_PREV:
      fillRect(fb, cx - 12, cy - 8, 3, 17, col);
      fillTriangle(fb, cx - 9, cy, cx + 1, cy - 8, cx + 1, cy + 8, col);
      fillTriangle(fb, cx + 1, cy, cx + 11, cy - 8, cx + 11, cy + 8, col);
      break;
    case IC_NEXT:
      fillTriangle(fb, cx - 11, cy - 8, cx - 11, cy + 8, cx - 1, cy, col);
      fillTriangle(fb, cx - 1, cy - 8, cx - 1, cy + 8, cx + 9, cy, col);
      fillRect(fb, cx + 9, cy - 8, 3, 17, col);
      break;
    case IC_PLAY:
      fillTriangle(fb, cx - 12, cy - 9, cx - 12, cy + 9, cx + 1, cy, col);
      fillRect(fb, cx + 4, cy - 8, 3, 17, col);
      fillRect(fb, cx + 10, cy - 8, 3, 17, col);
      break;
    case IC_MUTE:
      speaker(fb, cx - 2, cy, col);
      thickLine(fb, cx + 4, cy - 5, cx + 12, cy + 5, col);
      thickLine(fb, cx + 4, cy + 5, cx + 12, cy - 5, col);
      break;
    case IC_VOLDN:
      speaker(fb, cx - 2, cy, col);
      fillRect(fb, cx + 4, cy - 1, 9, 3, col);
      break;
    case IC_VOLUP:
      speaker(fb, cx - 2, cy, col);
      fillRect(fb, cx + 4, cy - 1, 9, 3, col);
      fillRect(fb, cx + 7, cy - 4, 3, 9, col);
      break;
    default:
      break;
  }
}

static void enter() {
  for (auto &f : flashUntil) f = 0;
  held = -1;
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  // which key is under the finger
  int under = -1;
  if (T.down && !inHomeCorner(T.startX, T.startY)) {
    int c = ((int)T.x - KX) / KW, r = ((int)T.y - KY) / KH;
    if (T.x >= KX && T.y >= KY && c < COLS && r < ROWS) under = r * COLS + c;
  }
  if (T.pressed && under >= 0) {               // fire on press: feels instant
    held = under;
    send(KEYS[under]);
    flashUntil[under] = now + 180;
    nextRepeat = now + 400;
    int cx = KX + (under % COLS) * KW + KW / 2, cy = KY + (under / COLS) * KH + KH / 2;
    sparkBurst(cx, cy, 40, 260, 0.6f, 60 * 256);
  }
  if (!T.down) held = -1;
  if (held >= 0 && KEYS[held].repeat && under == held && (int32_t)(now - nextRepeat) >= 0) {
    send(KEYS[held]);
    flashUntil[held] = now + 80;
    nextRepeat = now + 110;
  }

  // background: slow drifting glow
  if (random(0, 100) < 30)
    spark(random(0, W), H + 5, random(-20, 20), -random(20, 60), 4, 18 * 256);
  sparksUpdate(dt);
  glowRender(fb, pal[PAL_NEON], dt);

  appHeader(fb, "Macro Pad", rgb(255, 120, 220));
  char s[24];
  snprintf(s, sizeof(s), "%d sent", sent);
  tiny(fb, s, W - 6 - tinyWidth(s), 8, LIGHTGREY);

  for (int i = 0; i < 12; i++) {
    const Key &k = KEYS[i];
    int x = KX + (i % COLS) * KW + 2, y = KY + (i / COLS) * KH + 2, w = KW - 4, h = KH - 4;
    uint16_t col = i < 6 ? rgb(255, 120, 220) : rgb(120, 200, 255);
    bool lit = (int32_t)(flashUntil[i] - now) > 0 || (held == i && under == i);
    fillRoundRect(fb, x, y, w, h, 10, lit ? dim(col) : blend(CARD, col, 22));
    roundRect(fb, x, y, w, h, 10, lit ? col : blend(CARD_EDGE, col, 70));
    uint16_t fg = lit ? WHITE : col;
    if (k.icon != IC_TEXT) {
      drawIcon(fb, k.icon, x + w / 2, y + 22, fg);
      textCenter(fb, SMALL, k.label, x + w / 2, y + 50, fg);
    } else {
      textCenter(fb, MEDIUM, k.label, x + w / 2, y + 32, fg);
      tinyCenter(fb, k.hint, x + w / 2, y + 44, lit ? WHITE : LIGHTGREY);
    }
  }
}

static void leave() { Keyboard.releaseAll(); }

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) fillRoundRect(fb, cx - 15 + c * 11, cy - 15 + r * 11, 9, 9, 2, (r + c) % 2 ? col : WHITE);
}

}  // namespace macro
