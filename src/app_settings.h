// app_settings.h
// Settings: screen brightness, touch calibration, tear-free screen, Chindi's keyboard walk,
// and device info.
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
  if (EEPROM.read(EE_SYNC) == 0) lcdSetSync(false);
}

static void enter() {}

static void toggle(uint16_t *fb, int x, int y, bool on, uint16_t col) {
  fillRoundRect(fb, x, y, 36, 20, 10, on ? col : rgb(52, 56, 74));
  fillCircle(fb, on ? x + 26 : x + 10, y + 10, 7, WHITE);
}

// one setting row; returns true when tapped
static const int ROW_H = 35;
static bool row(uint16_t *fb, int y, const char *title, const char *sub, uint16_t col) {
  bool pressed = T.down && inBox(T.startX, T.startY, 8, y, W - 16, ROW_H) && inBox(T.x, T.y, 8, y, W - 16, ROW_H);
  fillRoundRect(fb, 8, y, W - 16, ROW_H, 10, pressed ? blend(CARD, col, 50) : CARD);
  roundRect(fb, 8, y, W - 16, ROW_H, 10, CARD_EDGE);
  fillRoundRect(fb, 18, y + 9, 3, 17, 1, col);
  text(fb, SMALL, title, 30, y + 16, INK);
  tiny(fb, sub, 30, y + 21, MUTED);
  return tapIn(8, y, W - 16, ROW_H);
}

static void frame(uint16_t *fb, float, uint32_t now) {
  backdrop(fb);
  appHeader(fb, "Settings", rgb(180, 180, 200));

  char s[56];
  // brightness
  if (row(fb, 30, "Brightness", "tap to change", rgb(255, 230, 120))) {
    lightIdx = (lightIdx + 1) % 4;
    setBacklight(LIGHTS[lightIdx]);
    EEPROM.write(EE_LIGHT, lightIdx);
    flashCommit();
  }
  snprintf(s, sizeof(s), "%d%%", LIGHTS[lightIdx]);
  pill(fb, W - 18, 39, s, rgb(255, 230, 120));

  // touch calibration
  if (row(fb, 69, "Touch calibration", "if taps land in the wrong place", rgb(200, 200, 220))) {
    calibrate();
    memset(&T, 0, sizeof(T));
    return;
  }

  // tear-free screen
  const uint16_t sc = rgb(110, 220, 170);
  if (!syncFound) strlcpy(s, "not available: this screen gives no answer", sizeof(s));
  else if (lcdSync) snprintf(s, sizeof(s), "on: in step with the screen, %.1f Hz", syncHz);
  else strlcpy(s, "off: a line can cross moving pictures", sizeof(s));
  if (row(fb, 108, "Tear-free screen", s, sc) && syncFound) {
    lcdSetSync(!lcdSync);
    EEPROM.write(EE_SYNC, lcdSync ? 1 : 0);
    flashCommit();
  }
  if (syncFound) toggle(fb, W - 54, 115, lcdSync, sc);

  // Chindi's keyboard walk
  bool kb = chindi::P.kbWalk;
  if (row(fb, 147, "Chindi keyboard walk", kb ? "she sometimes types on your PC" : "off", rgb(255, 160, 70))) {
    chindi::P.kbWalk = !kb;
    chindi::saveNow();
  }
  toggle(fb, W - 54, 154, chindi::P.kbWalk, rgb(255, 160, 70));

  // about
  card(fb, 8, 187, W - 16, 48);
  tiny(fb, "ABOUT", 18, 193, MUTED);
  uint32_t up = now / 1000;
  snprintf(s, sizeof(s), "Prats Deck   %d FPS   %d KB free", fps, rp2040.getFreeHeap() / 1024);
  text(fb, SMALL, s, 18, 216, INK);
  snprintf(s, sizeof(s), "Up %lu:%02lu:%02lu   CPU %lu MHz", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60),
           (unsigned long)(up % 60), (unsigned long)(clock_get_hz(clk_sys) / 1000000));
  tiny(fb, s, 18, 221, MUTED);
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
