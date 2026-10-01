// app_galaxy.h
// 500 glowing particles with light trails. Touch reactive.
//   Hold and drag : particles swirl around your finger and get dragged along
//   Lift / flick  : they explode outward, or get thrown in the flick direction
//   Tap top-right : change colour theme
#pragma once

#include "core.h"

namespace galaxy {

static const int N = 500;
static const float XMAX = GW * 64 - 1;       // positions: 1 grid cell = 64 units
static const float YMAX = GH * 64 - 1;
// galaxy.py moved everything once per frame at 13 FPS. Speeds here are "per tick" of that,
// scaled by real frame time, so the look is the same at any FPS.
static const float TICK_HZ = 13.0f;
static const float G = 14400, SOFT = 256, BOOM = 230;
static const float VMAX = 384;
static const float STIR = 0.15f;             // how strongly a moving finger drags nearby particles
static const float STIR_R = 60 * 32;         // drag radius: 60 screen px
static const float FLING_R = 120 * 32;       // flick reaches this far at half strength
static const float FLICK = 400;              // finger speed (per tick) that counts as a flick

struct State {
  float px[N], py[N], vx[N], vy[N], kk[N];
};
APP_STATE(State, S)

static const int THEMES[] = {PAL_FIRE, PAL_ICE, PAL_NEON, PAL_AURORA};
static int theme = 0;
static uint32_t labelUntil = 0, freeUntil = 0;
static float tx, ty, vmax;
static bool corner = false;

static inline int trailSteps(float m) {
  int s = 1;
  while (s * 64 < m) s <<= 1;
  return s;
}

static void step(bool pull, float t, bool stir, float svx, float svy) {
  const float fric = powf(63.0f / 64.0f, t);
  const float stirR2 = STIR_R * STIR_R;
  for (int i = 0; i < N; i++) {
    float x = S.px[i], y = S.py[i], dxv = S.vx[i], dyv = S.vy[i];
    if (pull) {
      float dx = tx - x, dy = ty - y;
      float d2 = dx * dx + dy * dy;
      float d = sqrtf(d2);
      if (d < 128) d = 128;
      float g = G * S.kk[i] / (16 * d * (d + SOFT)) * t;
      float ax = dx * g, ay = dy * g;
      dxv += ax - ay * 0.25f;                // pull plus a quarter sideways = swirl
      dyv += ay + ax * 0.25f;
      if (stir && d2 < stirR2) {
        float w = (1 - d2 / stirR2) * STIR * t;
        dxv += svx * w;
        dyv += svy * w;
      }
    }
    dxv = constrain(dxv * fric, -vmax, vmax);
    dyv = constrain(dyv * fric, -vmax, vmax);
    float ox = x, oy = y;
    x += dxv * t;
    y += dyv * t;
    if (x < 0) { x = 0; dxv = -dxv * 0.5f; }
    if (x > XMAX) { x = XMAX; dxv = -dxv * 0.5f; }
    if (y < 0) { y = 0; dyv = -dyv * 0.5f; }
    if (y > YMAX) { y = YMAX; dyv = -dyv * 0.5f; }
    S.px[i] = x;
    S.py[i] = y;
    S.vx[i] = dxv;
    S.vy[i] = dyv;

    // light along the path = trail. Same light per second as galaxy.py.
    float sdx = x - ox, sdy = y - oy;
    int steps = trailSteps(fmaxf(fabsf(sdx), fabsf(sdy)));
    int perTick = trailSteps(fmaxf(fabsf(dxv), fabsf(dyv)));
    float bright = 34 + (fabsf(dxv) + fabsf(dyv)) / 32;
    int add = (int)(bright * perTick * t / steps * 256);
    float sx = sdx / steps, sy = sdy / steps, qx = ox, qy = oy;
    for (int k = 0; k < steps; k++) {
      qx += sx;
      qy += sy;
      glowAdd((int)qx >> 6, (int)qy >> 6, add);
    }
  }
}

// explosion out from (x, y), plus a throw in the flick direction
static void kick(float x, float y, float boom, float fvx, float fvy) {
  const float r2 = FLING_R * FLING_R;
  for (int i = 0; i < N; i++) {
    float dx = S.px[i] - x, dy = S.py[i] - y;
    float d2 = dx * dx + dy * dy;
    float d = sqrtf(d2);
    if (d < 128) d = 128;
    float b = boom * BOOM * S.kk[i] / (16 * d);
    float w = r2 / (r2 + d2) * S.kk[i] / 16;
    S.vx[i] = dx * b + S.vx[i] * 0.25f + fvx * w;
    S.vy[i] = dy * b + S.vy[i] * 0.25f + fvy * w;
  }
}

static void enter() {
  for (int i = 0; i < N; i++) {
    S.px[i] = random(0, GW * 64);
    S.py[i] = random(0, GH * 64);
    S.vx[i] = random(-160, 161);
    S.vy[i] = random(-160, 161);
    S.kk[i] = random(10, 23);
  }
  tx = GW * 64 / 2;
  ty = GH * 64 / 2;
  vmax = VMAX;
  labelUntil = millis() + 1500;
  freeUntil = 0;
  corner = false;
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  float t = fminf(dt * TICK_HZ, 1.5f);
  const float toUnits = 32.0f / TICK_HZ;     // px per second -> grid units per tick

  if (T.pressed) {
    corner = T.x > 270 && T.y < 40;
    if (corner) {
      theme = (theme + 1) % 4;
      labelUntil = now + 1500;
    }
  }
  bool finger = T.down && !corner && !inHomeCorner(T.startX, T.startY);
  if (T.released && !corner && !inHomeCorner(T.startX, T.startY)) {
    float hvx = T.relVx * toUnits, hvy = T.relVy * toUnits;
    float s = sqrtf(hvx * hvx + hvy * hvy);
    tx = T.relX * 32;
    ty = T.relY * 32;
    kick(tx, ty, 1 / (1 + s / 300), hvx, hvy);   // faster flick = more throw, less explosion
    if (s > FLICK) vmax = VMAX * 3;
    freeUntil = now + 600 + (uint32_t)fminf(s, 1000) / 2;
  }
  vmax = VMAX + (vmax - VMAX) * expf(-dt / 0.5f);

  bool pull;
  if (finger) {
    tx = T.x * 32;
    ty = T.y * 32;
    pull = true;
  } else if ((int32_t)(freeUntil - now) > 0) {
    pull = false;                              // let the explosion fly
  } else {
    tx = 5120 + 3600 * sinf(now / 1700.0f);    // idle: a slow invisible target wanders
    ty = 3840 + 2600 * sinf(now / 1100.0f);
    pull = !T.down;
  }

  step(pull, t, finger, T.vx * toUnits, T.vy * toUnits);
  glowRender(fb, pal[THEMES[theme]], dt);
  if ((int32_t)(labelUntil - now) > 0) text(fb, MEDIUM, PAL_NAMES[THEMES[theme]], 34, 26, WHITE);
  rect(fb, 296, 4, 20, 14, GREY);              // theme button
  char s[12];
  snprintf(s, sizeof(s), "%d FPS", fps);
  tiny(fb, s, 4, 230, rgb(150, 150, 150));
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int a = 0; a < 3; a++)
    for (int k = 0; k < 14; k++) {
      float r = 3 + k * 1.2f, ang = k * 0.38f + a * 2.094f;
      fillCircle(fb, cx + (int)(cosf(ang) * r), cy + (int)(sinf(ang) * r * 0.8f), k < 4 ? 2 : 1, blend(WHITE, col, k * 18));
    }
  fillCircle(fb, cx, cy, 3, WHITE);
}

}  // namespace galaxy
