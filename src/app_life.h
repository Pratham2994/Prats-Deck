// app_life.h
// Conway's Game of Life on a 160x120 grid that wraps at the edges. Living cells glow,
// and dying ones leave fading trails, so you can see where the patterns have been.
// Draw on the screen to add cells.
#pragma once

#include "core.h"

namespace life {

// cells are stored as bits (1 = alive) to save RAM; rows are unpacked to bytes to work on
struct State {
  uint8_t bits[GH][GW / 8];
  uint8_t above[GW], cur[GW], below[GW], first[GW];
};
APP_STATE(State, S)

static inline void setCell(int x, int y) { S.bits[y][x >> 3] |= 1 << (x & 7); }

static void unpack(int y, uint8_t *out) {
  for (int x = 0; x < GW; x++) out[x] = (S.bits[y][x >> 3] >> (x & 7)) & 1;
}

static bool running = true;
static int speedIdx = 2, palIdx = 0;
static const int SPEEDS[] = {5, 10, 20, 40};
static const char *const SPEED_NAMES[] = {"5/s", "10/s", "20/s", "40/s"};
static const int PALS[] = {PAL_AURORA, PAL_NEON, PAL_ICE, PAL_FIRE};
static float acc = 0;
static uint32_t gen = 0;
static int population = 0;

static void randomize() {
  memset(S.bits, 0, sizeof(S.bits));
  for (int y = 0; y < GH; y++)
    for (int x = 0; x < GW; x++)
      if (random(0, 100) < 28) setCell(x, y);
  gen = 0;
}

// one generation, in place: a row is rewritten only after the rows that need it are unpacked
static void step() {
  unpack(GH - 1, S.above);
  unpack(0, S.first);
  unpack(0, S.cur);
  population = 0;
  for (int y = 0; y < GH; y++) {
    if (y == GH - 1) memcpy(S.below, S.first, GW);
    else unpack(y + 1, S.below);
    uint8_t *out = S.bits[y];
    memset(out, 0, GW / 8);
    for (int x = 0; x < GW; x++) {
      int l = x == 0 ? GW - 1 : x - 1, r = x == GW - 1 ? 0 : x + 1;
      int n = S.above[l] + S.above[x] + S.above[r] + S.cur[l] + S.cur[r] + S.below[l] + S.below[x] + S.below[r];
      if (n == 3 || (n == 2 && S.cur[x])) {
        out[x >> 3] |= 1 << (x & 7);
        population++;
      }
    }
    memcpy(S.above, S.cur, GW);
    memcpy(S.cur, S.below, GW);
  }
  gen++;
}

static void enter() {
  glowClear();
  randomize();
  acc = 0;
}

static void frame(uint16_t *fb, float dt, uint32_t /*now*/) {
  // draw cells
  if (T.down && !inToolbar(T.startY) && !inHomeCorner(T.startX, T.startY)) {
    int cx = (int)T.x / 2, cy = (int)T.y / 2;
    for (int k = 0; k < 12; k++) {
      int x = cx + random(-3, 4), y = cy + random(-3, 4);
      if ((unsigned)x < GW && (unsigned)y < GH) setCell(x, y);
    }
  }

  if (running) {
    acc += dt * SPEEDS[speedIdx];
    if (acc > 2) acc = 2;                      // at most 2 generations per frame
    while (acc >= 1) {
      acc -= 1;
      step();
    }
  }

  // living cells light up their grid cell; dead ones fade = trails
  for (int y = 0; y < GH; y++)
    for (int x = 0; x < GW; x++)
      if ((S.bits[y][x >> 3] >> (x & 7)) & 1) {
        uint16_t &h = heat[y * GW + x];
        if (h < 200 * 256) h = 200 * 256;
      }
  glowRender(fb, pal[PALS[palIdx]], dt, 0.25f, 20);

  appHeader(fb, "Life", rgb(140, 255, 120));
  char s[40];
  snprintf(s, sizeof(s), "Gen %lu   Alive %d", (unsigned long)gen, population);
  tiny(fb, s, W - 6 - tinyWidth(s), 8, LIGHTGREY);

  const char *labels[5] = {running ? "Pause" : "Play", SPEED_NAMES[speedIdx], "Random", "Clear", "Colour"};
  int hit = toolbar(fb, labels, 5, rgb(140, 255, 120));
  if (hit == 0) running = !running;
  if (hit == 1) speedIdx = (speedIdx + 1) % 4;
  if (hit == 2) randomize();
  if (hit == 3) {
    memset(S.bits, 0, sizeof(S.bits));
    gen = 0;
    population = 0;
  }
  if (hit == 4) palIdx = (palIdx + 1) % 4;
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  static const uint8_t GLIDER[3] = {0b010, 0b001, 0b111};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      fillRect(fb, cx - 15 + c * 11, cy - 15 + r * 11, 9, 9, GLIDER[r] & (4 >> c) ? col : rgb(30, 40, 30));
}

}  // namespace life
