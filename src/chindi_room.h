// chindi_room.h
// Chindi's rooms and props. The bedroom follows her photo: cream walls, a door with a
// lever handle and a dark half-round mat, a 3-way light switch above a dark wooden
// headboard, a grey paisley bed sheet and beige floor tiles. Windows show the real sky:
// time of day, sun, moon, stars, clouds, rain, snow and lightning.
#pragma once

#include "chindi_gfx.h"

namespace room {

enum { BEDROOM, STUDY, BALCONY, NROOM };
static const char *const NAMES[NROOM] = {"Bedroom", "Study", "Balcony"};

// layout shared with the behaviour code
static const int FLOOR_Y = 152;                  // wall meets floor
static const int CAT_Y = 192;                    // Chindi's feet
static const int WIN_X = 92, WIN_Y = 18, WIN_W = 84, WIN_H = 76;
static const int TABLE_X0 = 148, TABLE_X1 = 192, TABLE_TOP = 150;   // little table with the cup
static const int CUSHION_X = 262;
static const int LAPTOP_X = 228;

// what the sky looks like now
enum Wx { W_CLEAR, W_CLOUDY, W_RAIN, W_SNOW, W_STORM, W_FOG };
struct Env {
  float hour = 14;           // 0..24
  bool timeKnown = false;
  int wx = W_CLEAR;
  bool sunny() const { return (wx == W_CLEAR || wx == W_CLOUDY) && hour > 7.5f && hour < 17.5f; }
  bool night() const { return hour < 6 || hour >= 19.5f; }
  float light() const {      // room brightness 0..1 from the time of day
    if (hour >= 7 && hour < 18) return 1;
    if (hour >= 5 && hour < 7) return 0.55f + 0.45f * (hour - 5) / 2;
    if (hour >= 18 && hour < 20) return 1 - 0.45f * (hour - 18) / 2;
    return 0.55f;
  }
  float sunX() const { return 300 - (hour - 8) / 9 * 250; }    // sunbeam on the floor
};

static uint16_t mix(uint16_t a, uint16_t b, float k) { return blend(a, b, (int)(constrain(k, 0, 1) * 256)); }

static void vgrad(uint16_t *fb, int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
  for (int j = 0; j < h; j++) fillRect(fb, x, y + j, w, 1, mix(top, bottom, (float)j / max(h - 1, 1)));
}

// ---------- Sky (in a window, or the balcony) ----------
static void skyColours(const Env &e, uint16_t &top, uint16_t &bottom) {
  struct K { float h; uint16_t t, b; };
  static const K keys[] = {
    {0, rgb(8, 12, 34), rgb(26, 32, 70)},   {5, rgb(14, 20, 50), rgb(40, 44, 90)},
    {6.5f, rgb(70, 90, 160), rgb(250, 170, 120)}, {8, rgb(90, 160, 230), rgb(190, 220, 245)},
    {16.5f, rgb(90, 160, 230), rgb(190, 220, 245)}, {18.5f, rgb(70, 64, 140), rgb(250, 140, 90)},
    {20, rgb(14, 20, 50), rgb(40, 44, 90)}, {24, rgb(8, 12, 34), rgb(26, 32, 70)},
  };
  int i = 0;
  while (i < 6 && e.hour >= keys[i + 1].h) i++;
  float k = (e.hour - keys[i].h) / (keys[i + 1].h - keys[i].h);
  top = mix(keys[i].t, keys[i + 1].t, k);
  bottom = mix(keys[i].b, keys[i + 1].b, k);
  if (e.wx != W_CLEAR && e.wx != W_CLOUDY) {    // overcast
    uint16_t g = e.night() ? rgb(30, 34, 44) : rgb(140, 150, 162);
    top = mix(top, g, 0.7f);
    bottom = mix(bottom, g, 0.6f);
  } else if (e.wx == W_CLOUDY) {
    top = mix(top, rgb(160, 170, 180), 0.25f);
  }
}

static void sky(uint16_t *fb, int x, int y, int w, int h, const Env &e, float t) {
  uint16_t top, bottom;
  skyColours(e, top, bottom);
  vgrad(fb, x, y, w, h, top, bottom);
  auto clipIn = [&](int px, int py) { return px >= x && px < x + w && py >= y && py < y + h; };
  if (e.night()) {
    for (int k = 0; k < 18; k++) {             // stars
      int sx = x + (k * 53 + 17) % w, sy = y + (k * 37 + 11) % (h * 2 / 3);
      if ((int)(t * 2 + k) % 7 != 0 && e.wx == W_CLEAR) pixel(fb, sx, sy, k % 3 ? rgb(230, 230, 255) : rgb(255, 240, 200));
    }
    int mx = x + w * 3 / 4, my = y + h / 4;   // moon
    if (e.wx != W_RAIN && e.wx != W_STORM) {
      fillCircle(fb, mx, my, min(w, h) / 9 + 2, rgb(245, 240, 210));
      fillCircle(fb, mx + 3, my - 2, min(w, h) / 9, top);
    }
  } else if (e.wx == W_CLEAR || e.wx == W_CLOUDY) {
    float u = constrain((e.hour - 6) / 13, 0, 1);
    int sx = x + (int)(w * (0.1f + 0.8f * u)), sy = y + (int)(h * (0.75f - 0.55f * sinf(3.1416f * u)));
    int r = min(w, h) / 8 + 2;
    for (int k = 3; k >= 1; k--) {             // soft glow
      int rr = r + k * 4;
      for (int j = -rr; j <= rr; j++)
        for (int i = -rr; i <= rr; i++)
          if (i * i + j * j <= rr * rr && clipIn(sx + i, sy + j)) {
            uint16_t &p = fb[(sy + j) * W + sx + i];
            p = blend(p, rgb(255, 240, 180), 40);
          }
    }
    fillCircle(fb, sx, sy, r, rgb(255, 236, 160));
  }
  // clouds drifting
  int nc = e.wx == W_CLEAR ? 1 : 4;
  uint16_t cc = e.wx == W_CLEAR || e.wx == W_CLOUDY ? (e.night() ? rgb(70, 76, 100) : rgb(250, 250, 252)) : (e.night() ? rgb(50, 54, 66) : rgb(200, 205, 212));
  for (int k = 0; k < nc; k++) {
    float cx = x + fmodf(k * 61 + t * (4 + k * 1.5f), w + 60) - 30, cy = y + 10 + (k * 23) % max(h / 2, 1);
    for (int b = 0; b < 3; b++) {
      int bx = (int)cx + (b - 1) * 11, by = (int)cy - (b == 1 ? 5 : 0), br = b == 1 ? 11 : 8;
      for (int j = -br; j <= br; j++)
        for (int i = -br; i <= br; i++)
          if (i * i + j * j <= br * br && clipIn(bx + i, by + j)) fb[(by + j) * W + bx + i] = cc;
    }
  }
  if (e.wx == W_RAIN || e.wx == W_STORM) {
    for (int k = 0; k < w * h / 160; k++) {
      int rx = x + (int)(fmodf(k * 37.3f + t * 40, (float)w)), ry = y + (int)(fmodf(k * 23.7f + t * 220, (float)h));
      for (int d = 0; d < 6; d++)
        if (clipIn(rx - d / 3, ry + d)) {
          uint16_t &p = fb[(ry + d) * W + rx - d / 3];
          p = blend(p, rgb(200, 215, 240), 150);
        }
    }
  }
  if (e.wx == W_SNOW) {
    for (int k = 0; k < w * h / 120; k++) {
      int sx = x + (int)fmodf(k * 41.1f + sinf(t + k) * 4 + 100, (float)w), sy = y + (int)fmodf(k * 17.3f + t * 18, (float)h);
      if (clipIn(sx, sy)) fillRect(fb, sx, sy, 2, 2, WHITE);
    }
  }
  if (e.wx == W_FOG) {
    for (int j = 0; j < h; j += 2) {
      int band = (int)(sinf(j * 0.15f + t * 0.5f) * 40 + 60);
      for (int i = 0; i < w; i++) {
        uint16_t &p = fb[(y + j) * W + x + i];
        p = blend(p, rgb(210, 214, 220), band);
      }
    }
  }
  if (e.wx == W_STORM && fmodf(t, 6.0f) < 0.12f) fillRect(fb, x, y, w, h, rgb(240, 240, 255));   // lightning
}

static void window(uint16_t *fb, const Env &e, float t) {
  const int x = WIN_X, y = WIN_Y, w = WIN_W, h = WIN_H;
  sky(fb, x, y, w, h, e, t);
  uint16_t fr = rgb(250, 248, 242), frs = rgb(206, 196, 180);
  fillRect(fb, x - 5, y - 5, w + 10, 5, fr);
  fillRect(fb, x - 5, y + h, w + 10, 5, fr);
  fillRect(fb, x - 5, y, 5, h, fr);
  fillRect(fb, x + w, y, 5, h, fr);
  fillRect(fb, x + w / 2 - 2, y, 4, h, fr);
  fillRect(fb, x, y + h / 2 - 2, w, 4, fr);
  fillRect(fb, x - 9, y + h + 5, w + 18, 4, frs);           // sill
  // curtains
  for (int s = 0; s < 2; s++) {
    int cx = s ? x + w + 2 : x - 16;
    for (int j = -10; j < h + 18; j++) {
      int cw = 14 - (j > h / 2 ? (j - h / 2) / 6 : 0);
      uint16_t c = ((j / 2) % 4 == 0) ? rgb(226, 200, 160) : rgb(240, 218, 182);
      fillRect(fb, s ? cx : cx + 14 - cw, y + j, cw, 1, c);
    }
  }
  fillRect(fb, x - 22, y - 12, w + 44, 4, rgb(150, 120, 90));  // curtain rod
}

// ---------- Floor ----------
static void floorTiles(uint16_t *fb, uint16_t a, uint16_t b, uint16_t grout) {
  vgrad(fb, 0, FLOOR_Y, W, H - FLOOR_Y, a, b);
  static const int rows[] = {162, 175, 191, 211, 236};
  for (int r : rows) hline(fb, 0, r, W, grout);
  for (int k = -7; k <= 7; k++) line(fb, 160 + k * 34, FLOOR_Y, 160 + k * 82, H, grout);
}

// ---------- Rooms ----------
static void bedroom(uint16_t *fb, const Env &e, float t) {
  vgrad(fb, 0, 0, W, FLOOR_Y, rgb(244, 233, 211), rgb(230, 214, 188));
  window(fb, e, t);
  // door, lever handle and lock, like the photo
  fillRect(fb, 6, 26, 66, FLOOR_Y - 26, rgb(206, 192, 168));
  fillRect(fb, 10, 30, 58, FLOOR_Y - 30, rgb(238, 228, 208));
  vline(fb, 10, 30, FLOOR_Y - 30, rgb(220, 208, 186));
  fillRect(fb, 54, 86, 14, 3, rgb(150, 150, 150));
  fillCircle(fb, 56, 87, 3, rgb(170, 170, 170));
  fillCircle(fb, 57, 97, 2, rgb(150, 150, 150));
  // light switch plate above the bed
  fillRoundRect(fb, 228, 48, 38, 20, 3, rgb(250, 250, 248));
  roundRect(fb, 228, 48, 38, 20, 3, rgb(210, 205, 196));
  for (int k = 0; k < 3; k++) fillRect(fb, 233 + k * 10, 53, 7, 10, k == 2 ? rgb(236, 236, 232) : rgb(244, 244, 240));
  // headboard
  fillRect(fb, 196, 72, W - 196, FLOOR_Y - 72, rgb(84, 56, 40));
  fillRect(fb, 196, 72, W - 196, 3, rgb(120, 84, 60));
  floorTiles(fb, rgb(232, 224, 210), rgb(218, 208, 192), rgb(206, 194, 176));
  // doormat: dark half circle at the door
  for (int j = 0; j < 12; j++) {
    int hw = (int)(36 * sqrtf(1 - (j / 12.0f) * (j / 12.0f)));
    hline(fb, 39 - hw, FLOOR_Y + j, 2 * hw, rgb(92, 94, 96));
  }
  // bed: grey sheet with a paisley pattern, pillow
  uint16_t sheet = rgb(196, 198, 210), motif = rgb(150, 152, 168);
  fillRect(fb, 184, 118, W - 184, 52, sheet);
  fillRect(fb, 184, 168, W - 184, 12, rgb(170, 172, 186));
  fillRoundRect(fb, 210, 100, 80, 28, 8, rgb(214, 216, 226));
  roundRect(fb, 210, 100, 80, 28, 8, rgb(170, 172, 186));
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 6; i++) {
      int px = 196 + i * 24 + (j % 2) * 12, py = 128 + j * 15;
      circle(fb, px, py, 5, motif);
      fillCircle(fb, px, py, 1, motif);
      line(fb, px + 4, py - 3, px + 9, py - 8, motif);
    }
  for (int i = 0; i < 3; i++) {
    int px = 224 + i * 24, py = 113;
    circle(fb, px, py, 4, motif);
  }
}

static void study(uint16_t *fb, const Env &e, float t) {
  vgrad(fb, 0, 0, W, FLOOR_Y, rgb(218, 230, 222), rgb(200, 214, 205));
  window(fb, e, t);
  // bookshelf
  fillRect(fb, 8, 30, 70, FLOOR_Y - 30, rgb(140, 100, 70));
  for (int s = 0; s < 4; s++) {
    int sy = 36 + s * 29;
    fillRect(fb, 12, sy, 62, 25, rgb(96, 66, 46));
    for (int b = 0, bx = 14; bx < 70; b++) {
      int bw = 5 + (b * 7 + s * 3) % 5, bh = 16 + (b * 5 + s) % 8;
      fillRect(fb, bx, sy + 25 - bh, bw, bh, hsv((b * 47 + s * 90) % 360, 170 + (b % 3) * 30));
      bx += bw + 1;
    }
  }
  // desk with lamp and plant
  fillRect(fb, 200, 112, W - 200, 6, rgb(150, 108, 74));
  fillRect(fb, 206, 118, 6, FLOOR_Y - 112, rgb(120, 86, 58));
  fillRect(fb, W - 12, 118, 6, FLOOR_Y - 112, rgb(120, 86, 58));
  fillRect(fb, 290, 70, 4, 42, rgb(60, 60, 70));
  fillTriangle(fb, 278, 72, 306, 72, 292, 58, rgb(240, 200, 90));
  if (e.night()) fillCircle(fb, 292, 92, 18, blend(rgb(230, 216, 200), rgb(255, 240, 180), 120));
  fillRect(fb, 222, 96, 18, 16, rgb(200, 120, 80));
  fillCircle(fb, 231, 90, 10, rgb(80, 160, 90));
  fillCircle(fb, 224, 86, 7, rgb(100, 180, 100));
  floorTiles(fb, rgb(186, 150, 118), rgb(166, 130, 100), rgb(150, 116, 88));   // wooden-ish floor
}

static void balcony(uint16_t *fb, const Env &e, float t) {
  sky(fb, 0, 0, W, FLOOR_Y, e, t);
  // city skyline
  uint16_t bc = e.night() ? rgb(24, 28, 48) : rgb(120, 136, 160);
  for (int k = 0; k < 9; k++) {
    int bx = k * 38 - 6, bw = 30 + (k * 7) % 10, bh = 40 + (k * 29) % 50;
    fillRect(fb, bx, FLOOR_Y - bh, bw, bh, bc);
    if (e.night())
      for (int wy = FLOOR_Y - bh + 6; wy < FLOOR_Y - 6; wy += 9)
        for (int wx = bx + 4; wx < bx + bw - 4; wx += 7)
          if (((wx * 7 + wy * 3) / 5) % 3 == 0) fillRect(fb, wx, wy, 3, 4, rgb(255, 214, 120));
  }
  floorTiles(fb, rgb(206, 128, 92), rgb(184, 108, 76), rgb(160, 90, 62));
  // railing
  uint16_t rail = rgb(60, 64, 72);
  fillRect(fb, 0, 104, W, 5, rail);
  for (int x = 4; x < W; x += 14) fillRect(fb, x, 109, 3, FLOOR_Y - 109, rail);
  // plants
  for (int s = 0; s < 2; s++) {
    int px = s ? 296 : 22;
    fillRect(fb, px - 12, 130, 24, 22, rgb(190, 100, 70));
    for (int k = 0; k < 5; k++) fillCircle(fb, px - 10 + k * 5, 118 + (k % 2) * 6, 8, k % 2 ? rgb(70, 150, 80) : rgb(90, 175, 95));
  }
  if (e.wx == W_RAIN || e.wx == W_STORM) {     // rain over everything
    for (int k = 0; k < 140; k++) {
      int rx = (int)fmodf(k * 37.3f + t * 50, (float)W), ry = (int)fmodf(k * 23.7f + t * 260, (float)H);
      for (int d = 0; d < 8; d++) pixel(fb, rx - d / 3, ry + d, rgb(200, 215, 240));
    }
  }
}

static void draw(uint16_t *fb, int which, const Env &e, float t) {
  switch (which) {
    case STUDY: study(fb, e, t); break;
    case BALCONY: balcony(fb, e, t); break;
    default: bedroom(fb, e, t);
  }
}

// ---------- Props (smooth shapes) ----------
using namespace cg;

static void sunbeam(const Env &e) {
  if (!e.sunny()) return;
  float fx = e.sunX();
  int b = quad(WIN_X, WIN_Y + WIN_H, WIN_X + WIN_W, WIN_Y + WIN_H, fx + 44, CAT_Y + 8, fx - 44, CAT_Y + 8, rgb(255, 244, 200));
  alpha(b, 48);
  int p = ell(fx, CAT_Y - 4, 52, 12, 0, rgb(255, 246, 210));
  alpha(p, 95);
}

// little table; the cup sits on it unless it has been knocked off
static void table(bool cupOn, float cupX) {
  X.set(0, 0, 1, false);
  uint16_t wood = rgb(166, 118, 80), dark = rgb(116, 80, 52);
  int top = quad(TABLE_X0, TABLE_TOP, TABLE_X1, TABLE_TOP, TABLE_X1 + 4, TABLE_TOP + 6, TABLE_X0 - 4, TABLE_TOP + 6, wood);
  out(top, dark, 1);
  cap(TABLE_X0 + 2, TABLE_TOP + 6, TABLE_X0 + 1, 186, 2.2f, dark);
  cap(TABLE_X1 - 2, TABLE_TOP + 6, TABLE_X1 - 1, 186, 2.2f, dark);
  if (cupOn) {
    int c = quad(cupX - 6, TABLE_TOP - 13, cupX + 6, TABLE_TOP - 13, cupX + 5, TABLE_TOP, cupX - 5, TABLE_TOP, rgb(90, 160, 220));
    out(c, rgb(40, 80, 130), 1);
    int h = ell(cupX + 7.5f, TABLE_TOP - 7, 3.5f, 4, 0, rgb(90, 160, 220));
    out(h, rgb(40, 80, 130), 1);
    ell(cupX, TABLE_TOP - 12, 5, 1.5f, 0, rgb(120, 70, 40));
  }
}

static void fallingCup(float x, float y, float rot) {
  X.set(x, y, 1, false, rot);
  int c = quad(-6, -7, 6, -7, 5, 6, -5, 6, rgb(90, 160, 220));
  out(c, rgb(40, 80, 130), 1);
  int h = ell(7.5f, -1, 3.5f, 4, 0, rgb(90, 160, 220));
  out(h, rgb(40, 80, 130), 1);
}

static void cushion(float x) {
  X.set(x, CAT_Y + 2, 1, false);
  int rim = ell(0, -6, 58, 15, 0, rgb(70, 130, 140));
  grad(rim, rgb(46, 96, 104));
  out(rim, rgb(30, 64, 70), 1.2f);
  int in = ell(0, -8, 48, 10, 0, rgb(244, 232, 210));
  grad(in, rgb(226, 210, 184));
}

static void bowl(float x, float food) {
  X.set(x, CAT_Y + 1, 1, false);
  if (food > 0) {
    int f = ell(0, -11, 15, 4 + food * 2, 0, rgb(170, 110, 60));
    for (int k = 0; k < 5; k++) ell(-10 + k * 5, -12 - food * 2 + (k % 2) * 2, 2.5f, 2, 0, rgb(130, 80, 40));
    (void)f;
  }
  int b = quad(-19, -11, 19, -11, 13, 0, -13, 0, rgb(230, 90, 100));
  grad(b, rgb(190, 60, 70));
  out(b, rgb(120, 40, 50), 1.1f);
  ell(0, -11, 19, 3, 0, rgb(250, 140, 150));
  tri(-4, -6, 4, -6, 0, -2, rgb(255, 200, 205));
}

static void box(float x) {
  X.set(x, CAT_Y, 1, false);
  uint16_t cb = rgb(200, 150, 96);
  int f = quad(-44, -40, 44, -40, 42, 2, -42, 2, cb);
  grad(f, rgb(160, 112, 66));
  out(f, rgb(110, 76, 44), 1.3f);
  quad(-44, -40, -24, -40, -34, -52, -54, -50, rgb(214, 166, 112));
  quad(24, -40, 44, -40, 54, -50, 34, -52, rgb(214, 166, 112));
  quad(-5, -40, 5, -40, 5, 2, -5, 2, rgb(222, 196, 150));
}

static void laptop(float x, float t, bool hot) {
  X.set(x, CAT_Y + 1, 1.25f, false);
  int base = quad(-34, -8, 34, -8, 38, 0, -38, 0, rgb(170, 175, 185));
  out(base, rgb(80, 84, 94), 1.1f);
  int scr = quad(-30, -8, 30, -8, 26, -44, -26, -44, rgb(60, 64, 76));
  out(scr, rgb(30, 32, 40), 1.1f);
  int glow = quad(-26, -11, 26, -11, 23, -41, -23, -41, rgb(60, 140, 220));
  grad(glow, rgb(30, 70, 140));
  if (hot)
    for (int k = 0; k < 3; k++) {                // heat shimmer
      float ph = t * 3 + k * 2.1f;
      int w = cap(-14 + k * 14 + sinf(ph) * 3, -52, -14 + k * 14 + sinf(ph + 1) * 3, -64, 1.2f, rgb(255, 160, 80));
      alpha(w, 120);
    }
}

static void keyboard(float x, int litKey) {
  X.set(x, CAT_Y + 1, 1, false);
  int k = quad(-46, -9, 46, -9, 50, 0, -50, 0, rgb(60, 62, 70));
  out(k, rgb(25, 26, 30), 1.1f);
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 10; c++) {
      float kx = -42 + c * 9 + r * 3, ky = -7 + r * 3.5f;
      int key = quad(kx, ky, kx + 7, ky, kx + 7, ky + 2.5f, kx, ky + 2.5f, (r * 10 + c) == litKey ? rgb(255, 200, 80) : rgb(200, 202, 210));
      (void)key;
    }
}

static void yarn(float x, float y, float rot) {
  X.set(x, y, 1, false, rot);
  int b = ell(0, 0, 9, 9, 0, rgb(230, 80, 120));
  out(b, rgb(140, 40, 70), 1.1f);
  for (int k = -1; k <= 1; k++) {
    int s = cap(-7, k * 4 - 2, 7, k * 4 + 2, 0.8f, rgb(255, 150, 180));
    clipTo(s, b);
  }
}

static void feather(float x, float y, float ang) {
  X.set(x, y, 1, false, ang);
  int f = ell(0, 12, 5, 13, 0, rgb(110, 200, 230));
  grad(f, rgb(230, 120, 200));
  out(f, rgb(60, 90, 140), 1);
  cap(0, 0, 0, 25, 0.8f, rgb(240, 240, 240));
}

}  // namespace room
