// chindi_room.h
// Chindi's home, after her photos: a living room with her blue water fountain, her cat
// tree and the grooming brush on the wall corner; a dining room with the dark table, the
// vase of lilies and sunflowers and the shelf of photo frames; the bedroom; the balcony.
// Windows show the real sky: time of day, sun, moon, stars, clouds, rain, snow, lightning.
// Each room has things she can use. You tap them (see Spot).
#pragma once

#include "chindi_gfx.h"

namespace room {

enum { LIVING, DINING, BEDROOM, BALCONY, NROOM };
static const char *const NAMES[NROOM] = {"Living room", "Dining room", "Bedroom", "Balcony"};

// layout shared with the behaviour code
static const int FLOOR_Y = 152;                  // wall meets floor
static const int CAT_Y = 192;                    // Chindi's feet
static const int WIN_X = 92, WIN_Y = 18, WIN_W = 84, WIN_H = 76;
static const int TABLE_X0 = 148, TABLE_X1 = 192, TABLE_TOP = 150;   // bedroom: little table with the cup
static const int LAPTOP_X = 228;
static const int FOUNTAIN_X = 262;               // living room
static const int TREE_POST_X = 23, PERCH_X = 64, PERCH_Y = 142;     // living room: cat tree
static const int BRUSH_X = 296;                  // living room: brush on the wall corner
static const int DTABLE_Y = 137, DTABLE_X = 226, VASE_X = 294;      // dining room: table top, where she stands on it
static const int BED_X = 252, BED_Y = 126;       // bedroom: where she sleeps on the bed
static const int PLANT_X[2] = {22, 296};         // balcony

// Things she can use. A tap on one sends her to it.
enum Spot : uint8_t {
  SP_NONE, SP_FOUNTAIN, SP_TREE, SP_BRUSH, SP_WINDOW, SP_FLOWERS, SP_FRAMES, SP_SWITCH, SP_BED, SP_DOOR,
  SP_PLANT, SP_CUP, SP_CUSHION, NSPOT
};
struct Area {
  uint8_t spot;
  int16_t x, y, w, h;
};
struct Def {
  int16_t sleepX, sleepY;    // where she sleeps: the perch, the cushion, or the bed
  bool cushion, table, window;
  int16_t laptopX;           // where the laptop stands when your PC is busy
  int16_t boxLo, boxHi;      // a cardboard box can appear between these
  Area areas[5];
};
static const int CUSHION_X = 128;                // dining room and balcony
static const Def DEFS[NROOM] = {
  {PERCH_X, PERCH_Y, false, false, true, 172, 130, 200,
   {{SP_TREE, 0, 62, 92, 130}, {SP_WINDOW, 94, 40, 82, 62}, {SP_FOUNTAIN, 238, 132, 50, 48}, {SP_BRUSH, 288, 118, 32, 50},
    {SP_NONE, 0, 0, 0, 0}}},
  {CUSHION_X, CAT_Y, true, false, true, 236, 214, 268,
   {{SP_FRAMES, 4, 66, 86, 42}, {SP_WINDOW, 94, 40, 82, 62}, {SP_FLOWERS, 268, 56, 52, 84}, {SP_CUSHION, 86, 176, 84, 26},
    {SP_NONE, 0, 0, 0, 0}}},
  {BED_X, BED_Y, false, true, true, 228, 60, 110,
   {{SP_DOOR, 6, 42, 66, 110}, {SP_WINDOW, 94, 40, 82, 62}, {SP_SWITCH, 220, 40, 54, 30}, {SP_BED, 198, 98, 122, 74},
    {SP_CUP, 144, 128, 52, 30}}},
  {CUSHION_X, CAT_Y, true, false, false, 228, 204, 256,
   {{SP_PLANT, 0, 100, 46, 56}, {SP_PLANT, 274, 100, 46, 56}, {SP_CUSHION, 86, 176, 84, 26}, {SP_NONE, 0, 0, 0, 0},
    {SP_NONE, 0, 0, 0, 0}}},
};

static uint8_t spotAt(int which, float x, float y) {
  for (const Area &a : DEFS[which].areas)
    if (a.spot && x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h) return a.spot;
  return SP_NONE;
}

// what the sky looks like now
enum Wx { W_CLEAR, W_CLOUDY, W_RAIN, W_SNOW, W_STORM, W_FOG };
struct Env {
  float hour = 14;           // 0..24
  bool timeKnown = false;
  int wx = W_CLEAR;
  bool sunny() const { return (wx == W_CLEAR || wx == W_CLOUDY) && hour > 7.5f && hour < 17.5f; }
  bool night() const { return hour < 6 || hour >= 19.5f; }
  bool wet() const { return wx == W_RAIN || wx == W_SNOW || wx == W_STORM; }
  float sunX() const { return 300 - (hour - 8) / 9 * 250; }    // sunbeam on the floor
};

static uint16_t mix(uint16_t a, uint16_t b, float k) { return blend(a, b, (int)(constrain(k, 0, 1) * 256)); }

static void vgrad(uint16_t *fb, int x, int y, int w, int h, uint16_t top, uint16_t bottom) { gradRect(fb, x, y, w, h, top, bottom); }

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
  gradRect(fb, x, y, w, h, top, bottom);
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
    if (clipIn(sx - r - 1, sy - r - 1) && clipIn(sx + r + 1, sy + r + 1)) fillCircle(fb, sx, sy, r, rgb(255, 236, 160));
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
      if (clipIn(sx, sy) && clipIn(sx + 1, sy + 1)) fillRect(fb, sx, sy, 2, 2, WHITE);
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

// roofs and trees far away, along the bottom of a window
static void skyline(uint16_t *fb, int x, int y, int w, const Env &e) {
  uint16_t far = e.night() ? rgb(20, 24, 44) : rgb(150, 170, 190), near = e.night() ? rgb(14, 22, 30) : rgb(96, 140, 104);
  for (int i = 0; i < w; i++) {
    int hb = 6 + ((i / 11) * 7) % 9;                         // blocks of flats
    fillRect(fb, x + i, y - hb, 1, hb, far);
    int ht = 3 + (int)(2.5f + 2.5f * sinf(i * 0.45f) + 1.5f * sinf(i * 1.3f));   // tree tops
    fillRect(fb, x + i, y - ht, 1, ht, near);
  }
  if (e.night())
    for (int i = 4; i < w - 2; i += 11) pixel(fb, x + i + (i % 3), y - 5 - (i % 4), rgb(255, 214, 120));
}

static void window(uint16_t *fb, const Env &e, float t) {
  const int x = WIN_X, y = WIN_Y, w = WIN_W, h = WIN_H;
  sky(fb, x, y, w, h, e, t);
  skyline(fb, x, y + h, w, e);
  uint16_t fr = rgb(252, 250, 246), frs = rgb(208, 198, 182);
  fillRect(fb, x - 5, y - 5, w + 10, 5, fr);
  fillRect(fb, x - 5, y + h, w + 10, 5, fr);
  fillRect(fb, x - 5, y, 5, h, fr);
  fillRect(fb, x + w, y, 5, h, fr);
  fillRect(fb, x + w / 2 - 2, y, 4, h, fr);
  fillRect(fb, x, y + h / 2 - 2, w, 4, fr);
  vline(fb, x, y, h, frs);                                   // a little depth inside the frame
  hline(fb, x, y, w, frs);
  fillRoundRect(fb, x - 10, y + h + 5, w + 20, 5, 2, fr);    // sill
  hline(fb, x - 8, y + h + 10, w + 16, frs);
  // curtains
  for (int s = 0; s < 2; s++) {
    int cx = s ? x + w + 2 : x - 16;
    for (int j = -10; j < h + 18; j++) {
      int cw = 14 - (j > h / 2 ? (j - h / 2) / 6 : 0);
      for (int i = 0; i < cw; i++) {
        int px = (s ? cx : cx + 14 - cw) + i;
        pixel(fb, px, y + j, (i % 5 == 0) ? rgb(222, 198, 160) : rgb(240, 220, 186));   // soft folds
      }
    }
  }
  fillRoundRect(fb, x - 22, y - 12, w + 44, 4, 2, rgb(150, 120, 90));  // curtain rod
}

// ---------- Walls and floors ----------
static void wall(uint16_t *fb, uint16_t top, uint16_t bottom) {
  gradRect(fb, 0, 0, W, FLOOR_Y, top, bottom);
}

static void baseboard(uint16_t *fb, int x0 = 0, int x1 = W) {
  fillRect(fb, x0, FLOOR_Y - 7, x1 - x0, 7, rgb(252, 250, 244));
  hline(fb, x0, FLOOR_Y - 8, x1 - x0, rgb(218, 208, 190));
  hline(fb, x0, FLOOR_Y - 1, x1 - x0, rgb(196, 186, 170));
}

// big floor tiles, seen at an angle
static void floorTiles(uint16_t *fb, uint16_t a, uint16_t b, uint16_t grout) {
  gradRect(fb, 0, FLOOR_Y, W, H - FLOOR_Y, a, b);
  static const int rows[] = {164, 184, 216};
  for (int r : rows) hline(fb, 0, r, W, grout);
  for (int k = -5; k <= 5; k++) line(fb, 160 + k * 56, FLOOR_Y, 160 + k * 150, H, grout);
}

static void plank(uint16_t *fb, uint16_t a, uint16_t b, uint16_t gap) {
  gradRect(fb, 0, FLOOR_Y, W, H - FLOOR_Y, a, b);
  static const int rows[] = {160, 170, 183, 199, 219};
  for (int r : rows) hline(fb, 0, r, W, gap);
}

// ---------- Living room ----------
// rope-wrapped scratching post
static void post(uint16_t *fb, int x, int y, int w, int h) {
  fillRect(fb, x, y, w, h, rgb(198, 152, 100));
  for (int j = 2; j < h; j += 3) hline(fb, x, y + j, w, rgb(168, 124, 78));
  vline(fb, x, y, h, rgb(170, 126, 80));
  vline(fb, x + w - 1, y, h, rgb(150, 110, 68));
  vline(fb, x + 2, y, h, rgb(220, 178, 126));
}

static void plush(uint16_t *fb, int x, int y, int w, int h, int r) {
  fillRoundRectV(fb, x, y, w, h, r, rgb(190, 190, 196), rgb(160, 160, 168));
  hline(fb, x + r, y + 1, w - 2 * r, rgb(206, 206, 212));
}

static void catTree(uint16_t *fb) {
  plush(fb, 2, 178, 88, 13, 5);                              // base
  post(fb, TREE_POST_X - 7, 92, 14, 88);                     // tall post, the one she scratches
  post(fb, PERCH_X - 6, PERCH_Y + 6, 13, 32);                // short post under the perch
  plush(fb, 36, PERCH_Y - 2, 56, 10, 4);                     // perch
  // top bed with a cloth hanging over its edge
  fillRoundRectV(fb, 0, 78, 50, 16, 7, rgb(170, 170, 178), rgb(146, 146, 154));
  fillRoundRect(fb, 4, 76, 42, 7, 3, rgb(200, 200, 206));
  fillTriangle(fb, 30, 78, 50, 78, 46, 100, rgb(236, 228, 212));
  fillTriangle(fb, 30, 78, 46, 100, 36, 94, rgb(222, 212, 194));
  // pom-pom toy on a string under the perch
  vline(fb, 88, PERCH_Y + 8, 10, rgb(150, 150, 156));
  fillCircle(fb, 88, PERCH_Y + 21, 4, rgb(184, 184, 190));
}

static void cornerBrush(uint16_t *fb) {
  // the wall turns towards you here, and the grooming brush sits on the corner
  gradRect(fb, 302, 0, 18, FLOOR_Y, rgb(238, 228, 206), rgb(222, 208, 184));
  vline(fb, 302, 0, FLOOR_Y, rgb(212, 198, 174));
  fillRoundRect(fb, BRUSH_X - 2, 130, 12, 30, 3, rgb(96, 100, 110));
  for (int j = 0; j < 9; j++)
    for (int i = 0; i < 3; i++) pixel(fb, BRUSH_X - 4 + i * 2 - (j & 1), 133 + j * 3, rgb(150, 154, 164));
}

// The fountain: a blue box that glows, with a white flower where the water comes out.
static void fountain(uint16_t *fb, float t) {
  const int x = FOUNTAIN_X - 17, y = 150;
  // socket and cable
  fillRoundRect(fb, 216, 112, 14, 12, 2, rgb(250, 250, 248));
  roundRect(fb, 216, 112, 14, 12, 2, rgb(206, 200, 190));
  pixel(fb, 220, 117, rgb(120, 120, 120));
  pixel(fb, 225, 117, rgb(120, 120, 120));
  strokeLine(fb, 223, 124, 223, 150, 1.4f, rgb(50, 50, 56));
  strokeLine(fb, 223, 150, x + 2, 166, 1.4f, rgb(50, 50, 56));
  // glow on the floor, then the box
  for (int j = -9; j <= 9; j++)
    for (int i = -34; i <= 34; i++) {
      float d = (i * i) / 1156.0f + (j * j) / 81.0f;
      if (d < 1) pixelA(fb, FOUNTAIN_X + i, 174 + j, rgb(60, 110, 255), (1 - d) * 0.45f);
    }
  fillRoundRectV(fb, x, y, 34, 24, 4, rgb(70, 130, 255), rgb(24, 50, 210));
  fillRoundRect(fb, x + 3, y + 8, 28, 12, 3, rgb(40, 84, 240));
  roundRect(fb, x, y, 34, 24, 4, rgb(150, 190, 255));
  fillRoundRect(fb, x + 2, y - 3, 30, 6, 3, rgb(228, 238, 252));       // water tray
  // the flower spout and the water
  fillRect(fb, FOUNTAIN_X - 1, y - 11, 3, 9, rgb(244, 246, 250));
  for (int k = 0; k < 5; k++) {
    float a = k * 1.2566f - 1.5708f;
    fillCircle(fb, FOUNTAIN_X + (int)roundf(cosf(a) * 4), y - 12 + (int)roundf(sinf(a) * 3), 2, WHITE);
  }
  fillCircle(fb, FOUNTAIN_X, y - 12, 2, rgb(250, 150, 60));
  for (int k = 0; k < 6; k++) {                               // drops falling into the tray
    float ph = fmodf(t * 1.6f + k * 0.17f, 1.0f);
    int side = k & 1 ? 1 : -1;
    pixelA(fb, FOUNTAIN_X + side * (3 + (int)(ph * 6)), y - 11 + (int)(ph * ph * 9), rgb(170, 210, 255), 0.9f);
  }
}

// its light shows at night and with the lights off: call after the room has been dimmed
static void fountainGlow(uint16_t *fb, float k) {
  if (k <= 0) return;
  for (int j = -34; j <= 22; j++)
    for (int i = -52; i <= 52; i++) {
      float d = (i * i) / 2704.0f + (j * j) / (j < 0 ? 1156.0f : 484.0f);
      if (d < 1) pixelA(fb, FOUNTAIN_X + i, 166 + j, rgb(70, 120, 255), (1 - d) * (1 - d) * 0.55f * k);
    }
}

static void living(uint16_t *fb, const Env &e, float t) {
  wall(fb, rgb(247, 239, 221), rgb(234, 222, 200));
  window(fb, e, t);
  floorTiles(fb, rgb(234, 226, 212), rgb(218, 208, 192), rgb(204, 192, 174));
  baseboard(fb);
  cornerBrush(fb);
  catTree(fb);
  fountain(fb, t);
}

// ---------- Dining room ----------
// a framed photo on the shelf: gold frame, and (of course) a picture of her
static void photoFrame(uint16_t *fb, int x, int y, int w, int h, uint16_t frame, int kind) {
  fillRect(fb, x, y, w, h, frame);
  rect(fb, x, y, w, h, blend(frame, BLACK, 70));
  fillRect(fb, x + 3, y + 3, w - 6, h - 6, kind == 0 ? rgb(200, 216, 226) : rgb(226, 214, 196));
  int cx = x + w / 2, cy = y + h / 2 + 2;
  if (kind == 0) {                             // her face
    fillCircle(fb, cx, cy, 5, rgb(250, 248, 244));
    fillTriangle(fb, cx - 6, cy - 2, cx - 2, cy - 5, cx - 5, cy - 9, rgb(236, 150, 70));
    fillTriangle(fb, cx + 6, cy - 2, cx + 2, cy - 5, cx + 5, cy - 9, rgb(236, 150, 70));
    fillRect(fb, cx - 5, cy - 4, 4, 3, rgb(236, 150, 70));
    fillRect(fb, cx + 2, cy - 4, 4, 3, rgb(236, 150, 70));
  } else {                                     // two people
    fillCircle(fb, cx - 3, cy - 3, 2, rgb(120, 80, 60));
    fillCircle(fb, cx + 3, cy - 3, 2, rgb(120, 80, 60));
    fillRect(fb, cx - 6, cy, 5, 6, rgb(180, 60, 90));
    fillRect(fb, cx + 1, cy, 5, 6, rgb(50, 60, 100));
  }
}

static void dining(uint16_t *fb, const Env &e, float t) {
  wall(fb, rgb(244, 238, 226), rgb(230, 222, 206));
  window(fb, e, t);
  // framed picture: dark frame, red field, a round pattern in gold
  fillRect(fb, 236, 44, 62, 58, rgb(40, 30, 26));
  fillRect(fb, 240, 48, 54, 50, rgb(150, 44, 40));
  rect(fb, 243, 51, 48, 44, rgb(210, 170, 90));
  fillCircle(fb, 267, 73, 17, rgb(30, 78, 66));
  circle(fb, 267, 73, 17, rgb(210, 170, 90));
  for (int k = 0; k < 8; k++) fillCircle(fb, 267 + (int)roundf(cosf(k * 0.7854f) * 11), 73 + (int)roundf(sinf(k * 0.7854f) * 11), 3, rgb(210, 170, 90));
  fillCircle(fb, 267, 73, 6, rgb(222, 124, 60));
  fillCircle(fb, 267, 73, 2, rgb(250, 230, 170));
  plank(fb, rgb(214, 204, 188), rgb(196, 184, 166), rgb(182, 170, 152));
  baseboard(fb);
  // dark sideboard with photo frames
  fillRect(fb, 2, 108, 88, 66, rgb(70, 48, 38));
  fillRect(fb, 0, 104, 92, 5, rgb(100, 72, 56));
  hline(fb, 0, 104, 92, rgb(128, 96, 76));
  for (int k = 0; k < 2; k++) {
    rect(fb, 7 + k * 42, 114, 36, 52, rgb(50, 34, 28));
    fillCircle(fb, 38 + k * 12, 140, 2, rgb(200, 170, 110));
  }
  fillRect(fb, 6, 174, 5, 8, rgb(50, 34, 28));
  fillRect(fb, 81, 174, 5, 8, rgb(50, 34, 28));
  photoFrame(fb, 8, 70, 26, 34, rgb(214, 172, 84), 1);
  photoFrame(fb, 38, 76, 28, 28, rgb(214, 172, 84), 0);
  photoFrame(fb, 70, 84, 16, 20, rgb(190, 190, 198), 1);
  // chairs behind the table
  for (int k = 0; k < 2; k++) {
    int cx = 214 + k * 62;
    fillRoundRect(fb, cx - 15, 92, 30, 46, 4, rgb(48, 34, 30));
    for (int s = 0; s < 3; s++) fillRect(fb, cx - 9 + s * 7, 99, 4, 32, mix(rgb(244, 238, 226), rgb(230, 222, 206), 0.7f));
    fillCircle(fb, cx - 13, 92, 3, rgb(48, 34, 30));
    fillCircle(fb, cx + 13, 92, 3, rgb(48, 34, 30));
  }
  // the table: dark and shiny
  fillRect(fb, 190, DTABLE_Y, 130, 11, rgb(44, 32, 30));
  hline(fb, 190, DTABLE_Y, 130, rgb(120, 104, 100));
  for (int i = 0; i < 70; i++) pixelA(fb, 200 + i, DTABLE_Y + 3 + (i % 3 == 0), rgb(150, 140, 140), 0.5f - fabsf(i - 35) / 70.0f);
  fillRect(fb, 190, DTABLE_Y + 11, 130, 3, rgb(28, 20, 18));
  fillRect(fb, 196, DTABLE_Y + 14, 7, 44, rgb(36, 26, 24));
  fillRect(fb, 311, DTABLE_Y + 14, 7, 44, rgb(36, 26, 24));
}

// ---------- Bedroom ----------
static void bedroom(uint16_t *fb, const Env &e, float t) {
  wall(fb, rgb(244, 233, 211), rgb(230, 214, 188));
  window(fb, e, t);
  // door, lever handle and lock, like the photo
  fillRect(fb, 6, 26, 66, FLOOR_Y - 26, rgb(206, 192, 168));
  gradRect(fb, 10, 30, 58, FLOOR_Y - 30, rgb(242, 233, 214), rgb(232, 220, 198));
  rect(fb, 16, 44, 46, 44, rgb(222, 210, 188));
  rect(fb, 16, 96, 46, 48, rgb(222, 210, 188));
  fillRoundRect(fb, 53, 86, 14, 3, 1, rgb(150, 150, 150));
  fillCircle(fb, 56, 87, 3, rgb(176, 176, 176));
  fillCircle(fb, 57, 97, 2, rgb(150, 150, 150));
  // light switch plate above the bed
  fillRoundRect(fb, 228, 48, 38, 20, 3, rgb(250, 250, 248));
  roundRect(fb, 228, 48, 38, 20, 3, rgb(210, 205, 196));
  for (int k = 0; k < 3; k++) fillRoundRect(fb, 233 + k * 10, 53, 7, 10, 1, k == 2 ? rgb(232, 232, 228) : rgb(244, 244, 240));
  // headboard
  fillRect(fb, 196, 72, W - 196, FLOOR_Y - 72, rgb(84, 56, 40));
  fillRect(fb, 196, 72, W - 196, 3, rgb(120, 84, 60));
  floorTiles(fb, rgb(232, 224, 210), rgb(218, 208, 192), rgb(206, 194, 176));
  baseboard(fb, 72, 196);
  // doormat: dark half circle at the door
  for (int j = 0; j < 12; j++) {
    int hw = (int)(36 * sqrtf(1 - (j / 12.0f) * (j / 12.0f)));
    hline(fb, 39 - hw, FLOOR_Y + j, 2 * hw, rgb(92, 94, 96));
  }
  // bed: grey sheet with a paisley pattern, pillow
  uint16_t sheet = rgb(196, 198, 210), motif = rgb(150, 152, 168);
  gradRect(fb, 184, 118, W - 184, 52, rgb(204, 206, 218), rgb(188, 190, 204));
  fillRect(fb, 184, 168, W - 184, 12, rgb(170, 172, 186));
  fillRoundRect(fb, 210, 100, 80, 28, 8, rgb(218, 220, 230));
  roundRect(fb, 210, 100, 80, 28, 8, rgb(170, 172, 186));
  (void)sheet;
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 6; i++) {
      int px = 196 + i * 24 + (j % 2) * 12, py = 128 + j * 15;
      circle(fb, px, py, 5, motif);
      fillCircle(fb, px, py, 1, motif);
      line(fb, px + 4, py - 3, px + 9, py - 8, motif);
    }
  for (int i = 0; i < 3; i++) circle(fb, 224 + i * 24, 113, 4, motif);
}

// ---------- Balcony ----------
static void balcony(uint16_t *fb, const Env &e, float t) {
  sky(fb, 0, 0, W, FLOOR_Y, e, t);
  // city skyline
  uint16_t bc = e.night() ? rgb(24, 28, 48) : rgb(120, 136, 160);
  for (int k = 0; k < 9; k++) {
    int bx = k * 38 - 6, bw = 30 + (k * 7) % 10, bh = 40 + (k * 29) % 50;
    fillRect(fb, bx, FLOOR_Y - bh, bw, bh, bc);
    for (int wy = FLOOR_Y - bh + 6; wy < FLOOR_Y - 6; wy += 9)
      for (int wx = bx + 4; wx < bx + bw - 4; wx += 7) {
        bool lit = ((wx * 7 + wy * 3) / 5) % 3 == 0;
        if (e.night() ? lit : true) fillRect(fb, wx, wy, 3, 4, e.night() ? rgb(255, 214, 120) : (lit ? rgb(150, 170, 196) : rgb(104, 120, 144)));
      }
  }
  floorTiles(fb, rgb(206, 128, 92), rgb(184, 108, 76), rgb(160, 90, 62));
  // railing
  uint16_t rail = rgb(60, 64, 72);
  fillRoundRect(fb, 0, 103, W, 6, 2, rail);
  hline(fb, 0, 103, W, rgb(110, 114, 124));
  for (int x = 4; x < W; x += 14) fillRect(fb, x, 109, 3, FLOOR_Y - 109, rail);
  // plants
  for (int s = 0; s < 2; s++) {
    int px = PLANT_X[s];
    fillRoundRectV(fb, px - 13, 130, 26, 23, 3, rgb(204, 112, 78), rgb(170, 86, 58));
    fillRect(fb, px - 15, 128, 30, 5, rgb(214, 124, 88));
    for (int k = 0; k < 5; k++) fillCircle(fb, px - 10 + k * 5, 118 + (k % 2) * 6, 8, k % 2 ? rgb(70, 150, 80) : rgb(90, 175, 95));
    for (int k = 0; k < 3; k++) fillCircle(fb, px - 6 + k * 6, 110 + (k % 2) * 4, 5, rgb(110, 190, 104));
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
    case DINING: dining(fb, e, t); break;
    case BEDROOM: bedroom(fb, e, t); break;
    case BALCONY: balcony(fb, e, t); break;
    default: living(fb, e, t);
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
  int sh = ell((TABLE_X0 + TABLE_X1) / 2, 187, 30, 4, 0, BLACK);
  alpha(sh, 36);
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
  int sh = ell(0, -2, 60, 9, 0, BLACK);
  alpha(sh, 30);
  int rim = ell(0, -6, 58, 15, 0, rgb(70, 130, 140));
  grad(rim, rgb(46, 96, 104));
  out(rim, rgb(30, 64, 70), 1.2f);
  int in = ell(0, -8, 48, 10, 0, rgb(244, 232, 210));
  grad(in, rgb(226, 210, 184));
}

// the vase on the dining table: lilies and sunflowers. sway moves the flowers a little
static void flowers(float sway) {
  X.set(VASE_X, DTABLE_Y, 1, false);
  uint16_t leaf = rgb(70, 140, 70), leafD = rgb(46, 104, 52);
  // stems and leaves first
  cap(-3, -18, -12 + sway, -50, 1.1f, leafD);
  cap(2, -18, 10 + sway, -58, 1.1f, leafD);
  cap(0, -18, -2 + sway, -64, 1.1f, leafD);
  for (int k = 0; k < 4; k++) {
    int l = ell(-8 + k * 6, -30 - (k % 2) * 6, 7, 3, 0.9f - k * 0.6f, leaf);
    out(l, leafD, 0.6f);
  }
  // sunflowers, low on the right
  for (int s = 0; s < 2; s++) {
    float fx = 13 + s * 8 + sway * 0.5f, fy = -36 - s * 9;
    for (int k = 0; k < 8; k++) {
      float a = k * 0.7854f;
      ell(fx + cosf(a) * 5.5f, fy + sinf(a) * 5.5f, 3.2f, 1.9f, a, rgb(250, 200, 40));
    }
    int c = ell(fx, fy, 3.6f, 3.6f, 0, rgb(110, 70, 30));
    out(c, rgb(70, 44, 20), 0.6f);
  }
  // lilies: pink petals with a pale edge, and a bud
  auto lily = [&](float fx, float fy, float turn, float size) {
    for (int k = 0; k < 5; k++) {
      float a = turn + k * 1.2566f;
      int p = ell(fx + cosf(a) * 5 * size, fy + sinf(a) * 5 * size, 6.5f * size, 2.6f * size, a, rgb(238, 120, 170));
      grad(p, rgb(250, 190, 214));
      out(p, rgb(196, 80, 130), 0.5f);
    }
    ell(fx, fy, 1.8f * size, 1.8f * size, 0, rgb(250, 236, 170));
  };
  lily(-12 + sway, -52, 0.3f, 1);
  lily(10 + sway, -60, 1.1f, 0.9f);
  int bud = ell(-2 + sway, -68, 3, 7, 0.1f, rgb(226, 190, 196));
  out(bud, rgb(170, 110, 130), 0.6f);
  // the cut-glass vase
  int v = quad(-8, -24, 8, -24, 6, 0, -6, 0, rgb(214, 232, 238));
  alpha(v, 200);
  out(v, rgb(140, 170, 184), 0.9f);
  for (int k = -1; k <= 1; k++) {
    int f = cap(k * 4, -21, k * 3, -3, 0.6f, WHITE);
    alpha(f, 150);
  }
}

// a sparrow. On the window sill, or on the balcony railing
static void bird(float x, float y, bool flip, float t, bool flying) {
  X.set(x, y, 1, flip);
  uint16_t b = rgb(134, 100, 72), d = rgb(84, 60, 44);
  if (flying) {
    float fl = sinf(t * 22);
    tri(-2, -6, 4, -6, 1, -6 - 9 * fl, d);
  }
  int tail = tri(-7, -6, -13, -9, -12, -3, d);
  (void)tail;
  int body = ell(0, -5, 6.5f, 4.6f, -0.15f, b);
  out(body, d, 0.7f);
  ell(1, -3.5f, 4.2f, 2.6f, -0.1f, rgb(226, 214, 196));
  int head = ell(5.5f, -9, 3.6f, 3.4f, 0, b);
  out(head, d, 0.7f);
  tri(8.5f, -9.5f, 12, -8.5f, 8.5f, -7.5f, rgb(240, 180, 70));
  ell(6.5f, -9.6f, 0.8f, 0.8f, 0, BLACK);
  if (!flying) {
    cap(-1, -1, -1, 1.5f, 0.4f, d);
    cap(2, -1, 2, 1.5f, 0.4f, d);
  }
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

// Small things she brings you. Drawn about 14 px wide, centred on (x, y).
enum Gift : uint8_t { GF_LEAF, GF_SOCK, GF_CAP, GF_TIE, GF_MOUSE, GF_PETAL, GF_FEATHER, GF_BUTTON, GF_COIN, GF_RIBBON, GF_SHELL, GF_STAR, NGIFT };
static const char *const GIFT_NAMES[NGIFT] = {"Leaf", "Sock", "Bottle cap", "Hair tie", "Toy mouse", "Petal", "Feather",
                                              "Button", "Coin", "Ribbon", "Shell", "Lucky star"};

static void gift(uint8_t g, float x, float y, float s = 1) {
  X.set(x, y, s, false);
  switch (g) {
    case GF_LEAF: {
      int l = ell(0, 0, 7, 3.6f, -0.5f, rgb(96, 170, 84));
      out(l, rgb(50, 110, 54), 0.8f);
      cap(-5, 3, 5, -3, 0.5f, rgb(50, 110, 54));
      break;
    }
    case GF_SOCK: {
      int a = cap(-2, -6, -2, 2, 3.4f, rgb(120, 160, 230));
      out(a, rgb(60, 90, 160), 0.8f);
      int b = cap(-2, 3, 4, 4, 3.4f, rgb(120, 160, 230));
      out(b, rgb(60, 90, 160), 0.8f);
      cap(-5, -7, 1, -7, 1.2f, WHITE);
      break;
    }
    case GF_CAP: {
      int c = ell(0, 0, 6.5f, 6.5f, 0, rgb(220, 70, 70));
      out(c, rgb(130, 30, 30), 0.9f);
      ell(0, 0, 3.6f, 3.6f, 0, rgb(250, 190, 190));
      break;
    }
    case GF_TIE: {
      int o = ell(0, 0, 6.5f, 5, 0.3f, rgb(250, 120, 190));
      out(o, rgb(170, 60, 120), 0.8f);
      ell(0, 0, 3.8f, 2.6f, 0.3f, rgb(214, 204, 188));
      break;
    }
    case GF_MOUSE: {
      int b = ell(0, 1, 6.5f, 4.2f, 0, rgb(150, 150, 160));
      out(b, rgb(84, 84, 96), 0.8f);
      ell(-4, -3, 2.2f, 2.2f, 0, rgb(240, 170, 180));
      cap(6, 2, 10, -2, 0.5f, rgb(84, 84, 96));
      ell(-5, 1, 0.8f, 0.8f, 0, BLACK);
      break;
    }
    case GF_PETAL: {
      int p = ell(0, 0, 6.5f, 3.4f, 0.6f, rgb(240, 130, 176));
      grad(p, rgb(252, 200, 220));
      out(p, rgb(190, 80, 130), 0.6f);
      break;
    }
    case GF_FEATHER: {
      int f = ell(0, 0, 3, 7.5f, 0.5f, rgb(140, 200, 230));
      out(f, rgb(70, 110, 150), 0.7f);
      cap(-3, 6, 3, -6, 0.5f, WHITE);
      break;
    }
    case GF_BUTTON: {
      int c = ell(0, 0, 6, 6, 0, rgb(250, 210, 90));
      out(c, rgb(160, 110, 30), 0.9f);
      for (int k = 0; k < 4; k++) ell(-2 + (k % 2) * 4, -2 + (k / 2) * 4, 0.9f, 0.9f, 0, rgb(120, 80, 20));
      break;
    }
    case GF_COIN: {
      int c = ell(0, 0, 6.5f, 6.5f, 0, rgb(210, 214, 222));
      out(c, rgb(120, 124, 136), 0.9f);
      ell(0, 0, 4.2f, 4.2f, 0, rgb(236, 238, 244));
      break;
    }
    case GF_RIBBON: {
      int a = tri(0, 0, -8, -5, -8, 5, rgb(120, 200, 150));
      out(a, rgb(50, 120, 80), 0.7f);
      int b = tri(0, 0, 8, -5, 8, 5, rgb(120, 200, 150));
      out(b, rgb(50, 120, 80), 0.7f);
      ell(0, 0, 2.2f, 2.2f, 0, rgb(60, 140, 96));
      break;
    }
    case GF_SHELL: {
      int sh = ell(0, 1, 7, 5.4f, 0, rgb(250, 226, 200));
      out(sh, rgb(190, 150, 120), 0.8f);
      for (int k = -2; k <= 2; k++) cap(0, 6, k * 3, -3, 0.4f, rgb(210, 170, 140));
      break;
    }
    default: {                                   // lucky star
      for (int k = 0; k < 5; k++) {
        float a = k * 1.2566f - 1.5708f;
        tri(cosf(a) * 7.5f, sinf(a) * 7.5f, cosf(a + 2.2f) * 3, sinf(a + 2.2f) * 3, cosf(a - 2.2f) * 3, sinf(a - 2.2f) * 3, rgb(255, 214, 80));
      }
      ell(0, 0, 3.4f, 3.4f, 0, rgb(255, 214, 80));
    }
  }
}

}  // namespace room
