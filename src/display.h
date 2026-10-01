// display.h
// ST7789 320x240 over SPI1 with DMA, two frame buffers, and drawing helpers.
// Waveshare Pico-ResTouch-LCD-2.8 on a Pico 2 W.
#pragma once

#include <hardware/spi.h>
#include <hardware/dma.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include "fonts/gfxfont.h"
#include "fonts/glcdfont.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSansBold12pt7b.h"
#include "fonts/FreeSansBold18pt7b.h"

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

static uint16_t frame[2][W * H];             // RGB565. One is sent while the other is drawn
static int dmaCh;
static bool sending = false;

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

// Start sending a whole frame by DMA. Returns at once.
static void lcdStart(const uint16_t *buf) {
  lcdWait();
  static const uint8_t caset[] = {0, 0, (W - 1) >> 8, (W - 1) & 0xFF};
  static const uint8_t raset[] = {0, 0, (H - 1) >> 8, (H - 1) & 0xFF};
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

static void lcdShow(const uint16_t *buf) {
  lcdStart(buf);
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
  const uint8_t colmod = 0x55, madctl = MADCTL;
  lcdCmd(0x3A, &colmod, 1);
  lcdCmd(0x36, &madctl, 1);
  lcdCmd(INVERT ? 0x21 : 0x20);
  lcdCmd(0x13);
  lcdCmd(0x29);
  lcdShow(frame[0]);                           // buffers start black
  delay(20);
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
static inline void pixel(uint16_t *fb, int x, int y, uint16_t col) {
  if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = col;
}

static void fill(uint16_t *fb, uint16_t col) {
  for (int i = 0; i < W * H; i++) fb[i] = col;
}

static void fillRect(uint16_t *fb, int x, int y, int w, int h, uint16_t col) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) {
    uint16_t *p = fb + (y + j) * W + x;
    for (int i = 0; i < w; i++) p[i] = col;
  }
}

// darken a box so text on top of a busy background stays readable
static void shadeRect(uint16_t *fb, int x, int y, int w, int h, bool quarter = false) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  for (int j = 0; j < h; j++) {
    uint16_t *p = fb + (y + j) * W + x;
    for (int i = 0; i < w; i++) p[i] = quarter ? dim4(p[i]) : dim(p[i]);
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

static void line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t col) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    pixel(fb, x0, y0, col);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void thickLine(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t col) {
  line(fb, x0, y0, x1, y1, col);
  line(fb, x0 + 1, y0, x1 + 1, y1, col);
  line(fb, x0, y0 + 1, x1, y1 + 1, col);
}

static void fillCircle(uint16_t *fb, int cx, int cy, int r, uint16_t col) {
  for (int y = -r; y <= r; y++) {
    int w = (int)sqrtf((float)(r * r - y * y));
    hline(fb, cx - w, cy + y, 2 * w + 1, col);
  }
}

static void circle(uint16_t *fb, int cx, int cy, int r, uint16_t col) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    pixel(fb, cx + x, cy + y, col); pixel(fb, cx - x, cy + y, col);
    pixel(fb, cx + x, cy - y, col); pixel(fb, cx - x, cy - y, col);
    pixel(fb, cx + y, cy + x, col); pixel(fb, cx - y, cy + x, col);
    pixel(fb, cx + y, cy - x, col); pixel(fb, cx - y, cy - x, col);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

static void fillRoundRect(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t col) {
  fillRect(fb, x, y + r, w, h - 2 * r, col);
  for (int j = 0; j < r; j++) {
    int d = r - (int)sqrtf((float)(r * r - (r - j) * (r - j)));
    hline(fb, x + d, y + j, w - 2 * d, col);
    hline(fb, x + d, y + h - 1 - j, w - 2 * d, col);
  }
}

static void roundRect(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t col) {
  hline(fb, x + r, y, w - 2 * r, col);
  hline(fb, x + r, y + h - 1, w - 2 * r, col);
  vline(fb, x, y + r, h - 2 * r, col);
  vline(fb, x + w - 1, y + r, h - 2 * r, col);
  for (int j = 0; j < r; j++) {
    int d = r - (int)sqrtf((float)(r * r - (r - j) * (r - j)));
    int d2 = r - (int)sqrtf((float)(r * r - (r - j - 1) * (r - j - 1)));
    for (int k = d2; k <= d; k++) {
      pixel(fb, x + k, y + j, col); pixel(fb, x + w - 1 - k, y + j, col);
      pixel(fb, x + k, y + h - 1 - j, col); pixel(fb, x + w - 1 - k, y + h - 1 - j, col);
    }
  }
}

static void fillTriangle(uint16_t *fb, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t col) {
  int minY = min(y0, min(y1, y2)), maxY = max(y0, max(y1, y2));
  for (int y = minY; y <= maxY; y++) {
    float xs[3];
    int n = 0;
    const int px[3] = {x0, x1, x2}, py[3] = {y0, y1, y2};
    for (int e = 0; e < 3 && n < 3; e++) {
      int ax = px[e], ay = py[e], bx = px[(e + 1) % 3], by = py[(e + 1) % 3];
      if ((y >= ay && y < by) || (y >= by && y < ay) || (y == maxY && (y == ay || y == by) && ay != by))
        xs[n++] = ax + (float)(y - ay) * (bx - ax) / (by - ay);
    }
    if (n >= 2) {
      int a = (int)fminf(xs[0], xs[1]), b = (int)fmaxf(xs[0], xs[1]);
      hline(fb, a, y, b - a + 1, col);
    }
  }
}

// Ring segment from angle a0 to a1 (radians, 0 = up, clockwise), radii r0..r1.
// col0 -> col1 colour gradient along the arc.
static void arc(uint16_t *fb, int cx, int cy, int r0, int r1, float a0, float a1, uint16_t col0, uint16_t col1) {
  const float TWO_PI_F = 6.2831853f;
  float span = a1 - a0;
  if (span <= 0) return;
  for (int y = -r1; y <= r1; y++) {
    for (int x = -r1; x <= r1; x++) {
      int d2 = x * x + y * y;
      if (d2 > r1 * r1 || d2 < r0 * r0) continue;
      float a = atan2f((float)x, (float)-y);
      if (a < 0) a += TWO_PI_F;
      float k = (a - a0) / span;
      if (k < 0 || k > 1) continue;
      pixel(fb, cx + x, cy + y, blend(col0, col1, (int)(k * 256)));
    }
  }
}


// ---------- Text ----------
// FreeSans fonts. y is the baseline.
static const GFXfont *SMALL = &FreeSans9pt7b;
static const GFXfont *MEDIUM = &FreeSansBold12pt7b;
static const GFXfont *LARGE = &FreeSansBold18pt7b;

static int textWidth(const GFXfont *f, const char *s) {
  int w = 0;
  for (; *s; s++) {
    uint8_t c = *s;
    if (c < f->first || c > f->last) continue;
    w += f->glyph[c - f->first].xAdvance;
  }
  return w;
}

static int text(uint16_t *fb, const GFXfont *f, const char *s, int x, int y, uint16_t col) {
  for (; *s; s++) {
    uint8_t c = *s;
    if (c < f->first || c > f->last) continue;
    const GFXglyph &g = f->glyph[c - f->first];
    const uint8_t *bm = f->bitmap + g.bitmapOffset;
    int bit = 0;
    uint8_t bits = 0;
    for (int yy = 0; yy < g.height; yy++)
      for (int xx = 0; xx < g.width; xx++) {
        if (!(bit++ & 7)) bits = *bm++;
        if (bits & 0x80) pixel(fb, x + g.xOffset + xx, y + g.yOffset + yy, col);
        bits <<= 1;
      }
    x += g.xAdvance;
  }
  return x;
}

static void textCenter(uint16_t *fb, const GFXfont *f, const char *s, int cx, int y, uint16_t col) {
  text(fb, f, s, cx - textWidth(f, s) / 2, y, col);
}

static void textRight(uint16_t *fb, const GFXfont *f, const char *s, int rx, int y, uint16_t col) {
  text(fb, f, s, rx - textWidth(f, s), y, col);
}

// Classic 5x7 font. y is the top. Each character is 6 * scale pixels wide.
static void tiny(uint16_t *fb, const char *s, int x, int y, uint16_t col, int scale = 1) {
  for (; *s; s++, x += 6 * scale) {
    const uint8_t *g = font + (uint8_t)*s * 5;
    for (int c = 0; c < 5; c++)
      for (int r = 0; r < 8; r++)
        if (g[c] & (1 << r)) {
          if (scale == 1) pixel(fb, x + c, y + r, col);
          else fillRect(fb, x + c * scale, y + r * scale, scale, scale, col);
        }
  }
}

static int tinyWidth(const char *s, int scale = 1) { return strlen(s) * 6 * scale; }

static void tinyCenter(uint16_t *fb, const char *s, int cx, int y, uint16_t col, int scale = 1) {
  tiny(fb, s, cx - tinyWidth(s, scale) / 2 + scale / 2, y, col, scale);
}

// Seven-segment digit with a soft halo. w x h box, t = segment thickness.
// ch: '0'..'9', '-', ' ' or ':'.
static void seg7(uint16_t *fb, char ch, int x, int y, int w, int h, int t, uint16_t col) {
  static const uint8_t SEGS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  uint16_t halo = dim4(col);
  if (ch == ':') {
    fillCircle(fb, x + w / 2, y + h / 3, t / 2 + 2, halo);
    fillCircle(fb, x + w / 2, y + 2 * h / 3, t / 2 + 2, halo);
    fillCircle(fb, x + w / 2, y + h / 3, t / 2, col);
    fillCircle(fb, x + w / 2, y + 2 * h / 3, t / 2, col);
    return;
  }
  uint8_t s = ch == '-' ? 0x40 : (ch >= '0' && ch <= '9') ? SEGS[ch - '0'] : 0;
  int m = h / 2;
  // a b c d e f g: x, y, w, h of each segment
  const int box[7][4] = {
    {x + t, y, w - 2 * t, t},                 // a top
    {x + w - t, y + t, t, m - t - t / 2},     // b top right
    {x + w - t, y + m + t / 2, t, m - t - t / 2},  // c bottom right
    {x + t, y + h - t, w - 2 * t, t},         // d bottom
    {x, y + m + t / 2, t, m - t - t / 2},     // e bottom left
    {x, y + t, t, m - t - t / 2},             // f top left
    {x + t, y + m - t / 2, w - 2 * t, t},     // g middle
  };
  for (int i = 0; i < 7; i++) {
    if (!(s & (1 << i))) {
      fillRect(fb, box[i][0], box[i][1], box[i][2], box[i][3], rgb(14, 14, 20));   // unlit ghost segment
      continue;
    }
    fillRect(fb, box[i][0] - 2, box[i][1] - 2, box[i][2] + 4, box[i][3] + 4, halo);
  }
  for (int i = 0; i < 7; i++)
    if (s & (1 << i)) fillRect(fb, box[i][0], box[i][1], box[i][2], box[i][3], col);
}

// String of seven-segment characters. Returns the width used.
static int seg7Text(uint16_t *fb, const char *s, int x, int y, int w, int h, int t, uint16_t col) {
  int x0 = x;
  for (; *s; s++) {
    if (*s == ':') {
      seg7(fb, ':', x, y, t * 2, h, t, col);
      x += t * 2 + t;
    } else {
      seg7(fb, *s, x, y, w, h, t, col);
      x += w + t + 2;
    }
  }
  return x - x0;
}

static int seg7Width(const char *s, int w, int t) {
  int x = 0;
  for (; *s; s++) x += *s == ':' ? t * 3 : w + t + 2;
  return x;
}
