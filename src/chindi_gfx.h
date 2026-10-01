// chindi_gfx.h
// Smooth vector shapes for Chindi: ellipses, capsules and convex polygons with soft
// (anti-aliased) edges, outlines, vertical gradients, transparency and clipping.
// Shapes are collected in a list, then drawn in order. Each pixel row is drawn as a span
// found from the shape's equation, sampled at two heights for smooth top and bottom edges.
#pragma once

#include "core.h"
#include "twocore.h"

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

// A shape made ready to be cut into rows. All that stays the same from row to row is worked
// out here, once, so that a row needs no division.
struct Prep {
  const Shape *s;
  float g;                              // the shape is grown by this much (its outline)
  float ea4, eb, ec, ei;                // ellipse: terms of the row equation
  float rad, rad2, dx, dy, L2, radL, idx, idy;   // capsule
  float m[4];                           // polygon: x step per row of each edge
};

static void prep(Prep &p, const Shape &s, float g) {
  p.s = &s;
  p.g = g;
  switch (s.type) {
    case SH_ELLIPSE: {
      float rx = s.a[2] + g, ry = s.a[3] + g, c = s.a[4], sn = s.a[5];
      float iA = 1 / (rx * rx), iB = 1 / (ry * ry);
      float qa = c * c * iA + sn * sn * iB;
      p.ea4 = 4 * qa;
      p.eb = 2 * c * sn * (iA - iB);
      p.ec = sn * sn * iA + c * c * iB;
      p.ei = 1 / (2 * qa);
      break;
    }
    case SH_CAPSULE:
      p.rad = s.a[4] + g;
      p.rad2 = p.rad * p.rad;
      p.dx = s.a[2] - s.a[0];
      p.dy = s.a[3] - s.a[1];
      p.L2 = p.dx * p.dx + p.dy * p.dy;
      p.radL = p.rad * sqrtf(p.L2);
      p.idx = fabsf(p.dx) > 1e-5f ? 1 / p.dx : 0;
      p.idy = fabsf(p.dy) > 1e-5f ? 1 / p.dy : 0;
      break;
    default:
      for (int i = 0; i < s.npts; i++) {
        int j = i + 1 == s.npts ? 0 : i + 1;
        float h = s.a[2 * j + 1] - s.a[2 * i + 1];
        p.m[i] = h != 0 ? (s.a[2 * j] - s.a[2 * i]) / h : 0;
      }
  }
}

// x extent of the shape on screen row y. false if the row misses it.
static bool span(const Prep &p, float y, float &l, float &r) {
  const Shape &s = *p.s;
  switch (s.type) {
    case SH_ELLIPSE: {
      float dy = y - s.a[1];
      float qb = dy * p.eb;
      float disc = qb * qb - p.ea4 * (dy * dy * p.ec - 1);
      if (disc < 0) return false;
      float q = sqrtf(disc);
      l = s.a[0] + (-qb - q) * p.ei;
      r = s.a[0] + (-qb + q) * p.ei;
      return true;
    }
    case SH_CAPSULE: {
      bool any = false;
      l = 1e9f;
      r = -1e9f;
      for (int e = 0; e < 2; e++) {           // end circles
        float d = y - s.a[2 * e + 1];
        if (fabsf(d) <= p.rad) {
          float h = sqrtf(p.rad2 - d * d);
          widen(l, r, s.a[2 * e] - h, s.a[2 * e] + h);
          any = true;
        }
      }
      if (p.L2 > 1e-4f) {                     // the straight part
        float ax = s.a[0], lo = -1e9f, hi = 1e9f, yy = y - s.a[1];
        // 0 <= ((x-ax)dx + yy dy) / L2 <= 1
        if (p.idx != 0) {
          float x0 = ax - yy * p.dy * p.idx, x1 = ax + (p.L2 - yy * p.dy) * p.idx;
          lo = fmaxf(lo, fminf(x0, x1));
          hi = fminf(hi, fmaxf(x0, x1));
        } else {
          float u = yy * p.dy;
          if (u < 0 || u > p.L2) hi = lo - 1;
        }
        // |(x-ax)dy - yy dx| <= rad L
        if (p.idy != 0) {
          float x0 = ax + (yy * p.dx - p.radL) * p.idy, x1 = ax + (yy * p.dx + p.radL) * p.idy;
          lo = fmaxf(lo, fminf(x0, x1));
          hi = fminf(hi, fmaxf(x0, x1));
        } else if (fabsf(yy * p.dx) > p.radL) {
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
      float g = p.g;
      l = 1e9f;
      r = -1e9f;
      // grow: sample the rows g above and below too, and widen by g (good enough for thin lines)
      for (int k = (g > 0 ? -1 : 0); k <= (g > 0 ? 1 : 0); k++) {
        float yy = y + k * g;
        for (int i = 0; i < s.npts; i++) {
          int j = i + 1 == s.npts ? 0 : i + 1;
          float y0 = s.a[2 * i + 1], y1 = s.a[2 * j + 1];
          if ((yy < y0) == (yy < y1)) continue;
          float x = s.a[2 * i] + (yy - y0) * p.m[i];
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

#ifdef SIM_COUNT
static long simPixels = 0, simRows = 0;
static int simPeak = 0;
#endif

// draw the rows yLo .. yHi - 1 of a shape (or of its outline)
static void raster(uint16_t *fb, const Shape &s, bool outline, int yLo, int yHi) {
  if (s.alpha == 0) return;                   // invisible: only used to clip others
  float g = outline ? s.ow : 0;
  const bool cl = s.clip >= 0;
  Prep ps, pc;
  prep(ps, s, g);
  if (cl) prep(pc, SH[s.clip], 0);
  const bool fade = !outline && s.col2 != s.col && s.gy1 > s.gy0;
  const float fadeK = fade ? 256 / (s.gy1 - s.gy0) : 0;
  int y0 = max(yLo, (int)floorf(topOf(s, g))), y1 = min(yHi - 1, (int)ceilf(bottomOf(s, g)));
  for (int py = y0; py <= y1; py++) {
    float l[2], r[2];
    bool ok[2];
    for (int k = 0; k < 2; k++) {
      float ys = py + 0.25f + 0.5f * k;
      ok[k] = span(ps, ys, l[k], r[k]);
      if (ok[k] && cl) {
        float cl0, cr0;
        if (!span(pc, ys, cl0, cr0)) ok[k] = false;
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
    if (fade) col = blend(s.col, s.col2, (int)constrain((py - s.gy0) * fadeK, 0, 256));
    uint16_t *row = fb + py * W;
#ifdef SIM_COUNT
    simPixels += x1 - x0 + 1;
#endif
    // the row is: soft edge pixels, a solid run, soft edge pixels.
    // How much of an edge pixel the two spans cover is worked out in whole numbers (1/256 pixel).
    int fl[2] = {0, 0}, fr[2] = {0, 0};
    for (int k = 0; k < 2; k++)
      if (ok[k]) { fl[k] = (int)(l[k] * 256); fr[k] = (int)(r[k] * 256); }
    auto soft = [&](int a0, int a1) {
      for (int px = a0; px <= a1; px++) {
        int p0 = px << 8, p1 = p0 + 256;
        int c = constrain(min(fr[0], p1) - max(fl[0], p0), 0, 256) + constrain(min(fr[1], p1) - max(fl[1], p0), 0, 256);
        int a = c * s.alpha >> 9;
        if (a > 0) row[px] = a >= 255 ? col : blend(row[px], col, a + 1);
      }
    };
    if (in0 > in1) {
      soft(x0, x1);
      continue;
    }
    soft(x0, min(in0 - 1, x1));
    int i0 = max(in0, x0), i1 = min(in1, x1);
    if (s.alpha >= 255) {
      for (int px = i0; px <= i1; px++) row[px] = col;
    } else {
      for (int px = i0; px <= i1; px++) row[px] = blend(row[px], col, s.alpha + 1);
    }
    soft(max(in1 + 1, x0), x1);
  }
}

// Draw the rows yLo .. yHi - 1 of all shapes. Within a group, the outlines of all its shapes
// are drawn first, so the group reads as one outlined silhouette. Other outlined shapes get
// their outline right before their fill.
static void renderRows(uint16_t *fb, int yLo, int yHi) {
  uint32_t done = 0;
  for (int i = 0; i < nsh; i++) {
    const Shape &s = SH[i];
    if (s.group) {
      uint32_t bit = 1u << (s.group & 31);
      if (!(done & bit)) {
        done |= bit;
        for (int j = i; j < nsh; j++)
          if (SH[j].group == s.group && (SH[j].flags & F_OUTLINE)) raster(fb, SH[j], true, yLo, yHi);
      }
    } else if (s.flags & F_OUTLINE) {
      raster(fb, s, true, yLo, yHi);
    }
    raster(fb, s, false, yLo, yHi);
  }
}

// Draw all shapes. The two processor cores share the work: one draws the rows above
// splitRow, the other the rows from splitRow down. Each row belongs to one core only, and a
// core draws its rows of all shapes in the list's order, so the picture is the same as from
// one core. splitNudge moves the split by itself to where the two parts take the same time.
static int splitRow = H;
static float splitNudge = 0;
static void renderHalf(void *fb, int part) {
  if (part == 0) renderRows((uint16_t *)fb, 0, splitRow);
  else renderRows((uint16_t *)fb, splitRow, H);
}

static void render(uint16_t *fb) {
#ifdef SIM_COUNT
  if (nsh > simPeak) simPeak = nsh;
#endif
  // put the split where the work is: at the mean height of the shapes, each one counted by
  // its rows (twice with an outline)
  float rows = 0, at = 0;
  for (int i = 0; i < nsh; i++) {
    const Shape &s = SH[i];
    if (s.alpha == 0) continue;
    float top = fmaxf(topOf(s, 0), 0), bottom = fminf(bottomOf(s, 0), H);
    if (bottom <= top) continue;
    float w = (bottom - top) * ((s.flags & F_OUTLINE) ? 2 : 1);
    rows += w;
    at += w * (top + bottom) * 0.5f;
  }
  if (rows < 60) {                            // too little to share: the hand-over costs more
    renderRows(fb, 0, H);
  } else {
    splitRow = constrain((int)(at / rows + splitNudge), 1, H - 1);
    splitNudge = evenShare(splitNudge, onBothCores(renderHalf, fb), -60, 60);
  }
  nsh = 0;
}

}  // namespace cg
