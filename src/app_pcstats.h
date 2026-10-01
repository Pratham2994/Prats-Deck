// app_pcstats.h
// Live PC stats from pc_monitor.py over USB: CPU, RAM, GPU gauges, history graph,
// temperatures and network. Shows the Pico's own stats while no PC data arrives.
// Tap the graph to switch between CPU, RAM and GPU history.
#pragma once

#include "core.h"

namespace pcstats {

// one line per second from the PC:
// "PC cpu% ram% gpu% cpuTemp gpuTemp downKBs upKBs ramUsedGB ramTotalGB\n" (-1 = unknown)
enum { F_CPU_P, F_RAM, F_GPU, F_CPUT, F_GPUT, F_DOWN, F_UP, F_RAMU, F_RAMT, NF };
static const int HISTN = 150;

struct State {
  float v[NF];               // latest values
  float shown[3];            // gauge values, animated towards v
  uint8_t hist[3][HISTN];    // percent history, one sample per update
  int histHead, histCount;
  char line[128];
  int lineLen;
  uint32_t lastData;
  int graph;                 // which history the graph shows
  float picoTemp;
  uint32_t lastPico;
};
// Not in appMem: the USB data is read in every app (see poll), so the PC's writes never
// block, and the history keeps filling while another app is open.
static State S;

static const char *const NAMES[3] = {"CPU", "RAM", "GPU"};
static const uint16_t COLS[3] = {rgb(60, 200, 255), rgb(255, 90, 200), rgb(110, 255, 120)};

static void parse(char *s) {
  if (strncmp(s, "PC ", 3) != 0) return;
  char *p = s + 3;
  for (int i = 0; i < NF; i++) {
    char *end;
    float f = strtof(p, &end);
    if (end == p) return;                      // malformed line: ignore it all
    S.v[i] = f;
    p = end;
  }
  for (int g = 0; g < 3; g++) S.hist[g][S.histHead] = (uint8_t)constrain(S.v[g], 0, 100);
  S.histHead = (S.histHead + 1) % HISTN;
  if (S.histCount < HISTN) S.histCount++;
  S.lastData = millis();
}

static void readSerial() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (S.lineLen) {
        S.line[S.lineLen] = 0;
        parse(S.line);
      }
      S.lineLen = 0;
    } else if (S.lineLen < (int)sizeof(S.line) - 1) {
      S.line[S.lineLen++] = (char)c;
    }
  }
}

// once at start-up
static void begin() {
  for (int i = 0; i < NF; i++) S.v[i] = -1;
}

// every frame, whatever app is open
static void poll() { readSerial(); }

static void enter() {}

// ---------- Drawing ----------
// 270-degree gauge, open at the bottom, with round ends. frac 0..1.
static void gaugeArc(uint16_t *fb, int cx, int cy, int r0, int r1, float frac, uint16_t col) {
  const float START = 3.927f, SWEEP = 4.712f;   // starts bottom-left (225 deg), sweeps 270 deg
  const float TWO_PI_F = 6.2831853f;
  uint16_t track = rgb(28, 31, 46), dark = dim(col);
  for (int y = -r1; y <= r1; y++)
    for (int x = -r1; x <= r1; x++) {
      int d2 = x * x + y * y;
      if (d2 > r1 * r1 || d2 < r0 * r0) continue;
      float a = atan2f((float)x, (float)-y);     // 0 = up, clockwise
      float p = fmodf(a - START + 2 * TWO_PI_F, TWO_PI_F);
      if (p > SWEEP) continue;
      float k = p / SWEEP;
      pixel(fb, cx + x, cy + y, k <= frac ? blend(dark, col, (int)(k * 256)) : track);
    }
  // round ends
  float rm = (r0 + r1) / 2.0f;
  int cr = (r1 - r0) / 2;
  float a0 = START, a1 = START + SWEEP * frac;
  fillCircle(fb, cx + (int)(sinf(a0) * rm), cy - (int)(cosf(a0) * rm), cr, frac > 0.01f ? dark : track);
  fillCircle(fb, cx + (int)(sinf(START + SWEEP) * rm), cy - (int)(cosf(START + SWEEP) * rm), cr, frac > 0.99f ? col : track);
  if (frac > 0.01f) fillCircle(fb, cx + (int)(sinf(a1) * rm), cy - (int)(cosf(a1) * rm), cr + 1, col);
}

static void gaugeCard(uint16_t *fb, int x, int y, int g, float pct, bool known, const char *sub) {
  const int w = 100, h = 100;
  uint16_t col = COLS[g];
  card(fb, x, y, w, h);
  int cx = x + w / 2, cy = y + 44;
  gaugeArc(fb, cx, cy, 27, 34, known ? pct / 100 : 0, col);
  char s[8];
  if (known) snprintf(s, sizeof(s), "%d", (int)(pct + 0.5f));
  else strcpy(s, "--");
  int tw = textWidth(MEDIUM, s);
  int tx = text(fb, MEDIUM, s, cx - (tw + (known ? 8 : 0)) / 2, cy + 8, WHITE);
  if (known) tiny(fb, "%", tx + 1, cy - 2, MUTED);
  textCenter(fb, SMALL, NAMES[g], cx, y + 82, col);
  tinyCenter(fb, sub, cx, y + 87, MUTED);
}

// "1.2 MB/s" or "340 KB/s"
static void rate(char *s, int n, float kbs) {
  if (kbs >= 1024) snprintf(s, n, "%.1f MB/s", kbs / 1024);
  else snprintf(s, n, "%.0f KB/s", kbs);
}

static void arrow(uint16_t *fb, int x, int y, bool upward, uint16_t col) {
  if (upward) fillTriangle(fb, x, y + 7, x + 8, y + 7, x + 4, y, col);
  else fillTriangle(fb, x, y, x + 8, y, x + 4, y + 7, col);
}

static void waiting(uint16_t *fb, uint32_t now) {
  card(fb, 8, 30, W - 16, 112);
  textCenter(fb, MEDIUM, "Connect your PC", W / 2, 56, WHITE);
  const char *steps[2] = {"pip install psutil pyserial", "python pc_monitor.py"};
  for (int i = 0; i < 2; i++) {
    int y = 70 + i * 28;
    fillCircle(fb, 30, y + 10, 9, COLS[0]);
    char n[2] = {(char)('1' + i), 0};
    tinyCenter(fb, n, 30, y + 7, BLACK);
    text(fb, SMALL, steps[i], 46, y + 15, i ? COLS[0] : WHITE);
  }
  tinyCenter(fb, "In the picodeck folder. Close Serial Monitor.", W / 2, 128, MUTED);

  // this Pico, as three small cards
  char v[3][24];
  uint32_t up = now / 1000;
  snprintf(v[0], 24, "%.1f C", S.picoTemp);
  snprintf(v[1], 24, "%lu:%02lu:%02lu", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60), (unsigned long)(up % 60));
  snprintf(v[2], 24, "%d KB", rp2040.getFreeHeap() / 1024);
  const char *lbl[3] = {"PICO CHIP", "UPTIME", "FREE RAM"};
  for (int i = 0; i < 3; i++) {
    int x = 8 + i * 103;
    card(fb, x, 150, 98, 52);
    tiny(fb, lbl[i], x + 10, 160, MUTED);
    text(fb, SMALL, v[i], x + 10, 192, WHITE);
  }
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  // signed: lastData is stamped a few ms after this frame's "now"
  bool everData = S.lastData != 0;
  bool live = everData && (int32_t)(now - S.lastData) < 3000;

  if (!S.lastPico || (int32_t)(now - S.lastPico) > 1000) {
    S.picoTemp = analogReadTemp();
    S.lastPico = now;
  }

  // quiet background: a few slow sparks, more when the CPU is busy
  float busy = live ? fmaxf(S.v[F_CPU_P], 0) / 100 : 0.05f;
  if (random(0, 1000) < 60 + busy * 400)
    spark(random(0, W), random(30, H), random(-30, 30), random(-30, 30), 1.5f, (int)(14 + busy * 30) * 256);
  sparksUpdate(dt);
  glowRender(fb, pal[PAL_ICE], dt);
  appHeader(fb, "PC Stats", COLS[0]);

  // status pill: LIVE (pulsing) or OFFLINE
  pill(fb, W - 6, 3, live ? "LIVE" : everData ? "OFFLINE" : "NO PC", live ? rgb(80, 230, 120) : rgb(255, 90, 90),
       live ? (int)(128 + 127 * sinf(now / 250.0f)) : 256);

  if (!everData) {
    waiting(fb, now);
    return;
  }

  // gauge cards. Old data stays on screen (dimmed numbers would hide nothing useful).
  char sub[3][24];
  if (S.v[F_CPUT] >= 0) snprintf(sub[0], 24, "%.0f C", S.v[F_CPUT]);
  else strcpy(sub[0], "temp n/a");
  snprintf(sub[1], 24, "%.1f / %.0f GB", fmaxf(S.v[F_RAMU], 0), fmaxf(S.v[F_RAMT], 0));
  if (S.v[F_GPUT] >= 0) snprintf(sub[2], 24, "%.0f C", S.v[F_GPUT]);
  else strcpy(sub[2], S.v[F_GPU] >= 0 ? "temp n/a" : "no NVIDIA GPU");
  for (int g = 0; g < 3; g++) {
    float target = fmaxf(S.v[g], 0);
    S.shown[g] += (target - S.shown[g]) * (1 - expf(-dt / 0.25f));
    gaugeCard(fb, 6 + g * 104, 28, g, S.shown[g], S.v[g] >= 0, sub[g]);
  }

  // history card: all three lines, the selected one bright with a filled area
  const int gx = 6, gy = 134, gw = W - 12, gh = 72;
  card(fb, gx, gy, gw, gh);
  const int px0 = gx + 8, py0 = gy + 20, pw = gw - 16, ph = gh - 26;
  for (int k = 0; k <= 2; k++) hline(fb, px0, py0 + ph * k / 2, pw, rgb(24, 27, 40));
  // legend chips (tap the card to switch)
  int lx = gx + 8;
  for (int g = 0; g < 3; g++) {
    bool sel = g == S.graph;
    int cw = tinyWidth(NAMES[g]) + 16;
    fillRoundRect(fb, lx, gy + 5, cw, 12, 6, sel ? dim(COLS[g]) : rgb(22, 25, 38));
    fillCircle(fb, lx + 6, gy + 11, 2, COLS[g]);
    tiny(fb, NAMES[g], lx + 11, gy + 7, sel ? WHITE : MUTED);
    lx += cw + 4;
  }
  char span[20];
  snprintf(span, sizeof(span), "last %.1f min", pw / 2 / 60.0f);   // one sample per second, 2 px each
  tiny(fb, span, gx + gw - 8 - tinyWidth(span), gy + 7, MUTED);
  int n = S.histCount;
  for (int pass = 0; pass < 4; pass++) {          // others first, selected last (on top)
    int g = pass < 3 ? pass : S.graph;
    if (pass < 3 && g == S.graph) continue;
    bool sel = pass == 3;
    uint16_t col = sel ? COLS[g] : dim4(COLS[g]);
    int prevX = -1, prevY = 0;
    for (int k = 0; k < n; k++) {
      int idx = (S.histHead + HISTN - n + k) % HISTN;
      int x = px0 + pw - 1 - (n - 1 - k) * 2;
      if (x < px0) continue;
      int y = py0 + ph - 1 - S.hist[g][idx] * (ph - 1) / 100;
      if (prevX >= 0) {
        if (sel) {
          vline(fb, x, y, py0 + ph - y, dim4(col));
          vline(fb, x - 1, (y + prevY) / 2, py0 + ph - (y + prevY) / 2, dim4(col));
          thickLine(fb, prevX, prevY, x, y, col);
        } else {
          line(fb, prevX, prevY, x, y, col);
        }
      }
      prevX = x;
      prevY = y;
    }
  }
  if (tapIn(gx, gy, gw, gh)) S.graph = (S.graph + 1) % 3;

  // network row
  char d[20], u[20];
  rate(d, sizeof(d), fmaxf(S.v[F_DOWN], 0));
  rate(u, sizeof(u), fmaxf(S.v[F_UP], 0));
  card(fb, 6, 211, 152, 26);
  card(fb, 162, 211, 152, 26);
  arrow(fb, 16, 220, false, rgb(80, 200, 255));
  tiny(fb, "DOWN", 30, 221, MUTED);
  textRight(fb, SMALL, d, 150, 230, WHITE);
  arrow(fb, 172, 220, true, rgb(255, 150, 80));
  tiny(fb, "UP", 186, 221, MUTED);
  textRight(fb, SMALL, u, 306, 230, WHITE);
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  roundRect(fb, cx - 16, cy - 12, 32, 22, 3, WHITE);
  int ys[6] = {4, -1, 2, -5, -2, -7};
  for (int k = 0; k < 5; k++) thickLine(fb, cx - 12 + k * 5, cy + ys[k], cx - 7 + k * 5, cy + ys[k + 1], col);
  fillRect(fb, cx - 6, cy + 12, 12, 3, WHITE);
}

}  // namespace pcstats
