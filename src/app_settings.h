// app_settings.h
// Settings: screen brightness, touch calibration, Chindi's keyboard walk, and device info.
#pragma once

#include "core.h"
#include "app_chindi.h"

namespace settings {

static const int LIGHTS[] = {100, 70, 40, 15};
static int lightIdx = 0;

// once at start-up
static void begin() {
  lightIdx = EEPROM.read(EE_LIGHT);
  if (lightIdx > 3) lightIdx = 0;
  setBacklight(LIGHTS[lightIdx]);
}

static void enter() {}

static void toggle(uint16_t *fb, int x, int y, bool on, uint16_t col) {
  fillRoundRect(fb, x, y, 36, 20, 10, on ? col : rgb(50, 54, 70));
  fillCircle(fb, on ? x + 26 : x + 10, y + 10, 7, WHITE);
}

// one setting row; returns true when tapped
static bool row(uint16_t *fb, int y, const char *title, const char *sub, uint16_t col) {
  bool pressed = T.down && inBox(T.startX, T.startY, 8, y, W - 16, 42) && inBox(T.x, T.y, 8, y, W - 16, 42);
  fillRoundRect(fb, 8, y, W - 16, 42, 10, pressed ? blend(CARD, col, 50) : CARD);
  roundRect(fb, 8, y, W - 16, 42, 10, CARD_EDGE);
  fillRoundRect(fb, 18, y + 12, 4, 18, 2, col);
  text(fb, SMALL, title, 32, y + 20, WHITE);
  tiny(fb, sub, 32, y + 27, MUTED);
  return tapIn(8, y, W - 16, 42);
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  if (random(0, 100) < 20) spark(random(0, W), random(30, H), random(-20, 20), random(-20, 20), 2, 18 * 256);
  sparksUpdate(dt);
  glowRender(fb, pal[PAL_ICE], dt);
  appHeader(fb, "Settings", rgb(180, 180, 200));

  char s[48];
  // brightness
  if (row(fb, 30, "Brightness", "tap to change", rgb(255, 230, 120))) {
    lightIdx = (lightIdx + 1) % 4;
    setBacklight(LIGHTS[lightIdx]);
    EEPROM.write(EE_LIGHT, lightIdx);
    EEPROM.commit();
  }
  snprintf(s, sizeof(s), "%d%%", LIGHTS[lightIdx]);
  pill(fb, W - 18, 43, s, rgb(255, 230, 120));

  // touch calibration
  if (row(fb, 78, "Touch calibration", "if taps land in the wrong place", rgb(200, 200, 220))) {
    calibrate();
    memset(&T, 0, sizeof(T));
    return;
  }

  // Chindi's keyboard walk
  bool kb = chindi::P.kbWalk;
  if (row(fb, 126, "Chindi keyboard walk", kb ? "she sometimes types on your PC" : "off", rgb(255, 160, 70))) {
    chindi::P.kbWalk = !kb;
    chindi::saveNow();
  }
  toggle(fb, W - 54, 137, chindi::P.kbWalk, rgb(255, 160, 70));

  // about
  card(fb, 8, 176, W - 16, 58);
  tiny(fb, "ABOUT", 18, 184, MUTED);
  uint32_t up = now / 1000;
  snprintf(s, sizeof(s), "Pico Deck   %d FPS   %d KB free", fps, rp2040.getFreeHeap() / 1024);
  text(fb, SMALL, s, 18, 208, WHITE);
  snprintf(s, sizeof(s), "Up %lu:%02lu:%02lu   CPU %lu MHz", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60),
           (unsigned long)(up % 60), (unsigned long)(clock_get_hz(clk_sys) / 1000000));
  tiny(fb, s, 18, 218, MUTED);
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int k = 0; k < 8; k++) {
    float a = k * 0.785f;
    fillCircle(fb, cx + (int)(cosf(a) * 13), cy + (int)(sinf(a) * 13), 4, col);
  }
  fillCircle(fb, cx, cy, 12, col);
  fillCircle(fb, cx, cy, 5, rgb(30, 32, 44));
}

}  // namespace settings
