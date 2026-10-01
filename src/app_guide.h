// app_guide.h
// Guide: what each app is and how to use it, in plain words.
//   Tap a name to read about it. Prev / Next turn the pages. All goes back to the list.
#pragma once

#include "core.h"

namespace guide {

struct Page {
  const char *name;
  uint16_t col;
  const char *what;          // one or two sentences
  const char *how;           // one tip per line
};

static const Page PAGES[] = {
  {"Basics", rgb(120, 170, 255),
   "Twelve small apps on one touch screen. Use the stylus and press firmly.",
   "Tap an app to open it.\n"
   "To go home, hold the top-left corner for half a second.\n"
   "Taps land in the wrong place? Go to Settings, Touch calibration."},
  {"Chindi", rgb(255, 160, 70),
   "Your pet cat. She gets hungry, sleepy and bored, and she wants attention.",
   "Stroke her to pet her. Use the dock to feed, play, clean and sleep.\n"
   "Tap the things in her room. She uses them.\n"
   "Grant her daily wishes to earn treats."},
  {"Galaxy", rgb(255, 140, 40),
   "A galaxy of glowing dots that follow the stylus. It is only for fun.",
   "Hold: the dots circle the stylus.\n"
   "Drag: they follow. Flick: they fly away.\n"
   "Tap the top-right corner to change the colours."},
  {"Macros", rgb(255, 120, 220),
   "Buttons that control your PC. The deck works as a USB keyboard.",
   "Top buttons: music and volume.\n"
   "Bottom buttons: Windows shortcuts, such as Copy, Paste and Lock.\n"
   "Hold Vol + or Vol - to repeat."},
  {"Monitor", rgb(60, 200, 255),
   "Shows how hard your PC works: processor, memory, graphics, heat, network.",
   "On the PC, run: python pc_monitor.py\n"
   "Keep the USB cable connected.\n"
   "Tap the graph to change between CPU, RAM and GPU."},
  {"Clock", rgb(255, 210, 80),
   "A clock set from the internet, with the weather for today and 2 more days.",
   "It needs Wi-Fi. Put your network name and password in config.h, then upload again.\n"
   "Tap the weather panel to refresh it."},
  {"Wi-Fi", rgb(80, 255, 120),
   "Finds the Wi-Fi networks near you and shows how strong each one is.",
   "List: all networks, the strongest first.\n"
   "Chart: which channels are crowded.\n"
   "The bottom line gives the best channel for your own router."},
  {"Scope", rgb(90, 255, 130),
   "An oscilloscope. It draws how an electrical signal changes with time.",
   "Connect the signal to pin GP26. 3.3 V is the maximum.\n"
   "Zoom changes the time scale. Hold stops the picture.\n"
   "Test: wire GP0 to GP26 to see a 1 kHz wave."},
  {"Paint", rgb(255, 120, 220),
   "Paint with light. Lines that cross become brighter.",
   "Drag to paint.\n"
   "Mirror makes 2-way or 6-way patterns.\n"
   "Fade lets old lines go away. Clear starts again."},
  {"Bricks", rgb(255, 150, 60),
   "The classic game. Hit the ball with the bat and break all the bricks.",
   "Drag to move the bat. Tap to launch the ball.\n"
   "You have 3 lives.\n"
   "Your high score is saved."},
  {"Life", rgb(140, 255, 120),
   "A famous simulation. Each dot lives or dies by the count of its neighbours.",
   "Simple rules make patterns that move and grow.\n"
   "Draw on the screen to add living dots.\n"
   "Pause, change the speed, or start with a Random field."},
  {"Settings", rgb(180, 180, 200),
   "Screen brightness, touch calibration and other options.",
   "Tap a row to change it.\n"
   "Tear-free screen removes the line that crosses moving pictures. Some screens cannot do it."},
};
static const int NPAGES = sizeof(PAGES) / sizeof(PAGES[0]);
static const uint16_t ACCENT = rgb(120, 170, 255);

static int page = -1;                          // -1 = the list

static void enter() { page = -1; }

static void listView(uint16_t *fb) {
  appHeader(fb, "Guide", ACCENT);
  tiny(fb, "tap a name", W - 8 - tinyWidth("tap a name"), 8, MUTED);
  for (int i = 0; i < NPAGES; i++) {
    int x = 6 + (i % 2) * 156, y = 30 + (i / 2) * 35, w = 152, h = 31;
    bool pressed = T.down && inBox(T.startX, T.startY, x, y, w, h) && inBox(T.x, T.y, x, y, w, h);
    fillRoundRect(fb, x, y, w, h, 9, pressed ? blend(CARD, PAGES[i].col, 70) : CARD);
    roundRect(fb, x, y, w, h, 9, pressed ? PAGES[i].col : CARD_EDGE);
    fillCircle(fb, x + 15, y + 15, 4, PAGES[i].col);
    text(fb, SMALL, PAGES[i].name, x + 28, y + 20, INK);
    if (tapIn(x, y, w, h)) page = i;
  }
}

static void pageView(uint16_t *fb) {
  const Page &p = PAGES[page];
  appHeader(fb, p.name, p.col);
  char s[24];
  snprintf(s, sizeof(s), "%d / %d", page + 1, NPAGES);
  tiny(fb, s, W - 8 - tinyWidth(s), 8, MUTED);

  int y = 34;
  tiny(fb, "WHAT IT IS", 12, y, p.col);
  y = textWrap(fb, SMALL, p.what, 12, y + 23, W - 24, 16, INK) - 8;
  tiny(fb, "HOW TO USE IT", 12, y, p.col);
  y += 23;
  // one tip per line, each with a dot
  char line[160];
  const char *h = p.how;
  while (*h) {
    int n = 0;
    while (h[n] && h[n] != '\n' && n < (int)sizeof(line) - 1) n++;
    memcpy(line, h, n);
    line[n] = 0;
    fillCircle(fb, 15, y - 4, 2, p.col);
    y = textWrap(fb, SMALL, line, 24, y, W - 36, 16, rgb(206, 210, 226)) + 3;
    h += n;
    if (*h == '\n') h++;
  }

  static const char *const LABELS[] = {"Prev", "All", "Next"};
  int hit = toolbar(fb, LABELS, 3, p.col);
  if (hit == 0) page = (page + NPAGES - 1) % NPAGES;
  if (hit == 1) page = -1;
  if (hit == 2) page = (page + 1) % NPAGES;
}

static void frame(uint16_t *fb, float, uint32_t) {
  backdrop(fb);
  if (page < 0) listView(fb);
  else pageView(fb);
}

static void leave() {}

// an open book
static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  fillRoundRect(fb, cx - 16, cy - 11, 15, 22, 3, WHITE);
  fillRoundRect(fb, cx + 1, cy - 11, 15, 22, 3, WHITE);
  for (int k = 0; k < 3; k++) {
    fillRect(fb, cx - 12, cy - 6 + k * 5, 8, 2, col);
    fillRect(fb, cx + 5, cy - 6 + k * 5, 8, 2, col);
  }
}

}  // namespace guide
