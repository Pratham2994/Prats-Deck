// glow.h
// The shared light layer: a 160x120 grid of brightness (each cell = 2x2 screen pixels)
// drawn through a colour palette and fading every frame. Plus "sparks": short-lived
// particles that paint light trails into it.
#pragma once

#include "display.h"

static const int GW = 160, GH = 120;
static uint16_t heat[GW * GH];               // brightness: high byte 0..255, low byte = fraction

// ---------- Palettes ----------
enum { PAL_FIRE, PAL_ICE, PAL_NEON, PAL_AURORA, PAL_GOLD, PAL_PHOSPHOR, PAL_RAIN, PAL_SNOW, NPAL };
static const char *const PAL_NAMES[NPAL] = {"FIRE", "ICE", "NEON", "AURORA", "GOLD", "PHOSPHOR", "RAIN", "SNOW"};
static const uint8_t PAL_STOPS[NPAL][5][3] = {
  {{0, 0, 0}, {90, 0, 10}, {220, 40, 0}, {255, 160, 0}, {255, 255, 200}},
  {{0, 0, 0}, {0, 10, 70}, {0, 90, 220}, {60, 220, 255}, {240, 255, 255}},
  {{0, 0, 0}, {50, 0, 90}, {200, 0, 200}, {255, 80, 160}, {255, 230, 255}},
  {{0, 0, 0}, {0, 40, 50}, {0, 170, 110}, {140, 255, 90}, {255, 255, 220}},
  {{0, 0, 0}, {60, 30, 0}, {180, 110, 10}, {255, 200, 60}, {255, 250, 220}},
  {{0, 0, 0}, {0, 40, 10}, {0, 140, 40}, {80, 255, 120}, {230, 255, 230}},
  {{0, 0, 0}, {10, 20, 50}, {40, 80, 160}, {120, 170, 255}, {230, 240, 255}},
  {{0, 0, 0}, {30, 35, 50}, {110, 120, 150}, {200, 210, 235}, {255, 255, 255}},
};
static uint16_t pal[NPAL][256];

static void glowBegin() {
  for (int p = 0; p < NPAL; p++)
    for (int i = 0; i < 256; i++) {
      float f = i * 4 / 255.0f;
      int j = min((int)f, 3);
      float t = f - j;
      const uint8_t *a = PAL_STOPS[p][j], *b = PAL_STOPS[p][j + 1];
      pal[p][i] = rgb((int)(a[0] + (b[0] - a[0]) * t), (int)(a[1] + (b[1] - a[1]) * t),
                      (int)(a[2] + (b[2] - a[2]) * t));
    }
}

static void glowClear() { memset(heat, 0, sizeof(heat)); }

// add light to one cell. amount: 256 = one brightness step
static inline void glowAdd(int cx, int cy, int amount) {
  if ((unsigned)cx >= (unsigned)GW || (unsigned)cy >= (unsigned)GH) return;
  int i = cy * GW + cx;
  int h = heat[i] + amount;
  heat[i] = h > 65535 ? 65535 : (h < 0 ? 0 : h);
}

// soft round blob at screen position (x, y), radius r in screen px
static void glowSplat(float x, float y, float r, int amount) {
  float cx = x * 0.5f, cy = y * 0.5f, cr = fmaxf(r * 0.5f, 0.7f);
  int x0 = (int)(cx - cr), x1 = (int)(cx + cr), y0 = (int)(cy - cr), y1 = (int)(cy + cr);
  float inv = 1 / (cr * cr);
  for (int j = y0; j <= y1; j++)
    for (int i = x0; i <= x1; i++) {
      float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
      float f = 1 - (dx * dx + dy * dy) * inv;
      if (f > 0) glowAdd(i, j, (int)(amount * f));
    }
}

// line of light in screen px
static void glowLine(float x0, float y0, float x1, float y1, int amount) {
  float dx = x1 - x0, dy = y1 - y0;
  int steps = (int)(fmaxf(fabsf(dx), fabsf(dy)) * 0.5f) + 1;
  float sx = dx / steps, sy = dy / steps;
  for (int k = 0; k <= steps; k++) glowAdd((int)((x0 + sx * k) * 0.5f), (int)((y0 + sy * k) * 0.5f), amount);
}

// Draw the grid as 2x2 pixels through a palette, then fade it.
// keepPerSec: fraction of light kept per second. dropPerSec: brightness steps lost per second.
// The defaults match galaxy.py: 15/16 kept and 1 step lost per tick, 13 ticks per second.
static void glowRender(uint16_t *out, const uint16_t *p, float dt, float keepPerSec = 0.43f, float dropPerSec = 13) {
  const uint32_t mul = (uint32_t)(65536 * powf(keepPerSec, dt));
  uint32_t sub = (uint32_t)(256 * dropPerSec * dt + 0.5f);
  if (sub < 1 && dropPerSec > 0) sub = 1;
  int i = 0;
  for (int y = 0; y < GH; y++) {
    uint16_t *row = out + y * 2 * W;
    for (int x = 0; x < GW; x++, i++) {
      uint32_t h = heat[i];
      uint16_t c = p[h >> 8];
      uint16_t *o = row + x * 2;
      o[0] = c;
      o[1] = c;
      o[W] = c;
      o[W + 1] = c;
      h = (h * mul) >> 16;
      heat[i] = h > sub ? h - sub : 0;
    }
  }
}


// ---------- Sparks ----------
static const int NSPARK = 400;
static float spX[NSPARK], spY[NSPARK], spVx[NSPARK], spVy[NSPARK], spLife[NSPARK], spG[NSPARK];
static uint16_t spAmt[NSPARK];
static int spNext = 0;

static void sparksClear() {
  for (int i = 0; i < NSPARK; i++) spLife[i] = 0;
}

// one spark at (x, y) px, velocity px/s, life s, light amount, gravity px/s^2
static void spark(float x, float y, float vx, float vy, float life, int amount, float gravity = 0) {
  int i = spNext;
  spNext = (spNext + 1) % NSPARK;
  spX[i] = x;
  spY[i] = y;
  spVx[i] = vx;
  spVy[i] = vy;
  spLife[i] = life;
  spAmt[i] = amount;
  spG[i] = gravity;
}

// ring of sparks flying out of (x, y)
static void sparkBurst(float x, float y, int count, float speed, float life, int amount, float gravity = 0) {
  for (int k = 0; k < count; k++) {
    float a = random(0, 6283) / 1000.0f;
    float s = speed * (0.3f + random(0, 700) / 1000.0f);
    spark(x, y, cosf(a) * s, sinf(a) * s, life * (0.6f + random(0, 400) / 1000.0f), amount, gravity);
  }
}

static void sparksUpdate(float dt) {
  float drag = powf(0.35f, dt);
  for (int i = 0; i < NSPARK; i++) {
    if (spLife[i] <= 0) continue;
    spLife[i] -= dt;
    float ox = spX[i], oy = spY[i];
    spVy[i] += spG[i] * dt;
    spVx[i] *= drag;
    spVy[i] *= drag;
    spX[i] += spVx[i] * dt;
    spY[i] += spVy[i] * dt;
    if (spX[i] < -20 || spX[i] > W + 20 || spY[i] < -20 || spY[i] > H + 20) {
      spLife[i] = 0;
      continue;
    }
    int a = (int)(spAmt[i] * fminf(1, spLife[i] * 3));
    glowLine(ox, oy, spX[i], spY[i], a);
  }
}
