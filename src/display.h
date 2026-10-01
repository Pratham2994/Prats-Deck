// display.h
// ST7789 320x240 over SPI1 with DMA, two frame buffers, tear-free sending, and drawing helpers.
// Waveshare Pico-ResTouch-LCD-2.8 on a Pico 2 W.
#pragma once

#include <hardware/spi.h>
#include <hardware/dma.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include "fonts/inter.h"
#include "twocore.h"

#define PIN_DC 8
#define PIN_CS 9
#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_MISO 12
#define PIN_BL 13
#define PIN_RST 15
#define PIN_TP_CS 16
#define PIN_TP_IRQ 17

static const int W = 320, H = 240;
static const uint8_t MADCTL = 0x60;          // picture mirrored/upside down: try 0x20, 0xA0, 0xE0
static const bool INVERT = true;             // black shows as white: set false
static const uint32_t LCD_HZ = 62500000;     // the SDK rounds down to what the clock allows
static const uint32_t TP_HZ = 2000000;

// Two buffers, RGB565.
//   Normal mode   : one is sent while the other is drawn, then they swap.
//   Tear-free mode: frame[0] is always drawn. frame[1] holds the same picture turned to the
//                   screen's own scan order, and is the one sent.
static uint16_t frame[2][W * H];
static int dmaCh;
static bool sending = false;

// ---------- Frame timing, for development ----------
// Build with -DDECK_PROF=<app number> (-1 = home page). The deck then opens that app at
// start-up and prints the average time of each stage of a frame (microseconds) on USB serial,
// once a second. Without the flag, the marks compile to nothing.
enum { P_WAIT, P_PRESENT, P_APP, P_LOGIC, P_ROOM, P_PROPS, P_BUILD, P_CAT, P_FX, P_STATS, P_MSG, P_DOCK, P_COUNT };
#ifdef DECK_PROF
static uint32_t profAcc[P_COUNT], profT = 0;
#define PROF_START() (profT = micros())
#define PROF_MARK(i) do { uint32_t t_ = micros(); profAcc[i] += t_ - profT; profT = t_; } while (0)
#else
#define PROF_START() ((void)0)
#define PROF_MARK(i) ((void)0)
#endif

static constexpr uint16_t rgb(int r, int g, int b) {
  // 16-bit SPI frames go out high byte first, which is what the ST7789 wants. No byte swap.
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const uint16_t BLACK = 0x0000;
static const uint16_t WHITE = 0xFFFF;
static const uint16_t RED = rgb(255, 0, 0);
static const uint16_t GREY = rgb(70, 70, 70);
static const uint16_t LIGHTGREY = rgb(160, 160, 170);
static const uint16_t DARKGREY = rgb(30, 30, 40);


// ---------- SPI + DMA ----------
static void lcdCmd(uint8_t c, const uint8_t *d = nullptr, size_t n = 0) {
  gpio_put(PIN_CS, 0);
  gpio_put(PIN_DC, 0);
  spi_write_blocking(spi1, &c, 1);
  if (n) {
    gpio_put(PIN_DC, 1);
    spi_write_blocking(spi1, d, n);
  }
  gpio_put(PIN_CS, 1);
}

// Wait until the frame now being sent has fully left the SPI, then free the bus.
static void lcdWait() {
  if (!sending) return;
  dma_channel_wait_for_finish_blocking(dmaCh);
  while (spi_is_busy(spi1)) {}
  while (spi_is_readable(spi1)) (void)spi_get_hw(spi1)->dr;   // drop bytes clocked in during the send
  spi_get_hw(spi1)->icr = SPI_SSPICR_RORIC_BITS;
  gpio_put(PIN_CS, 1);
  spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  sending = false;
}

// Start sending a whole buffer by DMA. Returns at once. cols x rows is the order the
// pixels are stored in: 320 x 240 for a landscape buffer, 240 x 320 for the screen's own order.
static void lcdStart(const uint16_t *buf, int cols = W, int rows = H) {
  lcdWait();
  const uint8_t caset[] = {0, 0, (uint8_t)((cols - 1) >> 8), (uint8_t)((cols - 1) & 0xFF)};
  const uint8_t raset[] = {0, 0, (uint8_t)((rows - 1) >> 8), (uint8_t)((rows - 1) & 0xFF)};
  lcdCmd(0x2A, caset, 4);
  lcdCmd(0x2B, raset, 4);
  uint8_t c = 0x2C;
  gpio_put(PIN_CS, 0);
  gpio_put(PIN_DC, 0);
  spi_write_blocking(spi1, &c, 1);
  gpio_put(PIN_DC, 1);
  spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  dma_channel_set_read_addr(dmaCh, buf, false);
  dma_channel_set_trans_count(dmaCh, W * H, true);
  sending = true;
}


// ---------- Tear-free mode ----------
// The screen redraws itself line by line along its long side, about 60 times a second. A
// landscape picture is sent along the short side, so the two cross and a slanted line shows
// on anything that moves. The cure has three parts:
//   1. Turn the picture and send it in the screen's own line order.
//   2. Slow the screen's redraw a little, so a send is quicker than a redraw.
//   3. Ask the screen which line it is on, and start a send only when the send cannot
//      catch up with the redraw.
// Part 3 needs the screen to answer on the SPI bus. That is tested at start-up. If there is
// no answer, the old way of sending is used.
static const bool TEAR_FREE = true;          // false: never use this mode
static bool syncFound = false;               // the screen answers
static bool lcdSync = false;                 // tear-free mode is on
static int syncLines = 0;                    // scan lines in one redraw (about 344)
static int syncSafe = 0;                     // do not start a send before this line
static float syncHz = 0;                     // measured redraw rate
static uint8_t syncOnMosi = 0, syncSkip = 0; // which wire the answer comes on, and bits to skip

static inline void bbDelay() {
  for (volatile int i = 0; i < 3; i++) {}
}

// Send command c, then read `bits` bits of answer, by toggling the pins by hand. The ST7789
// answers too slowly for the fast clock, and some boards wire its answer to the MOSI pin.
// The first bit read ends up as the top bit of each result.
static void lcdReadRaw(uint8_t c, int bits, uint32_t &miso, uint32_t &mosi) {
  gpio_put(PIN_SCK, 0);
  gpio_set_dir(PIN_SCK, GPIO_OUT);
  gpio_set_dir(PIN_MOSI, GPIO_OUT);
  gpio_set_dir(PIN_MISO, GPIO_IN);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SIO);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SIO);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SIO);
  gpio_put(PIN_CS, 0);
  gpio_put(PIN_DC, 0);
  for (int b = 7; b >= 0; b--) {
    gpio_put(PIN_MOSI, (c >> b) & 1);
    bbDelay();
    gpio_put(PIN_SCK, 1);
    bbDelay();
    gpio_put(PIN_SCK, 0);
  }
  gpio_put(PIN_DC, 1);
  gpio_set_dir(PIN_MOSI, GPIO_IN);
  miso = mosi = 0;
  for (int b = 0; b < bits; b++) {
    bbDelay();
    gpio_put(PIN_SCK, 1);
    miso = (miso << 1) | (gpio_get(PIN_MISO) ? 1 : 0);
    mosi = (mosi << 1) | (gpio_get(PIN_MOSI) ? 1 : 0);
    bbDelay();
    gpio_put(PIN_SCK, 0);
  }
  gpio_put(PIN_CS, 1);
  gpio_set_dir(PIN_MOSI, GPIO_OUT);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
}

static const int SYNC_BITS = 26;

// the scan line as a number, from the raw bits of command 0x45 (-1 if it cannot be one)
static inline int syncValue(uint32_t raw, int skip) {
  uint32_t v = (raw >> (SYNC_BITS - 16 - skip)) & 0xFFFF;
  return v < 512 ? (int)v : -1;
}

#ifdef SIM_FORCE_SYNC
static int lcdScanLine() { return 200; }
#else
static int lcdScanLine() {
  uint32_t a, b;
  lcdReadRaw(0x45, SYNC_BITS, a, b);
  return syncValue(syncOnMosi ? b : a, syncSkip);
}
#endif

// Find out if the screen reports its scan line. A true answer climbs steadily with time and
// drops back to zero once per redraw. Noise does not. Sets syncFound, syncLines, syncHz.
static void lcdSyncProbe() {
#ifdef SIM_FORCE_SYNC
  syncFound = true;
  syncLines = 344;
  syncHz = 44.7f;
#else
  const int N = 96;
  static uint32_t us[N], rm[N], ro[N];
  for (int i = 0; i < N; i++) {
    lcdReadRaw(0x45, SYNC_BITS, rm[i], ro[i]);
    us[i] = micros();
    delayMicroseconds(300);
  }
  for (int pin = 0; pin < 2 && !syncFound; pin++)
    for (int skip = 0; skip <= 9 && !syncFound; skip++) {
      const uint32_t *raw = pin ? ro : rm;
      int good = 0, wraps = 0, top = 0, prev = syncValue(raw[0], skip);
      long lines = 0, span = 0;
      bool bad = prev < 0;
      for (int i = 1; i < N && !bad; i++) {
        int v = syncValue(raw[i], skip);
        if (v < 0) {
          bad = true;
          break;
        }
        int dv = v - prev;
        long dt = (long)(us[i] - us[i - 1]);
        if (dv < 0) wraps++;
        else if (dv * 1000L >= dt * 8 && dv * 1000L <= dt * 30) {   // 8..30 lines per ms
          good++;
          lines += dv;
          span += dt;
        }
        if (v > top) top = v;
        prev = v;
      }
      if (bad || wraps < 1 || wraps > 3 || good < N - 6 || top < 300 || top > 420 || span <= 0) continue;
      syncFound = true;
      syncOnMosi = pin;
      syncSkip = skip;
      syncLines = top + 1;
      syncHz = (float)lines / span * 1e6f / syncLines;
    }
#endif
  if (!syncFound) return;
  // a send must be quicker than a redraw, or the redraw would overtake it
  float sendS = (float)W * H * 16 / spi_get_baudrate(spi1);
  float ratio = sendS * syncHz;
  if (ratio > 0.96f) {
    syncFound = false;
    return;
  }
  syncSafe = 20 + (int)(W * (1 - ratio)) + 30;
}

// Hold until the redraw is at a line where a new send cannot cross it.
static void lcdSyncWait() {
  for (int tries = 0; tries < 60; tries++) {
    int line = lcdScanLine();
    if (line < 0 || line > syncLines + 20) return;            // no sense: do not wait
    if (line >= syncSafe && line < syncLines - 6) return;
    delayMicroseconds(150);
  }
}

// Landscape picture -> the screen's own order. Screen row r, column c = landscape (r, 239 - c).
static void lcdTurn(const uint16_t *src, uint16_t *dst) {
  for (int r = 0; r < W; r++) {
    const uint16_t *s = src + (H - 1) * W + r;
    for (int c = 0; c < H; c++, s -= W) *dst++ = *s;
  }
}

// Turn tear-free mode on or off. It stays off if the screen does not answer.
static void lcdSetSync(bool on) {
  on = on && syncFound && TEAR_FREE && MADCTL == 0x60;
  lcdWait();
  const uint8_t madctl = on ? 0x00 : MADCTL;
  const uint8_t rate = on ? 0x19 : 0x0F;       // 0x19 = about 45 redraws a second, 0x0F = 60
  lcdCmd(0x36, &madctl, 1);
  lcdCmd(0xC6, &rate, 1);
  lcdSync = on;
}

// Show the picture in fb. Returns the buffer to draw the next picture in.
static uint16_t *lcdPresent(uint16_t *fb) {
  if (!lcdSync) {
    lcdStart(fb);
    return fb == frame[0] ? frame[1] : frame[0];
  }
  lcdWait();
  if (fb != frame[0]) memcpy(frame[0], fb, sizeof(frame[0]));
  lcdTurn(frame[0], frame[1]);
  lcdSyncWait();
  lcdStart(frame[1], H, W);
  return frame[0];
}

// Show the picture in fb and wait until it is on the screen.
static void lcdShow(uint16_t *fb) {
  lcdPresent(fb);
  lcdWait();
}

static void setBacklight(int percent) {
  analogWrite(PIN_BL, percent * 255 / 100);
}

static void lcdBegin() {
  // SPI can run at half of clk_peri. Feed clk_peri from the CPU clock, not 48 MHz.
  uint32_t sys = clock_get_hz(clk_sys);
  if (clock_get_hz(clk_peri) != sys)
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, sys, sys);

  const uint outs[] = {PIN_DC, PIN_CS, PIN_RST, PIN_TP_CS};
  for (uint p : outs) {
    gpio_init(p);
    gpio_put(p, 1);
    gpio_set_dir(p, GPIO_OUT);
  }
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, LOW);
  gpio_init(PIN_TP_IRQ);
  gpio_set_dir(PIN_TP_IRQ, GPIO_IN);
  gpio_pull_up(PIN_TP_IRQ);

  spi_init(spi1, LCD_HZ);
  spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  // sharper edges for the fast clock
  gpio_set_slew_rate(PIN_SCK, GPIO_SLEW_RATE_FAST);
  gpio_set_slew_rate(PIN_MOSI, GPIO_SLEW_RATE_FAST);
  gpio_set_drive_strength(PIN_SCK, GPIO_DRIVE_STRENGTH_8MA);
  gpio_set_drive_strength(PIN_MOSI, GPIO_DRIVE_STRENGTH_8MA);

  dmaCh = dma_claim_unused_channel(true);
  dma_channel_config cfg = dma_channel_get_default_config(dmaCh);
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&cfg, true);
  channel_config_set_write_increment(&cfg, false);
  channel_config_set_dreq(&cfg, spi_get_dreq(spi1, true));
  dma_channel_configure(dmaCh, &cfg, &spi_get_hw(spi1)->dr, frame[0], 0, false);

  gpio_put(PIN_RST, 0);
  delay(50);
  gpio_put(PIN_RST, 1);
  delay(150);
  lcdCmd(0x01);
  delay(150);
  lcdCmd(0x11);
  delay(120);
  const uint8_t colmod = 0x55, madctl = MADCTL, rate = 0x19;
  lcdCmd(0x3A, &colmod, 1);
  lcdCmd(0x36, &madctl, 1);
  lcdCmd(INVERT ? 0x21 : 0x20);
  lcdCmd(0x13);
  lcdCmd(0xC6, &rate, 1);                      // slower redraw, needed for the test below
  lcdCmd(0x29);
  lcdShow(frame[0]);                           // buffers start black
  delay(40);
  if (TEAR_FREE) lcdSyncProbe();
  lcdSetSync(true);
}


// ---------- Colour helpers ----------
static inline uint16_t dim(uint16_t c) { return (c >> 1) & 0x7BEF; }        // half brightness
static inline uint16_t dim4(uint16_t c) { return (c >> 2) & 0x39E7; }       // quarter brightness

// mix a -> b by k (0..256)
static inline uint16_t blend(uint16_t a, uint16_t b, int k) {
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return ((ar + ((br - ar) * k >> 8)) << 11) | ((ag + ((bg - ag) * k >> 8)) << 5) | (ab + ((bb - ab) * k >> 8));
}

// hue 0..359, full saturation
static uint16_t hsv(int h, int v = 255) {
  h = ((h % 360) + 360) % 360;
  int x = (h % 60) * 255 / 60;
  int r, g, b;
  if (h < 60) { r = 255; g = x; b = 0; }
  else if (h < 120) { r = 255 - x; g = 255; b = 0; }
  else if (h < 180) { r = 0; g = 255; b = x; }
  else if (h < 240) { r = 0; g = 255 - x; b = 255; }
  else if (h < 300) { r = x; g = 0; b = 255; }
  else { r = 255; g = 0; b = 255 - x; }
  return rgb(r * v / 255, g * v / 255, b * v / 255);
}


// ---------- Shapes ----------
// Whole numbers name pixel centres. Curved and slanted edges are smoothed: an edge pixel
// is mixed with what is under it by how much of it the shape covers.

// The row window. A processor core draws only inside its own window of rows. Normally that
// is the whole screen. When the two cores draw one picture together (drawOnBothCores), each
// has a part of it. All the drawing functions of this file keep to the window.
#ifdef ARDUINO_ARCH_RP2040
#define THIS_CORE get_core_num()
#else
#define THIS_CORE 0
#endif
static int rowLo[2] = {0, 0}, rowHi[2] = {H, H};
static inline bool rowOk(int y) {
  int c = THIS_CORE;
  return y >= rowLo[c] && y < rowHi[c];
}

// Run draw(arg) on both cores at once: core 0 draws the rows above `split`, core 1 the rest.
// draw must only draw. It must not change any data, and all that it draws must go through
// the functions of this file (or look at rowOk itself).
struct SplitJob {
  void (*draw)(void *);
  void *arg;
  int split;
};
static void splitPart(void *job, int part) {
  const SplitJob *j = (const SplitJob *)job;
  int c = THIS_CORE;
  rowLo[c] = part ? j->split : 0;
  rowHi[c] = part ? H : j->split;
  j->draw(j->arg);
  rowLo[c] = 0;
  rowHi[c] = H;
}
// The result is how many microseconds core 0's part took more than core 1's (see evenShare).
static int drawOnBothCores(void (*draw)(void *), void *arg, int split) {
  SplitJob j = {draw, arg, split};
  return onBothCores(splitPart, &j);
}

static inline void pixel(uint16_t *fb, int x, int y, uint16_t col) {
  if ((unsigned)x < (unsigned)W && rowOk(y)) fb[y * W + x] = col;
}

// pixel mixed over the picture. a: 0 (nothing) .. 1 (solid)
static inline void pixelA(uint16_t *fb, int x, int y, uint16_t col, float a) {
  if ((unsigned)x >= (unsigned)W || !rowOk(y) || a <= 0.02f) return;
  uint16_t &p = fb[y * W + x];
  p = a >= 0.98f ? col : blend(p, col, (int)(a * 256));
}

static void fill(uint16_t *fb, uint16_t col) {
  int c = THIS_CORE;
  for (int i = rowLo[c] * W; i < rowHi[c] * W; i++) fb[i] = col;
}

static void fillRect(uint16_t *fb, int x, int y, int w, int h, uint16_t col) {
  const int c = THIS_CORE, lo = rowLo[c], hi = rowHi[c];
  if (x < 0) { w += x; x = 0; }
  if (y < lo) { h += y - lo; y = lo; }
  if (x + w > W) w = W - x;
  if (y + h > hi) h = hi - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) {
    uint16_t *p = fb + (y + j) * W + x;
    for (int i = 0; i < w; i++) p[i] = col;
  }
}

// darken a box so text on top of a busy background stays readable
static void shadeRect(uint16_t *fb, int x, int y, int w, int h, bool quarter = false) {
  const int c = THIS_CORE, lo = rowLo[c], hi = rowHi[c];
  if (x < 0) { w += x; x = 0; }
  if (y < lo) { h += y - lo; y = lo; }
  if (x + w > W) w = W - x;
  if (y + h > hi) h = hi - y;
  for (int j = 0; j < h; j++) {
    uint16_t *p = fb + (y + j) * W + x;
    for (int i = 0; i < w; i++) p[i] = quarter ? dim4(p[i]) : dim(p[i]);
  }
}

// the same, with round corners: for panels that float over a picture
static void shadeRoundRect(uint16_t *fb, int x, int y, int w, int h, int r) {
  for (int j = 0; j < h; j++) {
    int k = j < r ? j : (j >= h - r ? h - 1 - j : -1), in = 0;
    if (k >= 0) {
      float dy = r - 0.5f - k;
      in = (int)ceilf(r - 0.5f - sqrtf(fmaxf(0, r * r - dy * dy)));
    }
    shadeRect(fb, x + in, y + j, w - 2 * in, 1, true);
  }
}

static void hline(uint16_t *fb, int x, int y, int w, uint16_t col) { fillRect(fb, x, y, w, 1, col); }
static void vline(uint16_t *fb, int x, int y, int h, uint16_t col) { fillRect(fb, x, y, 1, h, col); }

static void rect(uint16_t *fb, int x, int y, int w, int h, uint16_t col) {
  hline(fb, x, y, w, col);
  hline(fb, x, y + h - 1, w, col);
  vline(fb, x, y, h, col);
  vline(fb, x + w - 1, y, h, col);
}

// Top-to-bottom colour fade. The steps between 16-bit colours are hidden with a fine
// dot pattern (ordered dither).
static void gradRect(uint16_t *fb, int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
  static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  int x0 = max(x, 0), x1 = min(x + w, W);
  int tr = (top >> 11) << 3, tg = ((top >> 5) & 63) << 2, tb = (top & 31) << 3;
  int br = (bottom >> 11) << 3, bg = ((bottom >> 5) & 63) << 2, bb = (bottom & 31) << 3;
  const int c = THIS_CORE;
  for (int j = max(0, rowLo[c] - y); j < min(h, rowHi[c] - y); j++) {
    int py = y + j;
    int k = h > 1 ? j * 256 / (h - 1) : 0;
    // colour in 1/16 steps of 8-bit, so the dither has something to work with
    int r = (tr * 16) + (br - tr) * k / 16, g = (tg * 16) + (bg - tg) * k / 16, b = (tb * 16) + (bb - tb) * k / 16;
    uint16_t pat[4];                           // the dots repeat every 4 pixels along a row
    for (int i = 0; i < 4; i++) {
      int d = BAYER[py & 3][i];
      int rr = min(255, (r + d * 8) >> 4), gg = min(255, (g + d * 4) >> 4), bl = min(255, (b + d * 8) >> 4);
      pat[i] = ((rr & 0xF8) << 8) | ((gg & 0xFC) << 3) | (bl >> 3);
    }
    uint16_t *p = fb + py * W + x0;
    int n = x1 - x0;
    for (int i = 0; i < n && i < 4; i++) p[i] = pat[(x0 + i) & 3];
    for (int done = 4; done < n; done *= 2) memcpy(p + done, p, min(done, n - done) * 2);   // copy the 4 along the row
  }
}

// Smooth line with round ends, `width` pixels wide.
static void strokeLine(uint16_t *fb, float x0, float y0, float x1, float y1, float width, uint16_t col) {
  float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy, hw = width * 0.5f;
  const int c = THIS_CORE, lo = rowLo[c], hi = rowHi[c];
  if (fmaxf(y0, y1) + hw + 2 < lo || fminf(y0, y1) - hw - 2 >= hi) return;   // no part of it is in the window
  const float ilen2 = len2 > 1e-6f ? 1 / len2 : 0;
  auto plot = [&](int px, int py) {
    float t = ((px - x0) * dx + (py - y0) * dy) * ilen2;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    float ex = px - (x0 + t * dx), ey = py - (y0 + t * dy);
    pixelA(fb, px, py, col, hw + 0.5f - sqrtf(ex * ex + ey * ey));
  };
  float ext = hw + 1;
  if (fabsf(dx) >= fabsf(dy)) {                // walk along x, look at a few pixels across
    float xa = fminf(x0, x1), xb = fmaxf(x0, x1), slope = fabsf(dx) > 1e-6f ? dy / dx : 0;
    float reach = ext * sqrtf(1 + slope * slope) + 1;
    int pa = max(0, (int)floorf(xa - ext)), pb = min(W - 1, (int)ceilf(xb + ext));
    if (fabsf(slope) > 0.01f) {                // only the columns where the line is near the rows of the window
      float u = x0 + (lo - reach - 1 - y0) / slope, v = x0 + (hi + reach - y0) / slope;
      pa = max(pa, (int)floorf(fminf(u, v) - ext - 2));
      pb = min(pb, (int)ceilf(fmaxf(u, v) + ext + 2));
    }
    for (int px = pa; px <= pb; px++) {
      float cx = px < xa ? xa : (px > xb ? xb : px);
      float yc = y0 + (cx - x0) * slope;
      for (int py = max(lo, (int)floorf(yc - reach)); py <= min(hi - 1, (int)ceilf(yc + reach)); py++) plot(px, py);
    }
  } else {
    float ya = fminf(y0, y1), yb = fmaxf(y0, y1), slope = dx / dy;
    float reach = ext * sqrtf(1 + slope * slope) + 1;
    for (int py = max(lo, (int)floorf(ya - ext)); py <= min(hi - 1, (int)ceilf(yb + ext)); py++) {
      float cy = py < ya ? ya : (py > yb ? yb : py);
      float xc = x0 + (cy - y0) * slope;
      for (int px = (int)floorf(xc - reach); px <= (int)ceilf(xc + reach); px++) plot(px, py);
    }
  }
}

static void line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t col) {
  if (y0 == y1) hline(fb, min(x0, x1), y0, abs(x1 - x0) + 1, col);
  else if (x0 == x1) vline(fb, x0, min(y0, y1), abs(y1 - y0) + 1, col);
  else strokeLine(fb, x0, y0, x1, y1, 1.1f, col);
}

static void thickLine(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t col) {
  strokeLine(fb, x0 + 0.5f, y0 + 0.5f, x1 + 0.5f, y1 + 0.5f, 2.2f, col);
}

static void fillCircle(uint16_t *fb, int cx, int cy, int r, uint16_t col) {
  const float R = r + 0.5f;
  const int c = THIS_CORE;
  for (int dy = max(-r - 1, rowLo[c] - cy); dy <= min(r + 1, rowHi[c] - 1 - cy); dy++) {
    float out2 = (R + 0.5f) * (R + 0.5f) - dy * dy;
    if (out2 <= 0) continue;
    float in2 = (R - 0.5f) * (R - 0.5f) - dy * dy;
    int xo = (int)sqrtf(out2), xi = in2 > 0 ? (int)sqrtf(in2) : -1;
    if (xi >= 0) hline(fb, cx - xi, cy + dy, 2 * xi + 1, col);
    for (int dx = xi + 1; dx <= xo; dx++) {
      float a = R + 0.5f - sqrtf((float)(dx * dx + dy * dy));
      pixelA(fb, cx + dx, cy + dy, col, a);
      if (dx) pixelA(fb, cx - dx, cy + dy, col, a);
    }
  }
}

// circle outline, 1 px
static void circle(uint16_t *fb, int cx, int cy, int r, uint16_t col) {
  const int c = THIS_CORE;
  for (int dy = max(-r - 1, rowLo[c] - cy); dy <= min(r + 1, rowHi[c] - 1 - cy); dy++)
    for (int dx = -r - 1; dx <= r + 1; dx++) {
      float d = fabsf(sqrtf((float)(dx * dx + dy * dy)) - r);
      if (d < 1) pixelA(fb, cx + dx, cy + dy, col, 1 - d);
    }
}

static void fillRoundRect(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t col) {
  if (w <= 0 || h <= 0) return;
  r = max(0, min(r, min(w, h) / 2));
  fillRect(fb, x, y + r, w, h - 2 * r, col);
  for (int j = 0; j < r; j++) {
    if (!rowOk(y + j) && !rowOk(y + h - 1 - j)) continue;
    float dy = r - 0.5f - j;                   // this row's distance from the corner centre
    float out2 = (r + 0.5f) * (r + 0.5f) - dy * dy, in2 = (r - 0.5f) * (r - 0.5f) - dy * dy;
    // i counts pixels in from the left edge. Its distance from the corner centre is r - 0.5 - i
    int solid = in2 > 0 ? (int)ceilf(r - 0.5f - sqrtf(in2)) : r;
    int first = out2 > 0 ? max(0, (int)floorf(r - 0.5f - sqrtf(out2))) : r;
    solid = max(0, min(solid, r));
    hline(fb, x + solid, y + j, w - 2 * solid, col);
    hline(fb, x + solid, y + h - 1 - j, w - 2 * solid, col);
    for (int i = first; i < solid; i++) {
      float dx = r - 0.5f - i;
      float a = r + 0.5f - sqrtf(dx * dx + dy * dy);
      pixelA(fb, x + i, y + j, col, a);
      pixelA(fb, x + w - 1 - i, y + j, col, a);
      pixelA(fb, x + i, y + h - 1 - j, col, a);
      pixelA(fb, x + w - 1 - i, y + h - 1 - j, col, a);
    }
  }
}

// rounded box with a top-to-bottom colour fade
static void fillRoundRectV(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t top, uint16_t bottom) {
  if (w <= 0 || h <= 0) return;
  r = max(0, min(r, min(w, h) / 2));
  for (int j = 0; j < h; j++) {
    if (!rowOk(y + j)) continue;
    uint16_t col = blend(top, bottom, h > 1 ? j * 256 / (h - 1) : 0);
    int k = j < r ? j : (j >= h - r ? h - 1 - j : -1);   // row inside a corner, or -1
    if (k < 0) {
      hline(fb, x, y + j, w, col);
      continue;
    }
    float dy = r - 0.5f - k;
    float out2 = (r + 0.5f) * (r + 0.5f) - dy * dy, in2 = (r - 0.5f) * (r - 0.5f) - dy * dy;
    int solid = in2 > 0 ? (int)ceilf(r - 0.5f - sqrtf(in2)) : r;
    int first = out2 > 0 ? max(0, (int)floorf(r - 0.5f - sqrtf(out2))) : r;
    solid = max(0, min(solid, r));
    hline(fb, x + solid, y + j, w - 2 * solid, col);
    for (int i = first; i < solid; i++) {
      float dx = r - 0.5f - i;
      float a = r + 0.5f - sqrtf(dx * dx + dy * dy);
      pixelA(fb, x + i, y + j, col, a);
      pixelA(fb, x + w - 1 - i, y + j, col, a);
    }
  }
}

// rounded outline, 1 px
static void roundRect(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t col) {
  if (w <= 0 || h <= 0) return;
  r = max(0, min(r, min(w, h) / 2));
  hline(fb, x + r, y, w - 2 * r, col);
  hline(fb, x + r, y + h - 1, w - 2 * r, col);
  vline(fb, x, y + r, h - 2 * r, col);
  vline(fb, x + w - 1, y + r, h - 2 * r, col);
  for (int j = 0; j < r; j++)
    for (int i = 0; i < r; i++) {
      float dx = r - 0.5f - i, dy = r - 0.5f - j;
      float d = fabsf(sqrtf(dx * dx + dy * dy) - (r - 0.5f));
      if (d >= 1) continue;
      pixelA(fb, x + i, y + j, col, 1 - d);
      pixelA(fb, x + w - 1 - i, y + j, col, 1 - d);
      pixelA(fb, x + i, y + h - 1 - j, col, 1 - d);
      pixelA(fb, x + w - 1 - i, y + h - 1 - j, col, 1 - d);
    }
}

static void fillTriangle(uint16_t *fb, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t col) {
  const float vx[3] = {(float)x0, (float)x1, (float)x2}, vy[3] = {(float)y0, (float)y1, (float)y2};
  int minY = min(y0, min(y1, y2)), maxY = max(y0, max(y1, y2));
  const int c = THIS_CORE;
  for (int py = max(minY, rowLo[c]); py <= min(maxY, rowHi[c] - 1); py++) {
    float l[2], r[2];
    bool ok[2];
    for (int k = 0; k < 2; k++) {              // two heights per row, for smooth slanted edges
      float ys = fminf(fmaxf(py - 0.25f + 0.5f * k, (float)minY), (float)maxY);
      l[k] = 1e9f;
      r[k] = -1e9f;
      for (int e = 0; e < 3; e++) {
        float ax = vx[e], ay = vy[e], bx = vx[(e + 1) % 3], by = vy[(e + 1) % 3];
        if (ay == by) {
          if (ys != ay) continue;
          l[k] = fminf(l[k], fminf(ax, bx));
          r[k] = fmaxf(r[k], fmaxf(ax, bx));
        } else if (ys >= fminf(ay, by) && ys <= fmaxf(ay, by)) {
          float xx = ax + (ys - ay) * (bx - ax) / (by - ay);
          l[k] = fminf(l[k], xx);
          r[k] = fmaxf(r[k], xx);
        }
      }
      ok[k] = r[k] >= l[k];
      l[k] -= 0.5f;                            // the shape reaches the far side of its edge pixels
      r[k] += 0.5f;
    }
    if (!ok[0] && !ok[1]) continue;
    float lo = fminf(ok[0] ? l[0] : 1e9f, ok[1] ? l[1] : 1e9f), hi = fmaxf(ok[0] ? r[0] : -1e9f, ok[1] ? r[1] : -1e9f);
    for (int px = max(0, (int)floorf(lo)); px <= min(W - 1, (int)ceilf(hi)); px++) {
      float a = 0;
      for (int k = 0; k < 2; k++)
        if (ok[k]) {
          float c = fminf(r[k], px + 0.5f) - fmaxf(l[k], px - 0.5f);
          a += c < 0 ? 0 : (c > 1 ? 1 : c);
        }
      pixelA(fb, px, py, col, a * 0.5f);
    }
  }
}

// atan2f for drawing: a polynomial, much quicker than the library, and right to 0.0001 rad.
static inline float atan2Fast(float y, float x) {
  float ax = fabsf(x), ay = fabsf(y), mx = fmaxf(ax, ay);
  if (mx == 0) return 0;
  float z = fminf(ax, ay) / mx, z2 = z * z;
  float a = z * (0.99997726f + z2 * (-0.33262347f + z2 * (0.19354346f + z2 * (-0.11643287f + z2 * (0.05265332f + z2 * -0.01172120f)))));
  if (ay > ax) a = 1.5707963f - a;
  if (x < 0) a = 3.1415927f - a;
  return y < 0 ? -a : a;
}

// Ring segment from angle a0 to a1 (radians, 0 = up, clockwise), radii r0..r1.
// col0 -> col1 colour gradient along the arc.
static void arc(uint16_t *fb, int cx, int cy, int r0, int r1, float a0, float a1, uint16_t col0, uint16_t col1) {
  const float TWO_PI_F = 6.2831853f;
  float span = a1 - a0;
  if (span <= 0) return;
  bool whole = col0 == col1 && a0 <= 0 && a1 >= TWO_PI_F;   // a full ring in one colour needs no angles
  // 4 x (distance squared), in whole numbers: no ring at all outside lo..hi, solid ring inside slo..shi
  const int lo = r0 > 0 ? (2 * r0 - 1) * (2 * r0 - 1) : -1, hi = (2 * r1 + 1) * (2 * r1 + 1);
  const int slo = (2 * r0 + 1) * (2 * r0 + 1), shi = (2 * r1 - 1) * (2 * r1 - 1);
  for (int y = -r1 - 1; y <= r1 + 1; y++) {
    if (!rowOk(cy + y)) continue;
    for (int x = -r1 - 1; x <= r1 + 1; x++) {
      int d4 = 4 * (x * x + y * y);
      if (d4 <= lo || d4 >= hi) continue;
      float edge = 1;                                    // > 0 inside the ring
      if (d4 < slo || d4 > shi) {
        float d = sqrtf((float)(x * x + y * y));
        edge = fminf(d - r0, r1 - d) + 0.5f;
      }
      if (whole) { pixelA(fb, cx + x, cy + y, col0, edge); continue; }
      float a = atan2Fast((float)x, (float)-y);
      if (a < 0) a += TWO_PI_F;
      float k = (a - a0) / span;
      if (k < 0 || k > 1) continue;
      pixelA(fb, cx + x, cy + y, blend(col0, col1, (int)(k * 256)), edge);
    }
  }
}


// ---------- Text ----------
// Inter, with smooth edges. y is the baseline.
static const Font *const TINY = &TINY_FONT;      // captions, 8 px capitals
static const Font *const SMALL = &SMALL_FONT;    // body text, 11 px capitals
static const Font *const MEDIUM = &MEDIUM_FONT;  // titles, 14 px capitals
static const Font *const LARGE = &LARGE_FONT;    // big numbers, 21 px capitals
static const Font *const GIANT = &GIANT_FONT;    // digits and ':' only, 34 px
static const Font *const CLOCKFACE = &CLOCK_FONT; // digits and ':' only, 76 px

static int textWidth(const Font *f, const char *s) {
  int w = 0;
  for (; *s; s++) {
    uint8_t c = *s;
    if (c < f->first || c > f->last) continue;
    w += f->glyph[c - f->first].adv;
  }
  return w;
}

static int text(uint16_t *fb, const Font *f, const char *s, int x, int y, uint16_t col) {
  for (; *s; s++) {
    uint8_t c = *s;
    if (c < f->first || c > f->last) continue;
    const Glyph &g = f->glyph[c - f->first];
    const uint8_t *bm = f->bits + g.off;
    int n = 0;
    for (int yy = 0; yy < g.h; yy++) {
      int py = y + g.yo + yy;
      if (!rowOk(py)) {
        n += g.w;
        continue;
      }
      uint16_t *row = fb + py * W;
      for (int xx = 0; xx < g.w; xx++, n++) {
        int a = (n & 1) ? (bm[n >> 1] & 15) : (bm[n >> 1] >> 4);
        int px = x + g.xo + xx;
        if (!a || (unsigned)px >= (unsigned)W) continue;
        row[px] = a == 15 ? col : blend(row[px], col, a * 17);
      }
    }
    x += g.adv;
  }
  return x;
}

static void textCenter(uint16_t *fb, const Font *f, const char *s, int cx, int y, uint16_t col) {
  text(fb, f, s, cx - textWidth(f, s) / 2, y, col);
}

static void textRight(uint16_t *fb, const Font *f, const char *s, int rx, int y, uint16_t col) {
  text(fb, f, s, rx - textWidth(f, s), y, col);
}

// Text broken into lines no wider than maxW. y is the first baseline. Returns the baseline
// after the last line. A '\n' starts a new line.
static int textWrap(uint16_t *fb, const Font *f, const char *s, int x, int y, int maxW, int lineH, uint16_t col) {
  char buf[64];
  while (*s) {
    int n = 0, lastSpace = -1, w = 0;
    while (s[n] && s[n] != '\n' && n < (int)sizeof(buf) - 1) {
      uint8_t c = s[n];
      int adv = (c >= f->first && c <= f->last) ? f->glyph[c - f->first].adv : 0;
      if (w + adv > maxW && lastSpace >= 0) break;
      if (c == ' ') lastSpace = n;
      w += adv;
      n++;
    }
    bool full = s[n] && s[n] != '\n';
    int take = (full && lastSpace >= 0) ? lastSpace : n;
    memcpy(buf, s, take);
    buf[take] = 0;
    text(fb, f, buf, x, y, col);
    y += lineH;
    s += take;
    while (*s == ' ') s++;
    if (*s == '\n') s++;
  }
  return y;
}

// Caption text. y is the top. scale 2 and 3 pick the bigger fonts.
static const Font *tinyFont(int scale) { return scale <= 1 ? TINY : (scale == 2 ? MEDIUM : LARGE); }

static void tiny(uint16_t *fb, const char *s, int x, int y, uint16_t col, int scale = 1) {
  const Font *f = tinyFont(scale);
  text(fb, f, s, x, y + f->cap - 1, col);
}

static int tinyWidth(const char *s, int scale = 1) { return textWidth(tinyFont(scale), s); }

static void tinyCenter(uint16_t *fb, const char *s, int cx, int y, uint16_t col, int scale = 1) {
  tiny(fb, s, cx - tinyWidth(s, scale) / 2, y, col, scale);
}
