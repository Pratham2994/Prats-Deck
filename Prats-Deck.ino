// Prats-Deck.ino
// Prats Deck: 15 touch apps for the Waveshare Pico-ResTouch-LCD-2.8 on a Pico 2 W.
//
//   Chindi     virtual pet cat: feed, pet, play, mini-games, photos
//   Galaxy     particle galaxy you swirl, drag and flick
//   Macros     media keys and shortcuts for your PC over USB
//   Monitor    live CPU / RAM / GPU from your PC (run pc_monitor.py)
//   Clock      internet clock + weather with a live weather background (needs Wi-Fi in config.h)
//   Wi-Fi      scanner, channel chart, best channel for your router
//   Scope      oscilloscope on GP26
//   Guide      what each app is and how to use it
//   Paint      glow painting with kaleidoscope mirror
//   Bricks     Breakout with explosions
//   Life       Conway's Game of Life with glowing trails
//   Snake      the classic: tap beside the head to turn
//   2048       slide the tiles, join the same numbers
//   Trackpad   the screen is a mouse pad for your PC over USB
//   Settings   brightness, touch calibration, tear-free screen, Chindi's keyboard walk
//
// Go home from any app: hold the top-left corner for half a second.
//
// Arduino IDE, Tools menu:
//   Board     : Raspberry Pi Pico 2W
//   CPU Speed : 125 MHz  -> SPI runs at 62.5 MHz, the ST7789 rating
//   Optimize  : Small (-Os), the default. Do NOT use -O3: with board package 6.2.0 it breaks
//               the USB code, and the board then needs BOOTSEL to be flashed again.

#include <Keyboard.h>
#include <Mouse.h>
#include "config.h"
// The sketch's own code is built for speed (about 3 times the frame rate of -Os), whatever
// the Tools menu says. The board package stays at the menu setting.
#pragma GCC optimize ("O2")
#include "src/core.h"
#include "src/net.h"
#include "src/app_galaxy.h"
#include "src/app_macro.h"
#include "src/app_pcstats.h"
#include "src/app_clock.h"
#include "src/app_wifiscan.h"
#include "src/app_scope.h"
#include "src/app_guide.h"
#include "src/app_paint.h"
#include "src/app_bricks.h"
#include "src/app_life.h"
#include "src/app_snake.h"
#include "src/app_2048.h"
#include "src/app_trackpad.h"
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
  {"Guide", rgb(120, 170, 255), guide::icon, guide::enter, guide::frame, guide::leave},
  {"Paint", rgb(255, 120, 220), paint::icon, paint::enter, paint::frame, paint::leave},
  {"Bricks", rgb(255, 150, 60), bricks::icon, bricks::enter, bricks::frame, bricks::leave},
  {"Life", rgb(140, 255, 120), life::icon, life::enter, life::frame, life::leave},
  {"Snake", rgb(120, 230, 110), snake::icon, snake::enter, snake::frame, snake::leave},
  {"2048", rgb(240, 180, 70), g2048::icon, g2048::enter, g2048::frame, g2048::leave},
  {"Trackpad", rgb(120, 200, 255), trackpad::icon, trackpad::enter, trackpad::frame, trackpad::leave},
  {"Settings", rgb(180, 180, 200), settings::icon, settings::enter, settings::frame, settings::leave},
};
static const int NAPPS = sizeof(APPS) / sizeof(APPS[0]);

// A build with -DDECK_TEST cannot lock the board. If the firmware hangs, or if no PC sees
// it on USB 12 s after start-up, the deck goes to boot mode by itself, where a new firmware
// can be flashed with no press of the BOOTSEL button. Use it to try a change that could break
// USB or crash. Do not use it for the firmware you keep: on a charger it would go to boot mode.
#ifdef DECK_TEST
extern "C" bool tud_mounted(void);             // TinyUSB: the PC has set the deck up as a USB device
#endif

static int cur = -1;                           // running app, -1 = home
static uint32_t homeSince = 0;                 // taps on home count only for presses after this

static void openApp(int i) {
  if (cur >= 0) APPS[cur].leave();
  cur = i;
  shownApp = i;
  glowClear();
  sparksClear();
  if (cur >= 0) APPS[cur].enter();
  else homeSince = millis();
}


// ---------- Home screen ----------
static const int HOME_COLS = 5;                // 5 x 3 tiles: room for 15 apps
static const int TILE_W = W / HOME_COLS, TILE_H = 62, TILE_Y = 54, ICON = 46;

// Top of the home page: big clock with the date and weather, or the name while there is
// no internet time. Chindi's mood is on the right.
static void homeHeader(uint16_t *fb, uint32_t now) {
  net::Status st = clockapp::service(now);     // keeps the time and weather fresh from here too
  char s[40];
  int x = 12;
  if (net::timeValid() && clockapp::tzKnown) {
    time_t t = time(nullptr) + clockapp::tzOffset;
    struct tm tm;
    gmtime_r(&t, &tm);
    int h = tm.tm_hour;
    if (CLOCK_24H) snprintf(s, sizeof(s), "%02d:%02d", h, tm.tm_min);
    else snprintf(s, sizeof(s), "%d:%02d", h % 12 == 0 ? 12 : h % 12, tm.tm_min);
    x = text(fb, GIANT, s, 10, 45, INK) + 11;
    static const char *const DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    tiny(fb, "PRATS DECK", x, 11, rgb(120, 170, 255));
    snprintf(s, sizeof(s), "%s %d %s", DAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon]);
    text(fb, SMALL, s, x, 33, INK);
    if (clockapp::fetchPending) strlcpy(s, "updating...", sizeof(s));
    else if (clockapp::haveWeather)
      snprintf(s, sizeof(s), "%s %.0f%c", clockapp::KIND_NAMES[clockapp::kindOf(clockapp::code)], clockapp::temp, 127);
    else s[0] = 0;
    tiny(fb, s, x, 38, MUTED);
  } else {
    text(fb, LARGE, "Prats Deck", 12, 36, INK);
    const char *sub = st == net::NO_CONFIG ? "no Wi-Fi set: the clock is off"
                    : st == net::FAILED ? "Wi-Fi failed" : st == net::CONNECTED ? "getting the time..." : "connecting to Wi-Fi...";
    tiny(fb, sub, 13, 41, MUTED);
  }

  // Chindi's mood chip. Tap it to visit her.
  uint16_t mc;
  const char *mood = chindi::homeMood(mc);
  if (mood) {
    int cw = textWidth(SMALL, mood) + 40, cx = W - 8 - cw;
    bool pressed = T.down && T.downMs >= homeSince && inBox(T.startX, T.startY, cx, 12, cw, 30);
    fillRoundRect(fb, cx, 12, cw, 30, 15, pressed ? blend(CARD, mc, 80) : CARD);
    roundRect(fb, cx, 12, cw, 30, 15, blend(CARD_EDGE, mc, 110));
    kitty::drawHead(fb, cx + 17, 28, 0.2f, 1, 0);
    text(fb, SMALL, mood, cx + 32, 32, mc);
  }
}

static void home(uint16_t *fb, float /*dt*/, uint32_t now) {
  backdrop(fb);
  homeHeader(fb, now);
  if (T.tap && T.downMs >= homeSince && T.startY < TILE_Y - 8 && T.startX > 200) {
    openApp(0);
    return;
  }

  for (int i = 0; i < NAPPS; i++) {
    int x = (i % HOME_COLS) * TILE_W, y = TILE_Y + (i / HOME_COLS) * TILE_H;
    int cx = x + TILE_W / 2, cy = y + 2 + ICON / 2;
    uint16_t col = APPS[i].color;
    bool pressed = T.down && T.downMs >= homeSince && inBox(T.startX, T.startY, x, y, TILE_W, TILE_H) &&
                   inBox(T.x, T.y, x, y, TILE_W, TILE_H);
    // app icon: rounded square with a soft tint of the app colour
    fillRoundRectV(fb, cx - ICON / 2, cy - ICON / 2, ICON, ICON, 14, blend(CARD, col, pressed ? 130 : 64),
                   blend(CARD, col, pressed ? 80 : 20));
    roundRect(fb, cx - ICON / 2, cy - ICON / 2, ICON, ICON, 14, blend(CARD_EDGE, col, pressed ? 220 : 96));
    APPS[i].icon(fb, cx, cy, col);
    tinyCenter(fb, APPS[i].name, cx, y + 51, pressed ? col : rgb(214, 218, 232));

    if (T.tap && T.downMs >= homeSince && inBox(T.startX, T.startY, x, y, TILE_W, TILE_H)) {
      openApp(i);
      return;
    }
  }
  chindi::homePeek(fb, now);                   // she peeks in from the bottom now and then
}

// Home button: hold the top-left corner. Shows a ring that fills up.
static void homeButton(uint16_t *fb, uint32_t now) {
  const uint32_t HOLD_MS = 550;
  fillCircle(fb, 13, 12, 9, rgb(22, 24, 36));
  strokeLine(fb, 15, 7.5f, 10.5f, 12, 2, rgb(170, 174, 196));
  strokeLine(fb, 10.5f, 12, 15, 16.5f, 2, rgb(170, 174, 196));
  if (T.down && inHomeCorner(T.startX, T.startY) && T.moved < 25) {
    uint32_t held = now - T.downMs;
    if (held > 100) arc(fb, 13, 12, 10, 13, 0, fminf((float)(held - 100) / (HOLD_MS - 100), 1) * 6.2831853f, WHITE, WHITE);
    if (held >= HOLD_MS) openApp(-1);
  }
}


// ---------- Main ----------
void setup() {
#ifdef DECK_TEST
  if (rp2040.getResetReason() == RP2040::WDT_RESET) rp2040.rebootToBootloader();   // it hung: wait for a new firmware
  rp2040.wdt_begin(5000);
#endif
  Serial.begin(115200);
  Keyboard.begin();
  Mouse.begin();
  EEPROM.begin(1024);
  lcdBegin();
  settings::begin();
  glowBegin();
  pcstats::begin();
  chindi::begin();
  getCal();
  randomSeed(rp2040.hwrand32());
  homeSince = millis();
#ifdef DECK_PROF
  openApp(DECK_PROF < -1 ? -1 : DECK_PROF);
#endif
}

void loop() {
  static uint16_t *fb = frame[0];              // buffer the CPU draws into
  static uint32_t lastUs = micros(), fpsT = millis();
  static int frames = 0;

  uint32_t now = millis();
  uint32_t us = micros();
  float dt = constrain((us - lastUs) / 1e6f, 0.001f, 0.1f);
  lastUs = us;

#ifdef DECK_TEST
  rp2040.wdt_reset();                          // a hang stops this, and the watchdog restarts the deck
  if (now > 12000 && !tud_mounted()) rp2040.rebootToBootloader();
#endif
  lcdWait();                                   // previous frame finished sending
#ifdef DECK_PROF
  uint32_t p1 = micros();
#endif
  touchUpdate(now, dt);
  fb = lcdPresent(fb);                         // send the frame drawn last time, draw the next one meanwhile
#ifdef DECK_PROF
  uint32_t p2 = micros();
#endif

  chindi::tick(now, dt);                       // her needs change while the Pico is on
  pcstats::poll();                             // read PC data in every app, so the PC never waits
  if (cur < 0) home(fb, dt, now);
  else {
    APPS[cur].frame(fb, dt, now);
    if (cur >= 0) homeButton(fb, now);
  }

  frames++;
#ifdef DECK_PROF
  profAcc[P_WAIT] += p1 - us;
  profAcc[P_PRESENT] += p2 - p1;
  profAcc[P_APP] += micros() - p2;
  if (now - fpsT >= 1000) {
    static const char *const PN[P_COUNT] = {"wait", "present", "app", "logic", "room", "props", "build", "cat", "fx", "stats", "msg", "dock"};
    Serial.printf("PROF app=%d fps=%d sync=%d", cur, (int)(frames * 1000 / (now - fpsT)), lcdSync);
    for (int i = 0; i < P_COUNT; i++) {
      if (profAcc[i]) Serial.printf(" %s=%lu", PN[i], (unsigned long)(profAcc[i] / frames));
      profAcc[i] = 0;
    }
    // work shared with core 1: the time of core 0's part, of core 1's part, and core 0's wait for core 1
    Serial.printf(" core0=%lu core1=%lu wait1=%lu\n", (unsigned long)(shareOwn / frames), (unsigned long)(shareOther / frames),
                  (unsigned long)(shareWait / frames));
    shareOwn = shareWait = shareOther = 0;
    static int profSec = 0;                    // DECK_PROF=-2: go through all the apps, 4 s each
    if (DECK_PROF == -2 && ++profSec % 4 == 0) openApp(cur + 1 >= NAPPS ? -1 : cur + 1);
  }
#endif
  if (now - fpsT >= 1000) {
    fps = frames * 1000 / (now - fpsT);
    frames = 0;
    fpsT = now;
  }
}
