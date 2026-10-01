// touch.h
// XPT2046 touch on the shared SPI bus, calibration saved in flash, and a filtered
// finger tracker every app uses (T).
#pragma once

#include <EEPROM.h>
#include "display.h"

// ---------- Raw reads ----------
static uint16_t tpRead(uint8_t c) {
  uint8_t rx[2];
  spi_write_blocking(spi1, &c, 1);
  spi_read_blocking(spi1, 0, rx, 2);
  return ((rx[0] << 8) | rx[1]) >> 3;
}

static void sort7(uint16_t *v) {
  for (int i = 1; i < 7; i++)
    for (int j = i; j > 0 && v[j - 1] > v[j]; j--) {
      uint16_t t = v[j];
      v[j] = v[j - 1];
      v[j - 1] = t;
    }
}

static bool touchRaw(int &a, int &b) {
  if (gpio_get(PIN_TP_IRQ)) return false;
  lcdWait();                                   // the bus must be free
  spi_set_baudrate(spi1, TP_HZ);
  gpio_put(PIN_TP_CS, 0);
  uint16_t av[7], bv[7];
  for (int i = 0; i < 7; i++) {
    av[i] = tpRead(0xD0);
    bv[i] = tpRead(0x90);
  }
  gpio_put(PIN_TP_CS, 1);
  spi_set_baudrate(spi1, LCD_HZ);
  sort7(av);
  sort7(bv);
  a = av[3];
  b = bv[3];
  return 80 < a && a < 8150 && 80 < b && b < 8150;
}

static void waitPress(int &a, int &b) {
  while (true) {
    int x, y;
    while (!touchRaw(x, y)) delay(10);
    delay(60);
    long sa = 0, sb = 0, n = 0;
    while (touchRaw(x, y)) {
      sa += x;
      sb += y;
      n++;
      delay(10);
    }
    if (n) {
      a = sa / n;
      b = sb / n;
      return;
    }
  }
}


// ---------- Calibration (same flash layout as galaxy.ino, so it carries over) ----------
struct Cal {
  uint32_t magic;
  bool swap;
  float sx, ox, sy, oy;
};
static const uint32_t CAL_MAGIC = 0x43414C31;  // "CAL1"
static Cal cal;

static void cross(uint16_t *fb, int x, int y, uint16_t col) {
  hline(fb, x - 10, y, 21, col);
  vline(fb, x, y - 10, 21, col);
}

static void calibrate() {
  static const int tx[3] = {20, 300, 20}, ty[3] = {20, 20, 220};
  uint16_t *fb = frame[0];
  while (true) {
    int ra[3], rb[3];
    for (int i = 0; i < 3; i++) {
      fill(fb, BLACK);
      textCenter(fb, SMALL, "Press the red cross firmly", W / 2, 124, WHITE);
      cross(fb, tx[i], ty[i], RED);
      lcdShow(fb);
      waitPress(ra[i], rb[i]);
      delay(300);
    }
    bool swap = abs(rb[1] - rb[0]) > abs(ra[1] - ra[0]);
    if (swap)
      for (int i = 0; i < 3; i++) {
        int t = ra[i];
        ra[i] = rb[i];
        rb[i] = t;
      }
    if (abs(ra[1] - ra[0]) < 200 || abs(rb[2] - rb[0]) < 200) continue;
    cal.magic = CAL_MAGIC;
    cal.swap = swap;
    cal.sx = (300.0f - 20) / (ra[1] - ra[0]);
    cal.ox = 20 - ra[0] * cal.sx;
    cal.sy = (220.0f - 20) / (rb[2] - rb[0]);
    cal.oy = 20 - rb[0] * cal.sy;
    EEPROM.put(0, cal);
    EEPROM.commit();
    return;
  }
}

// Load calibration. Recalibrate if missing, or if a finger is held at start.
static void getCal() {
  EEPROM.get(0, cal);
  int a, b;
  if (cal.magic != CAL_MAGIC || touchRaw(a, b)) {
    if (touchRaw(a, b)) {
      fill(frame[0], BLACK);
      textCenter(frame[0], SMALL, "Release to recalibrate", W / 2, 124, WHITE);
      lcdShow(frame[0]);
      while (touchRaw(a, b)) delay(10);
    }
    calibrate();
  }
}

static bool touchXY(int &x, int &y) {
  int a, b;
  if (!touchRaw(a, b)) return false;
  if (cal.swap) {
    int t = a;
    a = b;
    b = t;
  }
  x = constrain((int)(a * cal.sx + cal.ox), 0, W - 1);
  y = constrain((int)(b * cal.sy + cal.oy), 0, H - 1);
  return true;
}


// ---------- Finger tracker ----------
struct TouchState {
  bool down;                 // finger on the screen (with lift bounce removed)
  bool pressed;              // went down this frame
  bool released;             // went up this frame
  bool tap;                  // released after a short press that hardly moved
  float x, y;                // smoothed position, screen px
  float vx, vy;              // smoothed velocity, px per second
  float startX, startY;      // where this press began
  float moved;               // furthest distance from the start, px
  uint32_t downMs;           // when this press began
  float relX, relY, relVx, relVy;   // position and velocity just before the lift (for flicks)
};
static TouchState T;

static const uint32_t LIFT_MS = 100;           // no touch this long = lifted. Hides stylus bounce
static const float JUMP_PX = 80;               // one read this far from the last is ignored as noise
static const float SMOOTH_S = 0.02f;           // position smoothing time
static const float VSMOOTH_S = 0.04f;          // velocity smoothing time
static const uint32_t LOOKBACK_MS = 30;        // the last reads before a lift drift: skip them

static const int HIST = 8;
static uint32_t histMs[HIST];
static float histX[HIST], histY[HIST], histVx[HIST], histVy[HIST];
static int histHead = 0, histCount = 0;
static int pressReads = 0;
static bool jumped = false;
static uint32_t lastTouchMs = 0;

static void trackPush(uint32_t ms) {
  histMs[histHead] = ms;
  histX[histHead] = T.x;
  histY[histHead] = T.y;
  histVx[histHead] = T.vx;
  histVy[histHead] = T.vy;
  histHead = (histHead + 1) % HIST;
  if (histCount < HIST) histCount++;
}

// index of the newest entry not newer than ms (or the oldest entry)
static int trackFind(uint32_t ms) {
  for (int i = 1; i <= histCount; i++) {
    int k = (histHead + HIST - i) % HIST;
    if ((int32_t)(ms - histMs[k]) >= 0) return k;
  }
  return (histHead + HIST - histCount) % HIST;
}

// Call once per frame with the frame time in seconds.
static void touchUpdate(uint32_t now, float dt) {
  T.pressed = T.released = T.tap = false;
  int x, y;
  if (touchXY(x, y)) {
    lastTouchMs = now;
    if (!T.down) {
      // the first read of a press is often off: start on the second
      if (++pressReads >= 2) {
        T.down = T.pressed = true;
        T.x = T.startX = x;
        T.y = T.startY = y;
        T.vx = T.vy = 0;
        T.moved = 0;
        T.downMs = now;
        histCount = 0;
        jumped = false;
      }
    } else {
      float dx = x - T.x, dy = y - T.y;
      if (!jumped && dx * dx + dy * dy > JUMP_PX * JUMP_PX) {
        jumped = true;                         // one wild read: skip it. Two in a row = real fast move
      } else {
        jumped = false;
        float k = 1 - expf(-dt / SMOOTH_S);
        float nx = T.x + dx * k, ny = T.y + dy * k;
        float kv = 1 - expf(-dt / VSMOOTH_S);
        T.vx += ((nx - T.x) / dt - T.vx) * kv;
        T.vy += ((ny - T.y) / dt - T.vy) * kv;
        T.x = nx;
        T.y = ny;
        float mx = T.x - T.startX, my = T.y - T.startY;
        T.moved = fmaxf(T.moved, sqrtf(mx * mx + my * my));
      }
    }
    if (T.down) trackPush(now);
  } else {
    pressReads = 0;
    if (T.down && now - lastTouchMs > LIFT_MS) {
      T.down = false;
      T.released = true;
      int h = trackFind(lastTouchMs - LOOKBACK_MS);
      T.relX = histX[h];
      T.relY = histY[h];
      T.relVx = histVx[h];
      T.relVy = histVy[h];
      T.tap = T.moved < 14 && lastTouchMs - T.downMs < 800;
    }
  }
}

static bool inBox(float x, float y, int bx, int by, int bw, int bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

// tap that started and ended in the box
static bool tapIn(int bx, int by, int bw, int bh) {
  return T.tap && inBox(T.startX, T.startY, bx, by, bw, bh);
}
