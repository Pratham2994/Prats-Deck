// picodeck.ino
// Pico Deck: 12 touch apps for the Waveshare Pico-ResTouch-LCD-2.8 on a Pico 2 W.
//
//   Chindi     virtual pet cat: feed, pet, play, mini-games, photos
//   Galaxy     particle galaxy you swirl, drag and flick
//   Macro Pad  media keys and shortcuts for your PC over USB
//   PC Stats   live CPU / RAM / GPU from your PC (run pc_monitor.py)
//   Clock      internet clock + weather with a live weather background (needs Wi-Fi in config.h)
//   Wi-Fi      scanner, channel chart, best channel for your router
//   Scope      oscilloscope on GP26
//   Focus      Pomodoro timer with a burning-fuse ring
//   Paint      glow painting with kaleidoscope mirror
//   Bricks     Breakout with explosions
//   Life       Conway's Game of Life with glowing trails
//   Settings   brightness, touch calibration, Chindi's keyboard walk
//
// Go home from any app: hold the top-left corner for half a second.
//
// Arduino IDE, Tools menu:
//   Board     : Raspberry Pi Pico 2W
//   CPU Speed : 125 MHz  -> SPI runs at 62.5 MHz, the ST7789 rating
//   Optimize  : Optimize Even More (-O3)

#include <Keyboard.h>
#include "config.h"
#include "src/core.h"
#include "src/net.h"
#include "src/app_galaxy.h"
#include "src/app_macro.h"
#include "src/app_pcstats.h"
#include "src/app_clock.h"
#include "src/app_wifiscan.h"
#include "src/app_scope.h"
#include "src/app_focus.h"
#include "src/app_paint.h"
#include "src/app_bricks.h"
#include "src/app_life.h"
#include "src/app_chindi.h"
#include "src/app_settings.h"

static const App APPS[] = {
  {"Chindi", rgb(255, 160, 70), chindi::appIcon, chindi::enter, chindi::frame, chindi::leave},
  {"Galaxy", rgb(255, 140, 40), galaxy::icon, galaxy::enter, galaxy::frame, galaxy::leave},
  {"Macros", rgb(255, 120, 220), macro::icon, macro::enter, macro::frame, macro::leave},
  {"Monitor", rgb(60, 200, 255), pcstats::icon, pcstats::enter, pcstats::frame, pcstats::leave},
  {"Clock", rgb(255, 210, 80), clockapp::icon, clockapp::enter, clockapp::frame, clockapp::leave},
  {"Wi-Fi", rgb(80, 255, 120), wifiscan::icon, wifiscan::enter, wifiscan::frame, wifiscan::leave},
  {"Scope", rgb(90, 255, 130), scope::icon, scope::enter, scope::frame, scope::leave},
  {"Focus", rgb(255, 170, 40), focus::icon, focus::enter, focus::frame, focus::leave},
  {"Paint", rgb(255, 120, 220), paint::icon, paint::enter, paint::frame, paint::leave},
  {"Bricks", rgb(255, 150, 60), bricks::icon, bricks::enter, bricks::frame, bricks::leave},
  {"Life", rgb(140, 255, 120), life::icon, life::enter, life::frame, life::leave},
  {"Settings", rgb(180, 180, 200), settings::icon, settings::enter, settings::frame, settings::leave},
};
static const int NAPPS = sizeof(APPS) / sizeof(APPS[0]);

static int cur = -1;                           // running app, -1 = home
static uint32_t homeSince = 0;                 // taps on home count only for presses after this

static void openApp(int i) {
  if (cur >= 0) APPS[cur].leave();
  cur = i;
  glowClear();
  sparksClear();
  if (cur >= 0) APPS[cur].enter();
  else homeSince = millis();
}


// ---------- Home screen ----------
static const int TILE_W = 80, TILE_H = 68, TILE_Y = 34;

static void home(uint16_t *fb, float dt) {
  if (random(0, 100) < 35)
    spark(random(0, W), random(0, H), random(-25, 26), random(-25, 26), 2.5f, 30 * 256);
  sparksUpdate(dt);
  glowRender(fb, pal[PAL_AURORA], dt, 0.5f, 8);

  // status bar: name, then time and focus timer on the right
  text(fb, MEDIUM, "Pico Deck", 12, 25, WHITE);
  fillRoundRect(fb, 12, 29, 22, 2, 1, rgb(120, 230, 160));
  char s[32];
  int rx = W - 8;
  if (net::timeValid() && clockapp::tzKnown) {
    time_t t = time(nullptr) + clockapp::tzOffset;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(s, sizeof(s), "%02d:%02d", tm.tm_hour, tm.tm_min);
    textRight(fb, SMALL, s, rx, 22, WHITE);
    rx -= textWidth(SMALL, s) + 8;
  }
  if (focus::running || focus::done) {
    int secs = (int)ceilf(focus::left);
    if (focus::done) snprintf(s, sizeof(s), "DONE");
    else snprintf(s, sizeof(s), "%02d:%02d", secs / 60, secs % 60);
    rx = pill(fb, rx, 8, s, focus::isBreak ? rgb(90, 200, 255) : rgb(255, 170, 40)) - 6;
  }
  uint16_t bc;
  const char *badge = chindi::homeBadge(bc);
  if (badge && rx - tinyWidth(badge) - 22 > 128) pill(fb, rx, 8, badge, bc, (int)(128 + 127 * sinf(millis() / 200.0f)));

  for (int i = 0; i < 12; i++) {
    int x = (i % 4) * TILE_W, y = TILE_Y + (i / 4) * TILE_H;
    int cx = x + TILE_W / 2, cy = y + 27;
    if (i >= NAPPS) break;
    const char *name = APPS[i].name;
    uint16_t col = APPS[i].color;
    bool pressed = T.down && T.downMs >= homeSince && inBox(T.startX, T.startY, x, y, TILE_W, TILE_H) &&
                   inBox(T.x, T.y, x, y, TILE_W, TILE_H);
    // app icon: rounded square tinted with the app colour
    const int S = 50;
    fillRoundRect(fb, cx - S / 2, cy - S / 2, S, S, 13, pressed ? dim(col) : blend(CARD, col, 34));
    roundRect(fb, cx - S / 2, cy - S / 2, S, S, 13, pressed ? col : blend(CARD_EDGE, col, 80));
    APPS[i].icon(fb, cx, cy, col);
    textCenter(fb, SMALL, name, cx, y + 67, pressed ? col : rgb(225, 228, 240));

    if (T.tap && T.downMs >= homeSince && inBox(T.startX, T.startY, x, y, TILE_W, TILE_H)) {
      openApp(i);
      return;
    }
  }
  chindi::homePeek(fb, millis());              // she peeks in from the bottom now and then
}

// Home button: hold the top-left corner. Shows a ring that fills up.
static void homeButton(uint16_t *fb, uint32_t now) {
  const uint32_t HOLD_MS = 550;
  fillCircle(fb, 13, 11, 8, rgb(20, 20, 30));
  fillTriangle(fb, 9, 11, 15, 7, 15, 15, rgb(150, 150, 170));
  if (T.down && inHomeCorner(T.startX, T.startY) && T.moved < 25) {
    uint32_t held = now - T.downMs;
    if (held > 100) arc(fb, 13, 11, 10, 13, 0, fminf((float)(held - 100) / (HOLD_MS - 100), 1) * 6.2831853f, WHITE, WHITE);
    if (held >= HOLD_MS) openApp(-1);
  }
}


// ---------- Main ----------
void setup() {
  Serial.begin(115200);
  Keyboard.begin();
  EEPROM.begin(1024);
  lcdBegin();
  settings::begin();
  glowBegin();
  pcstats::begin();
  chindi::begin();
  getCal();
  randomSeed(rp2040.hwrand32());
  homeSince = millis();
}

void loop() {
  static int draw = 0;                         // buffer the CPU draws into
  static uint32_t lastUs = micros(), fpsT = millis();
  static int frames = 0;

  uint32_t now = millis();
  uint32_t us = micros();
  float dt = constrain((us - lastUs) / 1e6f, 0.001f, 0.1f);
  lastUs = us;

  lcdWait();                                   // previous frame finished sending
  touchUpdate(now, dt);
  lcdStart(frame[draw]);                       // send the frame drawn last time...
  draw ^= 1;                                   // ...and draw the next one meanwhile
  uint16_t *fb = frame[draw];

  focus::tick(now);
  chindi::tick(now, dt);                       // her needs change while the Pico is on
  pcstats::poll();                             // read PC data in every app, so the PC never waits
  if (cur < 0) home(fb, dt);
  else {
    APPS[cur].frame(fb, dt, now);
    if (cur >= 0) homeButton(fb, now);
  }

  frames++;
  if (now - fpsT >= 1000) {
    fps = frames * 1000 / (now - fpsT);
    frames = 0;
    fpsT = now;
  }
}
