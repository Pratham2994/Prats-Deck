// chindi_gfx.h
// Smooth vector shapes for Chindi: ellipses, capsules and convex polygons with soft
// (anti-aliased) edges, outlines, vertical gradients, transparency and clipping.
// Shapes are collected in a list, then drawn in order. Each pixel row is drawn as a span
// found from the shape's equation, sampled at two heights for smooth top and bottom edges.
#pragma once

#include "core.h"

namespace cg {

enum : uint8_t { SH_ELLIPSE, SH_CAPSULE, SH_POLY };
enum : uint8_t { F_OUTLINE = 1 };

struct Shape {
  uint8_t type, group, npts, flags;
  float a[8];                // ellipse: cx cy rx ry cos sin | capsule: ax ay bx by r | poly: x0 y0 .. x3 y3
  uint16_t col, col2, ocol;  // fill, gradient bottom colour, outline colour
  float gy0, gy1;            // gradient y range (screen)
  float ow;                  // outline width, px
  uint8_t alpha;
  int16_t clip;              // shape whose fill clips this one, or -1
};

static const int MAXSH = 160;     // a busy frame uses about 100
static Shape SH[MAXSH];
static int nsh = 0;

// ---------- Transform: local cat units -> screen ----------
struct Xf {
  float m[6];                // x' = m0 x + m1 y + m2, y' = m3 x + m4 y + m5
  float ang, s, fl;          // screen rotation, scale, 1 or -1 (mirrored)
  void set(float ox, float oy, float scale, bool flip, float rot = 0) {
    float c = cosf(rot), sn = sinf(rot), f = flip ? -1.0f : 1.0f;
    m[0] = c * scale * f; m[1] = -sn * scale; m[2] = ox;
    m[3] = sn * scale * f; m[4] = c * scale;  m[5] = oy;
    ang = rot; s = scale; fl = f;
  }
  void apply(float x, float y, float &X, float &Y) const {
    X = m[0] * x + m[1] * y + m[2];
    Y = m[3] * x + m[4] * y + m[5];
  }
  // same transform, then rotated by a about local point (px, py)
  Xf rotAbout(float px, float py, float a) const {
    float c = cosf(a), sn = sinf(a);
    // local map: p -> pivot + R(a)(p - pivot)
    float l[6] = {c, -sn, px - c * px + sn * py, sn, c, py - sn * px - c * py};
    Xf r = *this;
    r.m[0] = m[0] * l[0] + m[1] * l[3];
    r.m[1] = m[0] * l[1] + m[1] * l[4];
    r.m[2] = m[0] * l[2] + m[1] * l[5] + m[2];
    r.m[3] = m[3] * l[0] + m[4] * l[3];
    r.m[4] = m[3] * l[1] + m[4] * l[4];
    r.m[5] = m[3] * l[2] + m[4] * l[5] + m[5];
    r.ang = ang + fl * a;
    return r;
  }
  Xf shift(float dx, float dy) const {
    Xf r = *this;
    r.m[2] += m[0] * dx + m[1] * dy;
    r.m[5] += m[3] * dx + m[4] * dy;
    return r;
  }
};
static Xf X;

static void begin() { nsh = 0; }

static Shape &add(uint8_t type, uint16_t col) {
  static Shape dummy;
  if (nsh >= MAXSH) return dummy;
  Shape &s = SH[nsh++];
  memset(&s, 0, sizeof(s));
  s.type = type;
  s.col = s.col2 = col;
  s.alpha = 255;
  s.clip = -1;
  return s;
}
static int last() { return nsh - 1; }

// ellipse at local (x, y), radii, local angle
static int ell(float x, float y, float rx, float ry, float ang, uint16_t col) {
  Shape &s = add(SH_ELLIPSE, col);
  X.apply(x, y, s.a[0], s.a[1]);
  s.a[2] = rx * X.s;
  s.a[3] = ry * X.s;
  float a = X.ang + X.fl * ang;
  s.a[4] = cosf(a);
  s.a[5] = sinf(a);
  float e = fmaxf(s.a[2], s.a[3]);
  s.gy0 = s.a[1] - e;
  s.gy1 = s.a[1] + e;
  return last();
}

static int cap(float x0, float y0, float x1, float y1, float r, uint16_t col) {
  Shape &s = add(SH_CAPSULE, col);
  X.apply(x0, y0, s.a[0], s.a[1]);
  X.apply(x1, y1, s.a[2], s.a[3]);
  s.a[4] = r * X.s;
  s.gy0 = fminf(s.a[1], s.a[3]) - s.a[4];
  s.gy1 = fmaxf(s.a[1], s.a[3]) + s.a[4];
  return last();
}

// convex polygon, 3 or 4 points
static int poly(int n, const float *pts, uint16_t col) {
  Shape &s = add(SH_POLY, col);
  s.npts = n;
  s.gy0 = 1e9f;
  s.gy1 = -1e9f;
  for (int i = 0; i < n; i++) {
    X.apply(pts[2 * i], pts[2 * i + 1], s.a[2 * i], s.a[2 * i + 1]);
    s.gy0 = fminf(s.gy0, s.a[2 * i + 1]);
    s.gy1 = fmaxf(s.gy1, s.a[2 * i + 1]);
  }
  return last();
}
static int tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t col) {
  float p[6] = {x0, y0, x1, y1, x2, y2};
  return poly(3, p, col);
}
static int quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint16_t col) {
  float p[8] = {x0, y0, x1, y1, x2, y2, x3, y3};
  return poly(4, p, col);
}

// modifiers
static void out(int i, uint16_t c, float w = 1.3f) { if (i >= 0 && i < MAXSH) { SH[i].flags |= F_OUTLINE; SH[i].ocol = c; SH[i].ow = w * fmaxf(X.s, 0.6f); } }
static void grad(int i, uint16_t c2) { if (i >= 0 && i < MAXSH) SH[i].col2 = c2; }
static void alpha(int i, int a) { if (i >= 0 && i < MAXSH) SH[i].alpha = (uint8_t)constrain(a, 0, 255); }
static void clipTo(int i, int j) { if (i >= 0 && i < MAXSH) SH[i].clip = (int16_t)j; }
static void group(int i, int g) { if (i >= 0 && i < MAXSH) SH[i].group = (uint8_t)g; }

// ---------- Spans ----------
static inline void widen(float &l, float &r, float nl, float nr) {
  if (nl < l) l = nl;
  if (nr > r) r = nr;
}

// x extent of shape s on screen row y, grown by g. false if the row misses it.
static bool span(const Shape &s, float y, float g, float &l, float &r) {
  switch (s.type) {
    case SH_ELLIPSE: {
      float rx = s.a[2] + g, ry = s.a[3] + g, c = s.a[4], sn = s.a[5];
      float iA = 1 / (rx * rx), iB = 1 / (ry * ry), dy = y - s.a[1];
      float qa = c * c * iA + sn * sn * iB;
      float qb = 2 * dy * c * sn * (iA - iB);
      float qc = dy * dy * (sn * sn * iA + c * c * iB) - 1;
      float disc = qb * qb - 4 * qa * qc;
      if (disc < 0) return false;
      float q = sqrtf(disc);
      l = s.a[0] + (-qb - q) / (2 * qa);
      r = s.a[0] + (-qb + q) / (2 * qa);
      return true;
    }
    case SH_CAPSULE: {
      float ax = s.a[0], ay = s.a[1], bx = s.a[2], by = s.a[3], rad = s.a[4] + g;
      bool any = false;
      l = 1e9f;
      r = -1e9f;
      for (int e = 0; e < 2; e++) {           // end circles
        float cx = e ? bx : ax, cy = e ? by : ay, d = y - cy;
        if (fabsf(d) <= rad) {
          float h = sqrtf(rad * rad - d * d);
          widen(l, r, cx - h, cx + h);
          any = true;
        }
      }
      float dx = bx - ax, dyy = by - ay, L2 = dx * dx + dyy * dyy;
      if (L2 > 1e-4f) {                       // the straight part
        float L = sqrtf(L2), lo = -1e9f, hi = 1e9f, yy = y - ay;
        // 0 <= ((x-ax)dx + yy dyy) / L2 <= 1
        if (fabsf(dx) > 1e-5f) {
          float x0 = ax + (0 - yy * dyy) / dx, x1 = ax + (L2 - yy * dyy) / dx;
          lo = fmaxf(lo, fminf(x0, x1));
          hi = fminf(hi, fmaxf(x0, x1));
        } else {
          float t = yy * dyy / L2;
          if (t < 0 || t > 1) hi = lo - 1;
        }
        // |(x-ax)dyy - yy dx| <= rad L
        if (fabsf(dyy) > 1e-5f) {
          float x0 = ax + (yy * dx - rad * L) / dyy, x1 = ax + (yy * dx + rad * L) / dyy;
          lo = fmaxf(lo, fminf(x0, x1));
          hi = fminf(hi, fmaxf(x0, x1));
        } else if (fabsf(yy * dx) > rad * L) {
          hi = lo - 1;
        }
        if (hi >= lo) {
          widen(l, r, lo, hi);
          any = true;
        }
      }
      return any;
    }
    default: {                                // convex polygon
      bool any = false;
      l = 1e9f;
      r = -1e9f;
      // grow: sample the rows g above and below too, and widen by g (good enough for thin lines)
      for (int k = (g > 0 ? -1 : 0); k <= (g > 0 ? 1 : 0); k++) {
        float yy = y + k * g;
        for (int i = 0; i < s.npts; i++) {
          int j = (i + 1) % s.npts;
          float x0 = s.a[2 * i], y0 = s.a[2 * i + 1], x1 = s.a[2 * j], y1 = s.a[2 * j + 1];
          if ((yy < y0) == (yy < y1)) continue;
          float x = x0 + (yy - y0) * (x1 - x0) / (y1 - y0);
          widen(l, r, x - g, x + g);
          any = true;
        }
      }
      return any && r >= l;
    }
  }
}

static float topOf(const Shape &s, float g) {
  switch (s.type) {
    case SH_ELLIPSE: return s.a[1] - fmaxf(s.a[2], s.a[3]) - g;
    case SH_CAPSULE: return fminf(s.a[1], s.a[3]) - s.a[4] - g;
    default: return s.gy0 - g;
  }
}
static float bottomOf(const Shape &s, float g) {
  switch (s.type) {
    case SH_ELLIPSE: return s.a[1] + fmaxf(s.a[2], s.a[3]) + g;
    case SH_CAPSULE: return fmaxf(s.a[1], s.a[3]) + s.a[4] + g;
    default: return s.gy1 + g;
  }
}

static inline float cover(float l, float r, int px) {
  float c = fminf(r, px + 1.0f) - fmaxf(l, (float)px);
  return c < 0 ? 0 : (c > 1 ? 1 : c);
}

#ifdef SIM_COUNT
static long simPixels = 0, simRows = 0;
static int simPeak = 0;
#endif

static void raster(uint16_t *fb, const Shape &s, bool outline) {
  if (s.alpha == 0) return;                   // invisible: only used to clip others
  float g = outline ? s.ow : 0;
  const Shape *cl = s.clip >= 0 ? &SH[s.clip] : nullptr;
  int y0 = max(0, (int)floorf(topOf(s, g))), y1 = min(H - 1, (int)ceilf(bottomOf(s, g)));
  for (int py = y0; py <= y1; py++) {
    float l[2], r[2];
    bool ok[2];
    for (int k = 0; k < 2; k++) {
      float ys = py + 0.25f + 0.5f * k;
      ok[k] = span(s, ys, g, l[k], r[k]);
      if (ok[k] && cl) {
        float cl0, cr0;
        if (!span(*cl, ys, 0, cl0, cr0)) ok[k] = false;
        else {
          l[k] = fmaxf(l[k], cl0);
          r[k] = fminf(r[k], cr0);
          if (r[k] <= l[k]) ok[k] = false;
        }
      }
    }
    if (!ok[0] && !ok[1]) continue;
#ifdef SIM_COUNT
    simRows++;
#endif
    float lo = 1e9f, hi = -1e9f;
    for (int k = 0; k < 2; k++)
      if (ok[k]) widen(lo, hi, l[k], r[k]);
    int x0 = max(0, (int)floorf(lo)), x1 = min(W - 1, (int)ceilf(hi));
    int in0 = 1 << 20, in1 = -1;
    if (ok[0] && ok[1]) {
      in0 = (int)ceilf(fmaxf(l[0], l[1]));
      in1 = (int)floorf(fminf(r[0], r[1])) - 1;
    }
    uint16_t col = outline ? s.ocol : s.col;
    if (!outline && s.col2 != s.col && s.gy1 > s.gy0)
      col = blend(s.col, s.col2, (int)constrain((py - s.gy0) / (s.gy1 - s.gy0) * 256, 0, 256));
    uint16_t *row = fb + py * W;
#ifdef SIM_COUNT
    simPixels += x1 - x0 + 1;
#endif
    for (int px = x0; px <= x1; px++) {
      int a;
      if (px >= in0 && px <= in1) a = s.alpha;
      else {
        float c = ((ok[0] ? cover(l[0], r[0], px) : 0) + (ok[1] ? cover(l[1], r[1], px) : 0)) * 0.5f;
        a = (int)(c * s.alpha);
        if (a <= 0) continue;
      }
      row[px] = a >= 255 ? col : blend(row[px], col, a + 1);
    }
  }
}

// Draw all shapes. Within a group, the outlines of all its shapes are drawn first, so the
// group reads as one outlined silhouette. Other outlined shapes get their outline right
// before their fill.
static void render(uint16_t *fb) {
#ifdef SIM_COUNT
  if (nsh > simPeak) simPeak = nsh;
#endif
  uint32_t done = 0;
  for (int i = 0; i < nsh; i++) {
    const Shape &s = SH[i];
    if (s.group) {
      uint32_t bit = 1u << (s.group & 31);
      if (!(done & bit)) {
        done |= bit;
        for (int j = i; j < nsh; j++)
          if (SH[j].group == s.group && (SH[j].flags & F_OUTLINE)) raster(fb, SH[j], true);
      }
    } else if (s.flags & F_OUTLINE) {
      raster(fb, s, true);
    }
    raster(fb, s, false);
  }
  nsh = 0;
}

}  // namespace cg
