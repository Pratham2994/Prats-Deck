// app_paint.h
// Glow Paint: draw with light. Strokes that cross get brighter, up to white.
//   Colour : changes the whole picture's colour scheme at once
//   Size   : brush size
//   Mirror : off, 2-way, or 6-way kaleidoscope
//   Fade   : strokes fade away (good for doodling) or stay
//   Clear  : wipe the canvas
#pragma once

#include "core.h"

namespace paint {

static const int PALS[] = {PAL_NEON, PAL_FIRE, PAL_ICE, PAL_AURORA, PAL_GOLD, PAL_PHOSPHOR};
static int palIdx = 0, sizeIdx = 1, mirror = 2;
static bool fade = false;
static float lastX, lastY;
static bool drawing = false;
static const float SIZES[] = {4, 8, 14};
static const char *const SIZE_NAMES[] = {"Size S", "Size M", "Size L"};
static const int MIRRORS[] = {1, 2, 6};
static const char *const MIRROR_NAMES[] = {"Mirror", "Mirror 2", "Mirror 6"};

// one brush dab, copied for the mirror mode
static void dab(float x, float y, float r, int amount) {
  int m = MIRRORS[mirror];
  if (m == 1) {
    glowSplat(x, y, r, amount);
    return;
  }
  const float cx = W / 2, cy = (BAR_Y - 4) / 2;
  if (m == 2) {
    glowSplat(x, y, r, amount);
    glowSplat(2 * cx - x, y, r, amount);
    return;
  }
  // 6-way kaleidoscope: 6 rotations, each also mirrored
  float dx = x - cx, dy = y - cy;
  for (int k = 0; k < 6; k++) {
    float a = k * 1.0471976f, c = cosf(a), s = sinf(a);
    glowSplat(cx + dx * c - dy * s, cy + dx * s + dy * c, r, amount);
    glowSplat(cx - dx * c + dy * s, cy + dx * s + dy * c, r, amount);
  }
}

static void enter() {
  glowClear();
  drawing = false;
}

static void frame(uint16_t *fb, float dt, uint32_t /*now*/) {
  bool canvasTouch = T.down && !inToolbar(T.startY) && !inHomeCorner(T.startX, T.startY);
  if (canvasTouch) {
    float r = SIZES[sizeIdx];
    int amount = (int)(40 * 256 * (fade ? 1.6f : 1));
    if (!drawing) {
      lastX = T.x;
      lastY = T.y;
      drawing = true;
      dab(T.x, T.y, r, amount);
    }
    // dabs along the stroke, half a brush apart
    float dx = T.x - lastX, dy = T.y - lastY;
    float dist = sqrtf(dx * dx + dy * dy);
    int n = (int)(dist / (r * 0.5f));
    for (int k = 1; k <= n; k++) dab(lastX + dx * k / n, lastY + dy * k / n, r, amount);
    if (n) {
      lastX = T.x;
      lastY = T.y;
    }
    // fast strokes throw sparks
    float sp = sqrtf(T.vx * T.vx + T.vy * T.vy);
    if (sp > 400 && random(0, 100) < 60)
      spark(T.x, T.y, T.vx * 0.3f + random(-60, 60), T.vy * 0.3f + random(-60, 60), 0.5f, 60 * 256);
  } else {
    drawing = false;
  }

  sparksUpdate(dt);
  if (fade) glowRender(fb, pal[PALS[palIdx]], dt, 0.6f, 6);
  else glowRender(fb, pal[PALS[palIdx]], dt, 1.0f, 0);

  const char *labels[5] = {PAL_NAMES[PALS[palIdx]], SIZE_NAMES[sizeIdx], MIRROR_NAMES[mirror], fade ? "Fade on" : "Fade", "Clear"};
  int hit = toolbar(fb, labels, 5, rgb(255, 120, 220), fade ? 3 : -1);
  if (hit == 0) palIdx = (palIdx + 1) % 6;
  if (hit == 1) sizeIdx = (sizeIdx + 1) % 3;
  if (hit == 2) mirror = (mirror + 1) % 3;
  if (hit == 3) fade = !fade;
  if (hit == 4) {
    sparksClear();
    glowClear();
  }
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int k = 0; k < 6; k++) {
    float a = k * 1.047f;
    thickLine(fb, cx, cy, cx + (int)(cosf(a) * 16), cy + (int)(sinf(a) * 16), k % 2 ? col : WHITE);
  }
  fillCircle(fb, cx, cy, 4, WHITE);
}

}  // namespace paint
