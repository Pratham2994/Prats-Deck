// chindi_cat.h
// Chindi, drawn from smooth shapes after her photos: a white cat with an orange cap over
// both ears, a white blaze, a pink nose, gold-green eyes, a striped orange saddle on her
// back, an orange patch on the back leg and a tail with orange and cream rings.
// Front poses (sit, loaf, groom, knead, box), side poses (walk, run, crouch, pounce, bat,
// eat), a curled sleeping pose, and her favourite: flat on her side.
#pragma once

#include "chindi_gfx.h"

namespace kitty {

// ---------- Colours (from her photos) ----------
static const uint16_t FUR = rgb(252, 249, 244), FUR_SH = rgb(228, 220, 210), FUR_FAR = rgb(214, 205, 194);
static const uint16_t ORG = rgb(236, 150, 70), ORG_L = rgb(246, 184, 112), ORG_D = rgb(206, 120, 50);
static const uint16_t STRIPE = rgb(212, 122, 50), CREAM = rgb(250, 228, 194);
static const uint16_t PINK = rgb(244, 176, 170), NOSE = rgb(236, 150, 152), NOSE_D = rgb(200, 116, 122);
static const uint16_t IRIS_T = rgb(236, 192, 66), IRIS_B = rgb(180, 180, 70), PUPIL = rgb(24, 22, 20);
static const uint16_t LINE = rgb(122, 94, 78), LID = rgb(70, 50, 40), SOFT = rgb(204, 192, 180), MOUTH = rgb(140, 100, 90);
static const uint16_t WHISK = rgb(255, 255, 255), WHISK_O = rgb(186, 178, 170);

enum Pose : uint8_t { SIT, LOAF, GROOM, KNEAD, WALK, RUN, CROUCH, POUNCE, BAT, EAT, CURL, FLOP };
enum Mouth : uint8_t { M_CLOSED, M_MEOW, M_HISS, M_TONGUE, M_YAWN };
enum Acc : uint8_t { A_NONE, A_BANDANA, A_BOW, A_BELL, A_HAT, A_CROWN, NACC };

struct Look {
  float x = 160, y = 198, s = 0.85f;  // feet position on screen, scale
  uint8_t pose = SIT;
  bool flip = false;                  // side poses face right unless flipped
  float t = 0;                        // time, s
  float phase = 0;                    // walk cycle
  float eyeOpen = 1, pupil = 0.3f;    // pupil: 0 = slit, 1 = round and huge
  float lookX = 0, lookY = 0;         // -1..1
  float earBack = 0;                  // 0 normal, 1 flat (annoyed)
  float twitchL = 0, twitchR = 0;
  float tilt = 0;                     // head tilt, radians
  float tailSwing = 1, tailPuff = 0;
  uint8_t mouth = M_CLOSED;
  bool happy = false;                 // ^^ eyes
  float breath = 0;                   // radians
  float purr = 0;                     // 0..1 body vibration
  float pawL = 0, pawR = 0;           // paw lift (kneading)
  float crouch = 0, wiggle = 0;       // side: crouch amount, rear wiggle
  float stretch = 0;                  // side: pounce stretch
  float bat = 0;                      // side: paw raised
  float headDown = 0;                 // side: eating
  float groom = 0;                    // front: paw to mouth
  uint8_t acc = A_NONE;
  bool glasses = false, dirty = false, inBox = false;
};

// Where things are on screen, for touch.
struct Hit {
  float hx, hy, hr;                   // head
  float nx, ny;                       // nose
  float ex[2], ey[2];                 // eyes
  float cx, cy;                       // chin
  float bx, by, brx, bry;             // body
  float tx[4], ty[4];                 // tail
  float top;                          // top of the head
  float pawX, pawY;                   // reaching paw (side)
};
static Hit hit;

using namespace cg;

// ---------- Pieces ----------
static void bez(const float *p, float t, float &x, float &y) {
  float u = 1 - t;
  x = u * u * u * p[0] + 3 * u * u * t * p[2] + 3 * u * t * t * p[4] + t * t * t * p[6];
  y = u * u * u * p[1] + 3 * u * u * t * p[3] + 3 * u * t * t * p[5] + t * t * t * p[7];
}

// ringed tail along a curve, segments k0..k1 of n: orange with cream rings and a dark tip
static void tailCurve(const float *p, int n, int k0, int k1, float r0, float r1, float puff, int grp) {
  for (int k = k0; k < k1; k++) {
    float xa, ya, xb, yb;
    bez(p, (float)k / n, xa, ya);
    bez(p, (float)(k + 1) / n, xb, yb);
    float r = (r0 + (r1 - r0) * k / n) * (1 + puff * 0.7f);
    int i = cap(xa, ya, xb, yb, r, (k % 3 == 2) ? CREAM : ORG);
    out(i, LINE, 1.1f);
    group(i, grp);
    if (k == n - 1) {                        // darker tip
      int j = ell(xb, yb, r * 0.95f, r * 0.95f, 0, ORG_D);
      out(j, LINE, 1.1f);
      group(j, grp);
    }
  }
}

static void tailPoints(const float *p) {
  for (int k = 0; k < 4; k++) {
    float x, y;
    bez(p, 0.3f + 0.23f * k, x, y);
    X.apply(x, y, hit.tx[k], hit.ty[k]);
  }
}

// a few short tabby stripes across a patch, clipped to shape `clip`
static void stripes(float x, float y, float dx, int n, float len, float lean, int clip, float w = 1.6f) {
  for (int k = 0; k < n; k++) {
    int s = cap(x + k * dx, y, x + k * dx + lean, y + len, w, STRIPE);
    clipTo(s, clip);
    alpha(s, 190);
  }
}

// pink toe pads on the sole of a paw
static void beans(float x, float y, float s) {
  ell(x, y + 1.2f * s, 2.6f * s, 2.1f * s, 0, PINK);
  for (int k = -1; k <= 1; k++) ell(x + k * 2.7f * s, y - 2.0f * s, 1.2f * s, 1.3f * s, 0, PINK);
}

// closed eye as a curve: up = happy ^ shape, else a sleepy smile
static void closedEye(float ex, float ey, float w, bool up) {
  float px = 0, py = 0;
  for (int k = 0; k <= 4; k++) {
    float u = k / 4.0f, x = ex - w + 2 * w * u;
    float y = up ? ey + 2 - 5 * sinf(3.14159f * u) : ey - 1 + 3.5f * sinf(3.14159f * u);
    if (k) cap(px, py, x, y, 1.1f, LID);
    px = x;
    py = y;
  }
}

static void eye(const Look &L, float ex, float ey, float rx, float ry, float ang, float pr, float lookScale) {
  float o = L.eyeOpen;
  if (L.happy) {
    closedEye(ex, ey, rx, true);
    return;
  }
  if (o < 0.14f) {
    closedEye(ex, ey, rx, false);
    return;
  }
  float cy = ey + (1 - o) * ry * 0.45f;
  ell(ex, cy - 0.8f, rx + 0.7f, ry * o + 0.3f, ang, LID);     // dark upper lid, like eyeliner
  int iris = ell(ex, cy, rx, ry * o, ang, IRIS_T);
  grad(iris, IRIS_B);
  out(iris, LID, 0.8f);
  int p = ell(ex + L.lookX * 3.0f * lookScale, cy + L.lookY * 1.8f * lookScale, 1.5f + pr * 4.3f, ry * 0.88f * o, 0, PUPIL);
  clipTo(p, iris);
  int h1 = ell(ex - rx * 0.3f + L.lookX * lookScale, cy - ry * 0.38f * o, rx * 0.25f, rx * 0.23f, 0, WHITE);
  clipTo(h1, iris);
  int h2 = ell(ex + rx * 0.32f, cy + ry * 0.3f * o, rx * 0.11f, rx * 0.11f, 0, WHITE);
  clipTo(h2, iris);
  alpha(h2, 170);
}

static void whiskers(float rootX, float rootY, float side, float len, float droop) {
  for (int k = 0; k < 3; k++) {
    int i = cap(rootX, rootY + k * 2.4f, rootX + side * len, rootY - 3 + k * (6 + droop), 0.5f, WHISK);
    out(i, WHISK_O, 0.4f);
    alpha(i, 215);
  }
}

static void accessoryNeck(uint8_t acc, float y) {
  if (acc == A_BANDANA) {
    int i = tri(-25, y - 1, 25, y - 1, 0, y + 24, rgb(214, 52, 58));
    out(i, rgb(120, 30, 34), 1.1f);
    for (int k = 0; k < 4; k++) ell(-12 + k * 8 - (k > 1 ? 4 : 0), y + 5 + (k % 2) * 7, 1.6f, 1.6f, 0, WHITE);
  } else if (acc == A_BELL) {
    int i = cap(-23, y, 23, y, 3.4f, rgb(56, 110, 210));
    out(i, rgb(28, 50, 110), 1);
    int b = ell(0, y + 7, 5, 5, 0, rgb(250, 200, 60));
    out(b, rgb(150, 105, 20), 1.1f);
    cap(-2.5f, y + 8.5f, 2.5f, y + 8.5f, 0.6f, rgb(120, 80, 10));
    ell(-1.5f, y + 5, 1.4f, 1.2f, 0, rgb(255, 245, 200));
  }
}

static void accessoryHead(uint8_t acc) {
  if (acc == A_BOW) {
    int a = tri(14, -150, 14, -136, 27, -143, rgb(240, 88, 150));
    int b = tri(40, -152, 40, -134, 27, -143, rgb(240, 88, 150));
    out(a, rgb(150, 40, 90), 1);
    out(b, rgb(150, 40, 90), 1);
    int c = ell(27, -143, 4, 4, 0, rgb(255, 130, 185));
    out(c, rgb(150, 40, 90), 1);
  } else if (acc == A_HAT) {
    int i = tri(-15, -149, 15, -149, 3, -192, rgb(80, 170, 240));
    out(i, rgb(30, 70, 120), 1.1f);
    int s1 = quad(-11, -160, 12, -160, 10, -166, -8, -166, rgb(255, 210, 70));
    clipTo(s1, i);
    int s2 = quad(-5, -176, 8, -176, 7, -181, -2, -181, rgb(255, 90, 120));
    clipTo(s2, i);
    int p = ell(3, -193, 4.5f, 4.5f, 0, rgb(255, 90, 120));
    out(p, rgb(150, 40, 60), 1);
  } else if (acc == A_CROWN) {
    uint16_t g = rgb(250, 200, 60), gd = rgb(160, 110, 20);
    int b = quad(-20, -147, 20, -147, 19, -158, -19, -158, g);
    out(b, gd, 1.1f);
    for (int k = 0; k < 3; k++) {
      int t = tri(-20 + k * 14, -157, -8 + k * 14, -157, -14 + k * 14 + 0, -174, g);
      out(t, gd, 1.1f);
    }
    ell(0, -152, 2.4f, 2.4f, 0, rgb(220, 40, 70));
    ell(-12, -152, 1.8f, 1.8f, 0, rgb(60, 140, 230));
    ell(12, -152, 1.8f, 1.8f, 0, rgb(60, 200, 120));
  }
}

// ---------- Front head (also used for the home screen chip, peek and icon) ----------
// The head is drawn round (0, -122). details = false leaves out the small parts.
static void headFront(const Look &L, bool details = true) {
  // ears (drawn first, so the head covers their base)
  for (int side = -1; side <= 1; side += 2) {
    float tw = side < 0 ? L.twitchL : L.twitchR;
    Xf save = X;
    X = X.rotAbout(side * 24, -134, side * (L.earBack * 0.75f + tw * 0.25f));
    int o = tri(side * 9, -147, side * 39, -122, side * 35, -176, ORG);
    grad(o, ORG_D);
    out(o, LINE, 1.2f);
    group(o, 2);
    int in = tri(side * 16, -143, side * 32, -128, side * 32, -165, PINK);
    grad(in, ORG_L);
    alpha(in, 235);
    if (details) {                             // pale fur at the base of the ear
      int tuft = tri(side * 17, -140, side * 27, -133, side * 25, -153, FUR);
      alpha(tuft, 215);
    }
    X = save;
  }
  // cheek fur points
  for (int side = -1; side <= 1; side += 2) {
    int c = tri(side * 35, -116, side * 46, -105, side * 34, -98, FUR);
    out(c, LINE, 1.2f);
    group(c, 2);
  }
  int head = ell(0, -122, 37, 31, 0, FUR);
  out(head, LINE, 1.2f);
  group(head, 2);
  int cheeks = ell(0, -108, 40, 21, 0, FUR);
  grad(cheeks, rgb(240, 234, 226));
  out(cheeks, LINE, 1.2f);
  group(cheeks, 2);
  // orange cap round the eyes, white blaze down the middle
  for (int side = -1; side <= 1; side += 2) {
    int p = ell(side * 25, -131, 25, 23, side * -0.25f, ORG);
    grad(p, ORG_L);
    clipTo(p, head);
    if (details) {                             // tabby marks on the forehead
      int s1 = cap(side * 13, -151, side * 17, -141, 1.2f, STRIPE);
      clipTo(s1, head);
      int s2 = cap(side * 23, -149, side * 27, -139, 1.2f, STRIPE);
      clipTo(s2, head);
    }
  }
  int blaze = quad(-3.2f, -156, 3.2f, -156, 10, -110, -10, -110, FUR);
  clipTo(blaze, head);
  int muzzle = ell(0, -104, 19, 12.5f, 0, rgb(255, 254, 251));
  clipTo(muzzle, cheeks);
  if (L.dirty) {
    int d1 = ell(-20, -114, 2.2f, 1.6f, 0.4f, rgb(140, 118, 96));
    alpha(d1, 150);
    int d2 = ell(24, -102, 1.8f, 1.4f, 0, rgb(140, 118, 96));
    alpha(d2, 150);
  }
  // eyes: almond shaped, the outer corners a little higher
  for (int side = -1; side <= 1; side += 2) eye(L, side * 15.5f, -116, 8.8f, 7.9f, side * -0.2f, L.pupil, 1);
  if (!details) return;
  // nose and mouth
  int nose = tri(-5, -106.5f, 5, -106.5f, 0, -101, NOSE);
  out(nose, NOSE_D, 0.8f);
  ell(-1.5f, -105.3f, 1.3f, 0.8f, 0, rgb(252, 208, 208));
  switch (L.mouth) {
    case M_MEOW:
    case M_YAWN: {
      float h = L.mouth == M_YAWN ? 7.5f : 4.2f;
      int m = ell(0, -95 + h * 0.4f, L.mouth == M_YAWN ? 6.5f : 4.6f, h, 0, rgb(120, 40, 50));
      out(m, LINE, 1);
      int tg = ell(0, -93 + h * 0.8f, 3.4f, 2.2f, 0, PINK);
      clipTo(tg, m);
      break;
    }
    case M_HISS: {
      int m = ell(0, -94, 8, 5.2f, 0, rgb(110, 34, 44));
      out(m, LINE, 1);
      tri(-6, -97.5f, -3.5f, -97.5f, -4.8f, -93.5f, WHITE);
      tri(6, -97.5f, 3.5f, -97.5f, 4.8f, -93.5f, WHITE);
      break;
    }
    default:
      cap(0, -101, 0, -98, 0.65f, MOUTH);
      cap(0, -98, -3.2f, -95.8f, 0.65f, MOUTH);
      cap(-3.2f, -95.8f, -6.2f, -97.4f, 0.65f, MOUTH);
      cap(0, -98, 3.2f, -95.8f, 0.65f, MOUTH);
      cap(3.2f, -95.8f, 6.2f, -97.4f, 0.65f, MOUTH);
      if (L.mouth == M_TONGUE) {
        int tg = ell(0, -94, 2.6f, 2.6f, 0, PINK);
        out(tg, NOSE_D, 0.6f);
      }
  }
  whiskers(-11, -101, -1, 34, 2.5f);
  whiskers(11, -101, 1, 34, 2.5f);
  accessoryHead(L.acc);
  if (L.glasses) {
    for (int side = -1; side <= 1; side += 2) {
      int g = ell(side * 15.5f, -116, 12, 10.5f, 0, rgb(200, 230, 255));
      alpha(g, 40);
      out(g, rgb(40, 34, 30), 1.4f);
    }
    cap(-3.5f, -118, 3.5f, -118, 0.9f, rgb(40, 34, 30));
  }
}

// touch areas of the front head. Call with X set as for headFront.
static void headHits(float scale) {
  X.apply(0, -122, hit.hx, hit.hy);
  hit.hr = 40 * scale;
  X.apply(0, -103, hit.nx, hit.ny);
  X.apply(-15.5f, -116, hit.ex[0], hit.ey[0]);
  X.apply(15.5f, -116, hit.ex[1], hit.ey[1]);
  X.apply(0, -90, hit.cx, hit.cy);
  float tmp;
  X.apply(0, -175, tmp, hit.top);
}

// ---------- Front poses: SIT, LOAF, GROOM, KNEAD (+ in a box) ----------
static void front(const Look &L) {
  float shake = sinf(L.t * 70) * L.purr * 0.6f;
  X.set(L.x + shake, L.y, L.s, L.flip);
  float br = sinf(L.breath);
  bool loaf = L.pose == LOAF;
  // In a loaf she folds her legs under her: a wide, low body with the head set into its front.
  float headDrop = loaf ? 45 : 0;

  int sh = ell(0, -1, loaf ? 56 : 46, 7, 0, BLACK);
  alpha(sh, 45);

  // tail: starts behind the body, wraps round the front paws
  float sway = sinf(L.t * 1.3f) * 7 * L.tailSwing + sinf(L.t * 4.1f) * 2 * L.tailSwing;
  float tp[8] = {24, -22, 62, -2, 32, 13, -36 + sway, 6};
  if (loaf) {
    float lp[8] = {42, -14, 66, 2, 26, 9, -40 + sway, 1};
    memcpy(tp, lp, sizeof(tp));
  }
  if (!L.inBox) tailCurve(tp, 16, 0, 5, 7.5f, 5.5f, L.tailPuff, 3);

  // body
  int body;
  if (loaf) {
    body = ell(0, -27 - br * 0.5f, 51 + br * 0.6f, 28 + br * 0.7f, 0, FUR);
  } else {
    body = ell(0, -51, 35 + br * 0.9f, 48 + br * 0.5f, 0, FUR);
    int hl = ell(-22, -23, 19, 22, 0.25f, FUR);
    int hr = ell(22, -23, 19, 22, -0.25f, FUR);
    out(hl, LINE, 1.2f);
    out(hr, LINE, 1.2f);
    group(hl, 1);
    group(hr, 1);
    grad(hl, FUR_SH);
    grad(hr, FUR_SH);
  }
  grad(body, FUR_SH);
  out(body, LINE, 1.2f);
  group(body, 1);
  // orange saddle and hip patches, with tabby stripes
  if (loaf) {
    int p = ell(0, -53, 50, 17, 0, ORG);
    grad(p, ORG_L);
    clipTo(p, body);
    stripes(-40, -55, 11, 8, 12, 2, body);
    int q = ell(42, -30, 16, 20, 0, ORG);
    clipTo(q, body);
    int bib = ell(0, -26, 22, 20, 0, rgb(255, 254, 251));    // white chest under the chin
    alpha(bib, 225);
  } else {
    int p = ell(-37, -43, 15, 26, 0, ORG);
    clipTo(p, body);
    int q = ell(38, -49, 18, 30, 0, ORG);
    clipTo(q, body);
    for (int k = 0; k < 3; k++) {
      int s = cap(27, -62 + k * 11, 36, -65 + k * 11, 1.5f, STRIPE);
      clipTo(s, body);
      alpha(s, 190);
    }
    int chest = ell(0, -68, 21, 30, 0, rgb(255, 254, 251));
    alpha(chest, 210);
    if (L.dirty) {
      int d = ell(-10, -40, 2.4f, 1.8f, 0, rgb(140, 118, 96));
      alpha(d, 150);
      int e = ell(14, -60, 2, 1.6f, 0.5f, rgb(140, 118, 96));
      alpha(e, 150);
    }
  }

  // tail wraps along the floor, in front of the body but behind the paws
  if (!L.inBox) tailCurve(tp, 16, 5, 13, 7.5f, 5.5f, L.tailPuff, 4);

  // front legs and paws
  if (!loaf && !L.inBox) {
    for (int side = -1; side <= 1; side += 2) {
      float lift = (side < 0 ? L.pawL : L.pawR) * 9;
      bool grooming = side > 0 && L.groom > 0;
      float fx = side * 12, fy = -9 - lift;
      if (grooming) {                          // paw up to the mouth
        fx = side * 12 + (8 - side * 12) * L.groom;
        fy = -9 + (-93 + 9) * L.groom;
      }
      int leg = cap(side * 11, -56, fx, fy, 8.2f, FUR);
      out(leg, grooming ? LINE : SOFT, grooming ? 1.2f : 1.1f);
      int paw = ell(fx + side * 1.5f, fy + 4, 10.5f, 6.5f, grooming ? side * 1.2f : 0, FUR);
      out(paw, grooming ? LINE : SOFT, 1.1f);
      for (int k = -1; k <= 1; k += 2) cap(fx + side * 1.5f + k * 3, fy + 6, fx + side * 1.5f + k * 3.5f, fy + 9.5f, 0.45f, SOFT);
    }
  } else if (loaf && !L.inBox) {               // only the tips of the tucked paws show
    for (int side = -1; side <= 1; side += 2) {
      int paw = ell(side * 13, -3, 10, 5, 0, FUR);
      out(paw, SOFT, 1.1f);
    }
  }
  if (!L.inBox) tailCurve(tp, 16, 13, 16, 7.5f, 5.5f, L.tailPuff, 5);   // the tip curls over a paw
  accessoryNeck(L.acc, loaf ? -48 : -92);

  // cardboard box in front, paws on the rim
  if (L.inBox) {
    uint16_t cb = rgb(200, 150, 96), cbd = rgb(160, 112, 66);
    int f = quad(-52, -46, 52, -46, 50, 4, -50, 4, cb);
    grad(f, cbd);
    out(f, rgb(110, 76, 44), 1.3f);
    int fl = quad(-52, -46, -30, -46, -40, -60, -62, -58, rgb(214, 166, 112));
    out(fl, rgb(110, 76, 44), 1.2f);
    int fr = quad(30, -46, 52, -46, 62, -58, 40, -60, rgb(214, 166, 112));
    out(fr, rgb(110, 76, 44), 1.2f);
    quad(-6, -46, 6, -46, 6, 4, -6, 4, rgb(222, 196, 150));
    for (int side = -1; side <= 1; side += 2) {
      int paw = ell(side * 14, -48, 10, 6, 0, FUR);
      out(paw, LINE, 1.1f);
    }
  }

  // head, tilted about the neck
  Xf base = X;
  X = X.shift(0, headDrop).rotAbout(0, -90, L.tilt);
  headFront(L);
  headHits(L.s);
  X = base;
  X.apply(0, loaf ? -27 : -52, hit.bx, hit.by);
  hit.brx = (loaf ? 50 : 36) * L.s;
  hit.bry = (loaf ? 28 : 48) * L.s;
  tailPoints(tp);
}

// ---------- FLOP: flat on her side, head up, legs out (her favourite) ----------
static void flop(const Look &L) {
  float shake = sinf(L.t * 70) * L.purr * 0.6f;
  X.set(L.x + shake, L.y, L.s, L.flip);
  float br = sinf(L.breath);
  int sh = ell(0, -1, 70, 7, 0, BLACK);
  alpha(sh, 45);

  // tail lies on the floor behind her; the tip flicks
  float flick = sinf(L.t * 2.1f) * 5 * L.tailSwing;
  float tp[8] = {-50, -24, -80, -36, -108, -24, -100, -7 - fabsf(flick)};
  tailCurve(tp, 14, 0, 14, 7, 5, L.tailPuff, 3);

  // far legs
  int fh = cap(-34, -12, -54, -7, 7, FUR_FAR);
  out(fh, SOFT, 1);
  ell(-59, -6, 8, 5, 0, FUR_FAR);
  int ff = cap(34, -13, 60, -7, 6.5f, FUR_FAR);
  out(ff, SOFT, 1);
  ell(64, -6, 8, 5, 0, FUR_FAR);

  // body: belly towards you
  int body = ell(-2, -22 - br * 0.6f, 53, 22 + br * 0.8f, 0, FUR);
  grad(body, FUR_SH);
  out(body, LINE, 1.2f);
  group(body, 1);
  int haunch = ell(-35, -23, 25, 22, 0, FUR);
  grad(haunch, FUR_SH);
  out(haunch, LINE, 1.2f);
  group(haunch, 1);
  int chest = ell(30, -23, 22, 21, 0, FUR);
  out(chest, LINE, 1.2f);
  group(chest, 1);
  int saddle = ell(-8, -39, 52, 14, 0.04f, ORG);
  grad(saddle, ORG_L);
  clipTo(saddle, body);
  stripes(-40, -45, 12, 6, 12, 3, body, 1.7f);
  int hip = ell(-44, -32, 17, 15, 0, ORG);
  clipTo(hip, haunch);
  stripes(-54, -40, 8, 3, 11, 2, haunch);
  if (L.dirty) {
    int d = ell(4, -18, 2.4f, 1.8f, 0, rgb(140, 118, 96));
    alpha(d, 150);
  }

  // near hind leg, with the orange patch and the sole of the paw
  int nh = cap(-38, -9, -64, -4, 8, FUR);
  out(nh, SOFT, 1.1f);
  int hock = ell(-51, -7, 10, 6, -0.2f, ORG);
  clipTo(hock, nh);
  int hp = ell(-70, -4, 8, 6.5f, 0, FUR);
  out(hp, SOFT, 1);
  beans(-70.5f, -4.5f, 1);
  // near front leg
  int nf = cap(26, -9, 52, -3, 7.2f, FUR);
  out(nf, SOFT, 1.1f);
  int fp = ell(57, -3, 9, 5.5f, 0, FUR);
  out(fp, SOFT, 1);
  for (int k = -1; k <= 1; k += 2) cap(58 + k * 3, -2, 59 + k * 3.5f, 1.5f, 0.45f, SOFT);

  // head, raised and turned to you
  Xf base = X;
  X = X.shift(40, 76).rotAbout(0, -100, -0.14f + L.tilt);
  headFront(L);
  headHits(L.s);
  X = base;
  X.apply(-4, -22, hit.bx, hit.by);
  hit.brx = 54 * L.s;
  hit.bry = 22 * L.s;
  tailPoints(tp);
}

// ---------- Side poses: WALK, RUN, CROUCH, POUNCE, BAT, EAT ----------
static void side(const Look &L) {
  X.set(L.x, L.y, L.s, L.flip);
  float c = L.crouch, w = L.wiggle, st = L.stretch;
  bool run = L.pose == RUN;
  float stride = run ? 17 : 10, lift = run ? 9 : 5, p = L.phase;
  bool moving = L.pose == WALK || run;
  float by = -42 + 11 * c + (moving ? fabsf(sinf(p)) * (run ? 3 : 1.5f) : 0);

  int sh = ell(0, -1, 52 - st * 10, 6, 0, BLACK);
  alpha(sh, st > 0 ? 25 : 45);

  // feet: near/far, front/back
  auto foot = [&](float base, float ph, float &fx, float &fy) {
    fx = base + (moving ? sinf(p + ph) * stride : 0);
    fy = moving ? -fmaxf(0, cosf(p + ph)) * lift : 0;
  };
  float ffx, ffy, fbx, fby, nfx, nfy, nbx, nby;
  foot(22, 3.1416f, ffx, ffy);
  foot(-24, 0, fbx, fby);
  foot(28, 0, nfx, nfy);
  foot(-22, 3.1416f, nbx, nby);
  if (c > 0) {
    ffx += 6 * c; nfx += 8 * c; fbx -= 4 * c; nbx -= 4 * c;
  }
  if (st > 0) {                               // pounce: stretched out in the air
    ffx = 46; ffy = -26 * st; nfx = 52; nfy = -20 * st;
    fbx = -46; fby = -12 * st; nbx = -42; nby = -8 * st;
  }
  if (L.bat > 0) {                            // near front paw reaches up and forward
    nfx += (44 - nfx) * L.bat;
    nfy += (-62 - nfy) * L.bat;
  }
  float rearX = -24 + w;

  // far legs
  int a = cap(24, by + 4, ffx, ffy - 4, 6.4f, FUR_FAR);
  out(a, SOFT, 1);
  ell(ffx + 2, ffy - 2, 8, 4.5f, 0, FUR_FAR);
  int b = cap(rearX - 2, by + 4, fbx, fby - 4, 6.8f, FUR_FAR);
  out(b, SOFT, 1);
  ell(fbx + 2, fby - 2, 8, 4.5f, 0, FUR_FAR);

  // tail from the rear
  float ta = run ? -2.75f : (c > 0 ? -2.95f : -2.2f) - st * 0.6f;
  float tp[8];
  float tx = rearX - 20, ty = by - 2;
  float sway = sinf(L.t * (run ? 9 : 3)) * 0.25f * L.tailSwing;
  tp[0] = tx; tp[1] = ty;
  tp[2] = tx + cosf(ta) * 24; tp[3] = ty + sinf(ta) * 24;
  tp[4] = tp[2] + cosf(ta + 0.5f + sway) * 22; tp[5] = tp[3] + sinf(ta + 0.5f + sway) * 22;
  tp[6] = tp[4] + cosf(ta + 1.0f + sway * 2) * 20; tp[7] = tp[5] + sinf(ta + 1.0f + sway * 2) * 20;
  if (c > 0) tp[7] += sinf(L.t * 14) * 4;     // twitching tip when stalking
  tailCurve(tp, 12, 0, 12, 6.4f, 4.6f, L.tailPuff, 3);

  // body
  float tilt = st * -0.12f;
  int body = ell(0, by, 46 + st * 8, 22 - 3 * c - st * 3, tilt, FUR);
  grad(body, FUR_SH);
  out(body, LINE, 1.2f);
  group(body, 1);
  int chest = ell(28, by - 3, 19, 19, 0, FUR);
  out(chest, LINE, 1.2f);
  group(chest, 1);
  int haunch = ell(rearX - 4, by + 4, 18, 20 - 3 * c, 0, FUR);
  grad(haunch, FUR_SH);
  out(haunch, LINE, 1.2f);
  group(haunch, 1);
  int saddle = ell(-8, by - 17, 41, 14, tilt, ORG);
  grad(saddle, ORG_L);
  clipTo(saddle, body);
  stripes(-34, by - 24, 11, 6, 11, 3, body);
  int hip = ell(rearX - 8, by - 4, 14, 16, 0, ORG);
  clipTo(hip, haunch);

  // near legs
  int l1 = cap(rearX, by + 6, nbx, nby - 4, 7.6f, FUR);
  out(l1, SOFT, 1.1f);
  int hock = ell((rearX + nbx) * 0.5f - 1, (by + 6 + nby - 4) * 0.5f, 7, 8, 0, ORG);
  clipTo(hock, l1);
  int p1 = ell(nbx + 2, nby - 2, 9, 5, 0, FUR);
  out(p1, SOFT, 1);
  int l2 = cap(27, by + 4, nfx, nfy - 4, 6.9f, FUR);
  out(l2, L.bat > 0 ? LINE : SOFT, 1.1f);
  int p2 = ell(nfx + 2, nfy - 2, 9, 5, L.bat * -0.6f, FUR);
  out(p2, L.bat > 0 ? LINE : SOFT, 1);
  X.apply(nfx + 4, nfy - 2, hit.pawX, hit.pawY);

  // head
  float hx = 50, hy = by - 22 - c * 4;
  if (L.headDown > 0) {
    hx += 10 * L.headDown;
    hy += 34 * L.headDown + sinf(L.t * 14) * 1.5f * L.headDown;
  }
  if (st > 0) { hx += 8; hy += 4; }
  Xf base = X;
  X = X.shift(hx, hy).rotAbout(0, 0, L.tilt + L.headDown * 0.45f);
  if (L.acc == A_BELL) {                       // collar round the neck, under the head
    int col = cap(-12, 13, 4, 19, 3.2f, rgb(56, 110, 210));
    out(col, rgb(28, 50, 110), 1);
    int bell = ell(2, 25, 4.5f, 4.5f, 0, rgb(250, 200, 60));
    out(bell, rgb(150, 105, 20), 1);
  } else if (L.acc == A_BANDANA) {
    int bd = tri(-14, 12, 8, 19, -6, 36, rgb(214, 52, 58));
    out(bd, rgb(120, 30, 34), 1.1f);
  }
  {
    int far = tri(-13, -11, -3, -18, -11, -38, ORG_D);
    out(far, LINE, 1.2f);
    group(far, 2);
    Xf save = X;
    X = X.rotAbout(8, -16, L.earBack * 0.8f + L.twitchR * 0.25f);
    int nearE = tri(1, -17, 15, -13, 9, -40, ORG);
    grad(nearE, ORG_D);
    out(nearE, LINE, 1.2f);
    group(nearE, 2);
    int in = tri(5, -18, 12, -15, 9, -34, PINK);
    alpha(in, 230);
    X = save;
    int head = ell(0, 0, 23, 20, 0, FUR);
    out(head, LINE, 1.2f);
    group(head, 2);
    int muz = ell(15, 6, 12, 9, 0, FUR);
    out(muz, LINE, 1.2f);
    group(muz, 2);
    int cap1 = ell(-8, -11, 21, 14, 0.3f, ORG);
    grad(cap1, ORG_L);
    clipTo(cap1, head);
    int ep = ell(9, -5, 11, 8.5f, 0, ORG);
    clipTo(ep, head);
    Look e = L;
    e.lookX = 0.6f + L.lookX * 0.4f;
    eye(e, 11, -4, 6.0f, 6.0f, -0.15f, L.pupil, 0.5f);
    eye(e, 23, -5, 2.8f, 5.0f, 0, L.pupil * 0.7f, 0.2f);
    int nose = tri(23.5f, 3, 28.5f, 3, 27, 7.5f, NOSE);
    out(nose, NOSE_D, 0.7f);
    if (L.mouth == M_MEOW || L.mouth == M_HISS || L.mouth == M_YAWN) {
      int m = ell(22, 12, 5, L.mouth == M_YAWN ? 5 : 3.5f, 0.3f, rgb(120, 40, 50));
      out(m, LINE, 1);
    } else {
      cap(26, 8.5f, 21, 11, 0.7f, MOUTH);
    }
    if (L.headDown > 0.5f && sinf(L.t * 14) > 0) ell(27, 12, 2.2f, 1.6f, 0, PINK);   // eating
    whiskers(22, 7, 1, 26, 3);
    if (L.acc == A_HAT) {
      int h = tri(-12, -17, 12, -21, -6, -56, rgb(80, 170, 240));
      out(h, rgb(30, 70, 120), 1.1f);
      ell(-6, -57, 4, 4, 0, rgb(255, 90, 120));
    } else if (L.acc == A_CROWN) {
      int cr = quad(-14, -17, 12, -21, 12, -30, -14, -26, rgb(250, 200, 60));
      out(cr, rgb(160, 110, 20), 1.1f);
    } else if (L.acc == A_BOW) {
      int bw = ell(-6, -18, 6, 4, 0.4f, rgb(240, 88, 150));
      out(bw, rgb(150, 40, 90), 1);
    }
  }
  X.apply(4, -4, hit.hx, hit.hy);
  hit.hr = 28 * L.s;
  X.apply(27, 5, hit.nx, hit.ny);
  X.apply(11, -4, hit.ex[0], hit.ey[0]);
  hit.ex[1] = hit.ex[0];
  hit.ey[1] = hit.ey[0];
  X.apply(14, 14, hit.cx, hit.cy);
  float tx2;
  X.apply(0, -44, tx2, hit.top);
  X = base;
  X.apply(0, by, hit.bx, hit.by);
  hit.brx = 48 * L.s;
  hit.bry = 22 * L.s;
  tailPoints(tp);
}

// ---------- Curled up asleep ----------
static void curl(const Look &L) {
  X.set(L.x, L.y, L.s, L.flip);
  float br = sinf(L.breath);
  int sh = ell(0, -1, 52, 7, 0, BLACK);
  alpha(sh, 45);
  int body = ell(4, -24 - br, 46, 24 + br * 1.3f, 0, FUR);
  grad(body, FUR_SH);
  out(body, LINE, 1.2f);
  group(body, 1);
  int p = ell(10, -44, 46, 17, 0, ORG);
  grad(p, ORG_L);
  clipTo(p, body);
  stripes(-18, -48, 11, 6, 12, 3, body);
  int q = ell(38, -28, 14, 18, 0, ORG);
  clipTo(q, body);
  float tp[8] = {44, -10, 54, 6, 0, 9, -44 + sinf(L.t * 0.7f) * 3, -3};
  tailCurve(tp, 14, 0, 14, 7, 5, 0, 3);
  // head resting on the paws at the left
  Xf base = X;
  X = X.shift(-30, -24).rotAbout(0, 0, -0.22f);
  int pw = ell(-2, 17, 11, 6, 0, FUR);
  out(pw, LINE, 1.1f);
  int e1 = tri(-17, -9, -6, -16, -21, -32, ORG);
  out(e1, LINE, 1.2f);
  group(e1, 2);
  int e2 = tri(3, -17, 14, -11, 11, -33, ORG);
  out(e2, LINE, 1.2f);
  group(e2, 2);
  tri(-14, -12, -8, -15, -18, -27, PINK);
  int head = ell(0, 0, 22, 18, 0, FUR);
  out(head, LINE, 1.2f);
  group(head, 2);
  for (int s2 = -1; s2 <= 1; s2 += 2) {
    int pc = ell(s2 * 14, -8, 15, 13, s2 * -0.3f, ORG);
    clipTo(pc, head);
  }
  int bl = quad(-2, -18, 2, -18, 6, 6, -6, 6, FUR);
  clipTo(bl, head);
  closedEye(-9, -1, 6, false);
  closedEye(9, -1, 6, false);
  tri(-3.5f, 7, 3.5f, 7, 0, 10.5f, NOSE);
  whiskers(-6, 10, -1, 20, 2);
  if (L.acc == A_BELL || L.acc == A_BANDANA) {
    int col = cap(10, 14, 24, 8, 3, L.acc == A_BELL ? rgb(56, 110, 210) : rgb(214, 52, 58));
    out(col, LINE, 1);
  }
  X.apply(0, 0, hit.hx, hit.hy);
  hit.hr = 24 * L.s;
  hit.nx = hit.hx;
  hit.ny = hit.hy;
  float tmp;
  X.apply(0, -34, tmp, hit.top);
  X = base;
  X.apply(4, -24, hit.bx, hit.by);
  hit.brx = 50 * L.s;
  hit.bry = 26 * L.s;
  hit.cx = hit.hx;
  hit.cy = hit.hy;
  hit.ex[0] = hit.ex[1] = hit.hx;
  hit.ey[0] = hit.ey[1] = hit.hy;
  tailPoints(tp);
}

static void draw(uint16_t *fb, const Look &L) {
  cg::begin();
  switch (L.pose) {
    case WALK: case RUN: case CROUCH: case POUNCE: case BAT: case EAT: side(L); break;
    case CURL: curl(L); break;
    case FLOP: flop(L); break;
    default: front(L);
  }
  cg::render(fb);
}

// Just the head, e.g. peeking in from the edge of the home screen, or as the app icon.
static void drawHead(uint16_t *fb, float x, float y, float s, float blink, float lookX) {
  Look L;
  L.eyeOpen = blink;
  L.lookX = lookX;
  L.pupil = 0.45f;
  cg::begin();
  X.set(x, y + 120 * s, s, false);           // (x, y) = centre of the head
  headFront(L, s > 0.35f);
  cg::render(fb);
}

}  // namespace kitty
