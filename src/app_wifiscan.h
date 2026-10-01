// app_wifiscan.h
// Wi-Fi scanner: nearby 2.4 GHz networks with signal strength, a channel chart, and the
// least busy channel for your router. Radar background: each network is a blip, closer
// to the centre when stronger. Works without Wi-Fi settings. Rescans every few seconds.
#pragma once

#include <WiFi.h>
#include "core.h"

namespace wifiscan {

static const int MAXN = 40;
struct Net {
  char ssid[33];
  int8_t rssi;
  uint8_t ch;
  bool open;
};
struct State {
  Net nets[MAXN];
  int count;
  bool scanning, everScanned;
  uint32_t nextScan;
  int view;                  // 0 = list, 1 = chart
  float scroll, scrollStart;
  float sweep;               // radar angle
  int best;                  // least busy channel (1, 6 or 11)
};
APP_STATE(State, S)

static const uint16_t GREEN = rgb(80, 255, 120);

static uint16_t strengthColor(int rssi) {
  if (rssi > -55) return rgb(80, 255, 120);
  if (rssi > -67) return rgb(200, 255, 80);
  if (rssi > -78) return rgb(255, 170, 40);
  return rgb(255, 70, 60);
}

static uint32_t hashStr(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
  return h;
}

static void startScan(uint32_t now) {
  if (WiFi.scanNetworks(true) < 0) {           // -1 = scan started
    S.scanning = true;
  } else {
    S.scanning = false;                        // could not start: retry later
    S.nextScan = now + 2000;
  }
}

static void collect(int n) {
  S.count = 0;
  for (int i = 0; i < n && S.count < MAXN; i++) {
    Net &e = S.nets[S.count++];
    const char *ss = WiFi.SSID(i);
    strlcpy(e.ssid, ss && ss[0] ? ss : "(hidden)", sizeof(e.ssid));
    e.rssi = (int8_t)constrain(WiFi.RSSI(i), -100, -20);
    e.ch = WiFi.channel(i);
    e.open = WiFi.encryptionType(i) == ENC_TYPE_NONE;
  }
  // strongest first
  for (int i = 1; i < S.count; i++)
    for (int j = i; j > 0 && S.nets[j - 1].rssi < S.nets[j].rssi; j--) {
      Net t = S.nets[j];
      S.nets[j] = S.nets[j - 1];
      S.nets[j - 1] = t;
    }
  // channel load: stronger and closer networks count more
  float bestLoad = 1e30f;
  for (int c : {1, 6, 11}) {
    float load = 0;
    for (int i = 0; i < S.count; i++) {
      int d = abs(S.nets[i].ch - c);
      if (d < 5) load += powf(10, (S.nets[i].rssi + 100) / 20.0f) * (1 - d / 5.0f);
    }
    if (load < bestLoad) {
      bestLoad = load;
      S.best = c;
    }
  }
  S.everScanned = true;
}

static void enter() {
  memset(&S, 0, sizeof(S));
  S.best = 0;
  startScan(millis());
}

static void radar(float dt) {
  const float cx = W / 2, cy = 120, TWO_PI_F = 6.2831853f;
  float old = S.sweep, swept = 2.2f * dt;
  S.sweep = fmodf(S.sweep + swept, TWO_PI_F);
  glowLine(cx, cy, cx + cosf(S.sweep) * 170, cy + sinf(S.sweep) * 170, 22 * 256);
  for (int i = 0; i < S.count; i++) {
    float a = (hashStr(S.nets[i].ssid) % 6283) / 1000.0f;
    // blip lights up as the sweep passes it
    if (fmodf(a - old + TWO_PI_F, TWO_PI_F) <= swept) {
      float r = 18 + (-30 - S.nets[i].rssi) * 1.6f;
      glowSplat(cx + cosf(a) * r, cy + sinf(a) * r, 5, 255 * 256);
    }
  }
}

static void bars(uint16_t *fb, int x, int y, int rssi) {
  int n = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
  uint16_t c = strengthColor(rssi);
  for (int k = 0; k < 4; k++) fillRect(fb, x + k * 4, y + 12 - (k + 1) * 3, 3, (k + 1) * 3, k < n ? c : rgb(40, 40, 50));
}

static void lockIcon(uint16_t *fb, int x, int y, uint16_t col) {
  roundRect(fb, x + 1, y, 6, 7, 2, col);
  fillRect(fb, x, y + 4, 8, 6, col);
}

static void listView(uint16_t *fb) {
  const int top = 26, rowH = 21, bottom = 200;
  int visible = (bottom - top) / rowH;
  float maxScroll = fmaxf(0, (S.count - visible) * rowH);
  if (T.pressed) S.scrollStart = S.scroll;
  if (T.down && inBox(T.startX, T.startY, 0, top, W, bottom - top) && T.moved > 6)
    S.scroll = constrain(S.scrollStart - (T.y - T.startY), 0, maxScroll);
  for (int i = 0; i < S.count; i++) {
    int y = top + i * rowH - (int)S.scroll;
    if (y < top - rowH || y > bottom) continue;
    if (y < top || y + rowH > bottom) continue;
    const Net &e = S.nets[i];
    fillRoundRect(fb, 4, y, W - 8, rowH - 2, 6, CARD);
    bars(fb, 10, y + 3, e.rssi);
    char name[24];
    strlcpy(name, e.ssid, sizeof(name));
    while (textWidth(SMALL, name) > 170 && strlen(name) > 1) name[strlen(name) - 1] = 0;
    text(fb, SMALL, name, 32, y + 14, WHITE);
    if (!e.open) lockIcon(fb, 208, y + 4, LIGHTGREY);
    char s[16];
    snprintf(s, sizeof(s), "ch %d", e.ch);
    tiny(fb, s, 222, y + 6, LIGHTGREY);
    snprintf(s, sizeof(s), "%d dBm", e.rssi);
    tiny(fb, s, W - 10 - tinyWidth(s), y + 6, strengthColor(e.rssi));
  }
}

static void chartView(uint16_t *fb) {
  const int x0 = 30, x1 = 312, y0 = 30, y1 = 190;
  auto xOf = [&](float ch) { return x0 + (int)((ch + 1) * (x1 - x0) / 16); };
  auto yOf = [&](int rssi) { return y1 - (rssi + 100) * (y1 - y0) / 70; };
  shadeRect(fb, x0, y0, x1 - x0, y1 - y0, true);
  for (int r = -90; r <= -30; r += 20) {
    hline(fb, x0, yOf(r), x1 - x0, rgb(30, 40, 30));
    char s[8];
    snprintf(s, sizeof(s), "%d", r);
    tiny(fb, s, 2, yOf(r) - 4, LIGHTGREY);
  }
  for (int c = 1; c <= 13; c++) {
    char s[4];
    snprintf(s, sizeof(s), "%d", c);
    tinyCenter(fb, s, xOf(c), y1 + 3, c == S.best ? GREEN : LIGHTGREY);
  }
  hline(fb, x0, y1, x1 - x0, GREY);
  for (int i = S.count - 1; i >= 0; i--) {     // weakest first, so strong ones draw on top
    const Net &e = S.nets[i];
    uint16_t col = hsv(hashStr(e.ssid) % 360);
    int xc = xOf(e.ch), half = xOf(e.ch + 2) - xc, peak = y1 - yOf(e.rssi);
    int px = -1, py = 0;
    for (int x = xc - half; x <= xc + half; x += 2) {
      float f = (float)(x - xc) / half;
      int y = y1 - (int)(peak * (1 - f * f));
      if (px >= 0) thickLine(fb, px, py, x, y, col);
      px = x;
      py = y;
    }
    char name[12];
    strlcpy(name, e.ssid, sizeof(name));
    tinyCenter(fb, name, xc, y1 - peak - 10, col);
  }
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  if (S.scanning) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      collect(n);
      S.scanning = false;
      S.nextScan = now + 5000;
    }
  } else if ((int32_t)(now - S.nextScan) >= 0) {
    startScan(now);
  }

  radar(dt);
  glowRender(fb, pal[PAL_PHOSPHOR], dt, 0.2f, 20);
  appHeader(fb, "Wi-Fi Scan", GREEN);
  button(fb, 196, 1, 58, 20, "List", GREEN, S.view == 0);
  button(fb, 258, 1, 58, 20, "Chart", GREEN, S.view == 1);
  if (tapIn(196, 0, 60, 24)) S.view = 0;
  if (tapIn(256, 0, 64, 24)) S.view = 1;

  if (!S.everScanned) {
    textCenter(fb, MEDIUM, "Scanning...", W / 2, 130, WHITE);
  } else if (S.count == 0) {
    textCenter(fb, MEDIUM, "No networks found", W / 2, 130, WHITE);
  } else if (S.view == 0) {
    listView(fb);
  } else {
    chartView(fb);
  }

  card(fb, 4, 203, W - 8, 34);
  char s[48];
  snprintf(s, sizeof(s), "%d networks%s", S.count, S.scanning ? "  - scanning" : "");
  tiny(fb, s, 12, 208, MUTED);
  if (S.best) {
    snprintf(s, sizeof(s), "Best channel for your router: %d", S.best);
    text(fb, SMALL, s, 12, 231, GREEN);
  }
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int r = 18; r >= 6; r -= 6) {
    arc(fb, cx, cy + 10, r - 3, r, 5.5f, 6.28f, col, col);
    arc(fb, cx, cy + 10, r - 3, r, 0, 0.78f, col, col);
  }
  fillCircle(fb, cx, cy + 10, 3, WHITE);
}

}  // namespace wifiscan
