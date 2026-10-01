// app_trackpad.h
// Trackpad: the screen works as a mouse pad for the PC. The deck is a USB mouse as well as a
// USB keyboard, so the PC needs no software.
//   Drag            : moves the pointer. A quick drag moves it further.
//   Tap             : left click
//   Strip at right  : drag up or down there to scroll
//   Bottom buttons  : Left click, Right click, and Hold (keeps the left button down, to drag
//                     a window or to select text; tap Hold again, or click, to let go)
#pragma once

#include <Mouse.h>
#include "core.h"

namespace trackpad {

static const uint16_t ACCENT = rgb(120, 200, 255);
static const int STRIP_W = 40, STRIP_X = W - STRIP_W;   // the scroll strip
static const int PAD_Y = 26;                            // the pad starts under the header

static float lastX, lastY, remX, remY, remW;   // last stylus position, and the parts of a step not sent yet
static bool tracking = false, holdOn = false;
static int clicks = 0;

static void enter() {
  tracking = false;
  remX = remY = remW = 0;
}

static void setHold(bool on) {
  holdOn = on;
  if (on) Mouse.press(MOUSE_LEFT);
  else Mouse.release(MOUSE_LEFT);
}

// A left click ends a hold: that is how you drop the thing you drag. A right click keeps it.
// (Mouse.click lets all the buttons go, so the hold is taken again after it.)
static void click(uint8_t button) {
  if (holdOn && button == MOUSE_LEFT) {
    setHold(false);
    return;
  }
  Mouse.click(button);
  if (holdOn) Mouse.press(MOUSE_LEFT);
  clicks++;
}

static void frame(uint16_t *fb, float dt, uint32_t /*now*/) {
  const bool inPad = !inHomeCorner(T.startX, T.startY) && T.startY >= PAD_Y && !inToolbar(T.startY);
  const bool inStrip = T.startX >= STRIP_X;
  if (T.down && inPad && T.moved >= 3) {         // under 3 px it is a tap that shakes, not a drag
    if (!tracking) {
      tracking = true;
      lastX = T.x;
      lastY = T.y;
    }
    float dx = T.x - lastX, dy = T.y - lastY;
    lastX = T.x;
    lastY = T.y;
    if (inStrip) {
      remW -= dy * 0.12f;                        // 8 px of drag is one step of the wheel
      int w = (int)remW;
      if (w) {
        Mouse.move(0, 0, w);
        remW -= w;
      }
    } else {
      float speed = sqrtf(dx * dx + dy * dy) / fmaxf(dt, 0.001f);   // px a second
      float gain = 1.6f + fminf(speed / 250, 3.0f);
      remX += dx * gain;
      remY += dy * gain;
      int mx = (int)remX, my = (int)remY;
      if (mx || my) {
        Mouse.move(constrain(mx, -127, 127), constrain(my, -127, 127), 0);
        remX -= mx;
        remY -= my;
      }
    }
    glowSplat(T.x, T.y, inStrip ? 4 : 6, 50 * 256);
  } else if (!T.down) {
    tracking = false;
  }
  if (T.tap && inPad && !inStrip) {
    click(MOUSE_LEFT);
    sparkBurst(T.startX, T.startY, 14, 150, 0.4f, 90 * 256);
  }

  sparksUpdate(dt);
  glowRender(fb, pal[PAL_ICE], dt);
  appHeader(fb, "Trackpad", ACCENT);
  char s[24];
  snprintf(s, sizeof(s), "%d clicks", clicks);
  tiny(fb, s, W - 6 - tinyWidth(s), 8, LIGHTGREY);

  // the scroll strip, with an arrow at each end
  shadeRect(fb, STRIP_X, PAD_Y, STRIP_W, BAR_Y - 6 - PAD_Y);
  vline(fb, STRIP_X, PAD_Y, BAR_Y - 6 - PAD_Y, rgb(40, 60, 84));
  const int ax = STRIP_X + STRIP_W / 2;
  fillTriangle(fb, ax - 7, PAD_Y + 20, ax + 7, PAD_Y + 20, ax, PAD_Y + 9, ACCENT);
  fillTriangle(fb, ax - 7, BAR_Y - 26, ax + 7, BAR_Y - 26, ax, BAR_Y - 15, ACCENT);
  if (!T.down) {
    textCenter(fb, SMALL, "Drag to move the pointer", STRIP_X / 2, 104, LIGHTGREY);
    textCenter(fb, SMALL, "Tap to click", STRIP_X / 2, 124, LIGHTGREY);
  }

  static const char *const LABELS[] = {"Left", "Right", "Hold"};
  int hit = toolbar(fb, LABELS, 3, ACCENT, holdOn ? 2 : -1);
  if (hit == 0) click(MOUSE_LEFT);
  if (hit == 1) click(MOUSE_RIGHT);
  if (hit == 2) setHold(!holdOn);
}

static void leave() {
  if (holdOn) setHold(false);                     // never leave a button down on the PC
}

// a pad with a pointer arrow on it
static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  roundRect(fb, cx - 16, cy - 12, 32, 24, 5, WHITE);
  hline(fb, cx - 15, cy + 5, 30, WHITE);
  vline(fb, cx, cy + 5, 6, WHITE);
  fillTriangle(fb, cx - 8, cy - 9, cx - 8, cy + 2, cx, cy - 2, col);
  strokeLine(fb, cx - 5, cy - 2, cx - 1, cy + 3, 2, col);
}

}  // namespace trackpad
