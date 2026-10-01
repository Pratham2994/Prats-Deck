// app_focus.h
// Focus timer (Pomodoro). The ring is a burning fuse: it shrinks as time runs out and
// throws sparks from its tip. Fireworks when a session ends.
//   Tap the centre : start / pause
//   Hold the centre: reset
//   Buttons        : 15, 25, 50 minute focus, or a 5 minute break
#pragma once

#include "core.h"

namespace focus {

static const int CX = 160, CY = 104, R0 = 76, R1 = 86;
static const float TWO_PI_F = 6.2831853f;

static float total = 25 * 60, left = 25 * 60;   // seconds (kept between visits)
static bool running = false, isBreak = false, done = false;
static int sessions = 0;
static uint32_t lastMs = 0, doneAt = 0, nextFirework = 0;
static bool resetFired = false;

static void setTimer(float minutes, bool brk) {
  total = left = minutes * 60;
  isBreak = brk;
  running = false;
  done = false;
}

// Count down on real time. main calls this every frame, so the timer keeps running
// while another app is open.
static void tick(uint32_t now) {
  float elapsed = (now - lastMs) / 1000.0f;
  lastMs = now;
  if (running && !done) {
    left -= elapsed;
    if (left <= 0) {
      left = 0;
      running = false;
      done = true;
      doneAt = now;
      if (!isBreak) sessions++;
    }
  }
}

static void enter() {}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  uint16_t col = isBreak ? rgb(90, 200, 255) : rgb(255, 170, 40);
  float frac = total > 0 ? left / total : 0;

  // fuse tip sparks
  if (running) {
    float a = frac * TWO_PI_F;
    float sx = CX + sinf(a) * (R0 + R1) / 2, sy = CY - cosf(a) * (R0 + R1) / 2;
    for (int k = 0; k < 3; k++)
      spark(sx, sy, random(-90, 90), random(-110, 40), 0.6f, 90 * 256, 160);
    glowSplat(sx, sy, 8, 120 * 256);
  }
  // fireworks when done
  if (done && now - doneAt < 5000 && (int32_t)(now - nextFirework) >= 0) {
    sparkBurst(random(40, W - 40), random(30, 150), 70, 320, 1.4f, 120 * 256, 90);
    nextFirework = now + random(200, 500);
  }
  sparksUpdate(dt);
  glowRender(fb, pal[isBreak ? PAL_ICE : PAL_GOLD], dt);

  // ring: dark track, then the remaining part
  arc(fb, CX, CY, R0, R1, 0, TWO_PI_F, rgb(28, 24, 20), rgb(28, 24, 20));
  if (frac > 0.002f) arc(fb, CX, CY, R0, R1, 0, frac * TWO_PI_F, dim(col), col);

  // time
  int secs = (int)ceilf(left);
  char s[16];
  snprintf(s, sizeof(s), "%02d:%02d", secs / 60, secs % 60);
  const int dw = 26, dh = 48, dtk = 6;
  int tw = seg7Width(s, dw, dtk);
  seg7Text(fb, s, CX - tw / 2, CY - dh / 2 - 6, dw, dh, dtk, done ? WHITE : col);
  const char *state = done ? (isBreak ? "Break over!" : "Well done!") : running ? (isBreak ? "BREAK" : "FOCUS") : (left < total ? "PAUSED" : "TAP TO START");
  textCenter(fb, SMALL, state, CX, CY + 42, done ? WHITE : LIGHTGREY);
  for (int i = 0; i < min(sessions, 8); i++) fillCircle(fb, CX - (min(sessions, 8) - 1) * 6 + i * 12, CY + 54, 3, rgb(255, 170, 40));

  appHeader(fb, "Focus", col);
  if (sessions) {
    char t[24];
    snprintf(t, sizeof(t), "%d done today", sessions);
    tiny(fb, t, W - 6 - tinyWidth(t), 8, LIGHTGREY);
  }

  // centre: tap = start/pause, hold 1 s = reset
  bool inCentre = inBox(T.startX, T.startY, CX - R0, CY - R0, 2 * R0, 2 * R0);
  if (T.down && inCentre && T.moved < 20 && now - T.downMs > 1000 && !resetFired) {
    setTimer(total / 60, isBreak);
    resetFired = true;
    sparkBurst(CX, CY, 60, 200, 0.7f, 80 * 256);
  }
  if (T.released) {
    if (T.tap && inCentre && !resetFired) {
      if (done) setTimer(isBreak ? 25 : 5, !isBreak);         // next: break after focus, focus after break
      else running = !running;
    }
    resetFired = false;
  }
  if (T.down && inCentre && now - T.downMs > 300 && !resetFired) {
    float hold = fminf((now - T.downMs - 300) / 700.0f, 1);
    arc(fb, CX, CY, R0 - 8, R0 - 4, 0, hold * TWO_PI_F, WHITE, WHITE);
  }

  static const char *const LABELS[] = {"15 min", "25 min", "50 min", "Break 5"};
  int hit = toolbar(fb, LABELS, 4, col);
  if (hit == 0) setTimer(15, false);
  if (hit == 1) setTimer(25, false);
  if (hit == 2) setTimer(50, false);
  if (hit == 3) setTimer(5, true);
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  arc(fb, cx, cy, 13, 18, 0, 4.5f, dim(col), col);
  fillRect(fb, cx - 1, cy - 9, 3, 10, WHITE);
  fillRect(fb, cx - 1, cy - 1, 8, 3, WHITE);
}

}  // namespace focus
