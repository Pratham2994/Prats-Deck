// app_scope.h
// Oscilloscope on GP26 (ADC0), 0 to 3.3 V, up to 500,000 samples per second.
// Phosphor-style glowing trace, rising-edge trigger, Vpp / average / frequency readouts.
// Try it: touch the GP26 pin with a finger to see mains hum (50/60 Hz),
// or turn on TEST and wire GP0 to GP26 to see a 1 kHz square wave.
#pragma once

#include <hardware/adc.h>
#include <hardware/pwm.h>
#include "core.h"

namespace scope {

static const int PIN_IN = 26, PIN_TEST = 0;
static const int NS = 600;                   // samples per capture
static const int SHOW = 300;                 // samples shown, 1 per pixel
static const uint32_t RATES[] = {500000, 200000, 100000, 50000, 20000, 10000, 5000, 2000, 1000};
static const int NRATES = sizeof(RATES) / sizeof(RATES[0]);
static const int PX = 10, PY = 24, PW = 300, PH = 160;   // plot area. 10 x 8 divisions

struct State {
  uint16_t buf[NS];          // DMA writes here
  uint16_t trace[SHOW];      // last trace shown
  bool haveTrace;
  float vmin, vmax, vavg, freq;
  bool capturing;
  bool hold;
  bool test;
  int rate;
  int dma;
};
APP_STATE(State, S)

static const uint16_t GREEN = rgb(90, 255, 130);

static void startCapture() {
  adc_run(false);
  adc_fifo_drain();
  adc_set_clkdiv(48000000.0f / RATES[S.rate] - 1);
  dma_channel_config c = dma_channel_get_default_config(S.dma);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
  channel_config_set_read_increment(&c, false);
  channel_config_set_write_increment(&c, true);
  channel_config_set_dreq(&c, DREQ_ADC);
  dma_channel_configure(S.dma, &c, S.buf, &adc_hw->fifo, NS, true);
  adc_run(true);
  S.capturing = true;
}

static void process() {
  adc_run(false);
  adc_fifo_drain();
  int lo = 4095, hi = 0;
  long sum = 0;
  for (int i = 0; i < NS; i++) {
    int v = S.buf[i] & 0x0FFF;
    lo = min(lo, v);
    hi = max(hi, v);
    sum += v;
  }
  // trigger: first rising crossing of the middle level, with a little hysteresis
  int mid = (lo + hi) / 2, hyst = max(8, (hi - lo) / 10), start = 0;
  bool armed = false;
  for (int i = 0; i < NS - SHOW; i++) {
    int v = S.buf[i] & 0x0FFF;
    if (v < mid - hyst) armed = true;
    if (armed && v >= mid) {
      start = i;
      break;
    }
  }
  for (int i = 0; i < SHOW; i++) S.trace[i] = S.buf[start + i] & 0x0FFF;
  // frequency from rising crossings over the whole capture
  int crossings = 0, first = -1, last = -1;
  armed = false;
  for (int i = 0; i < NS; i++) {
    int v = S.buf[i] & 0x0FFF;
    if (v < mid - hyst) armed = true;
    if (armed && v >= mid + hyst) {
      armed = false;
      if (first < 0) first = i;
      last = i;
      crossings++;
    }
  }
  S.freq = (crossings >= 2 && hi - lo > 40) ? (crossings - 1) * (float)RATES[S.rate] / (last - first) : 0;
  S.vmin = lo * 3.3f / 4095;
  S.vmax = hi * 3.3f / 4095;
  S.vavg = sum / (float)NS * 3.3f / 4095;
  S.haveTrace = true;
  S.capturing = false;
}

static void setTest(bool on) {
  S.test = on;
  if (on) {
    gpio_set_function(PIN_TEST, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_TEST);
    pwm_set_clkdiv(slice, clock_get_hz(clk_sys) / 1000000.0f);   // 1 MHz counter
    pwm_set_wrap(slice, 999);                                     // 1 kHz
    pwm_set_gpio_level(PIN_TEST, 500);
    pwm_set_enabled(slice, true);
  } else {
    pwm_set_enabled(pwm_gpio_to_slice_num(PIN_TEST), false);
    gpio_deinit(PIN_TEST);
  }
}

static void enter() {
  memset(&S, 0, sizeof(S));
  S.rate = 6;                                  // 5 kHz: shows mains hum well
  adc_init();
  adc_gpio_init(PIN_IN);
  adc_select_input(PIN_IN - 26);
  adc_fifo_setup(true, true, 1, false, false);
  S.dma = dma_claim_unused_channel(true);
}

static void fmtTime(char *s, int n, float sec) {
  if (sec < 1e-3f) snprintf(s, n, "%.0f us", sec * 1e6f);
  else snprintf(s, n, "%.1f ms", sec * 1e3f);
}

static void frame(uint16_t *fb, float dt, uint32_t /*now*/) {
  if (S.capturing && !dma_channel_is_busy(S.dma)) process();
  if (!S.capturing && !S.hold) startCapture();

  // trace into the glow layer = phosphor afterglow
  auto yOf = [](int v) { return PY + PH - 1 - v * (PH - 1) / 4095; };
  if (S.haveTrace && !S.hold)
    for (int i = 1; i < SHOW; i++)
      glowLine(PX + i - 1, yOf(S.trace[i - 1]), PX + i, yOf(S.trace[i]), 70 * 256);
  glowRender(fb, pal[PAL_PHOSPHOR], dt, 0.05f, 30);

  // grid
  for (int k = 0; k <= 10; k++) vline(fb, PX + k * PW / 10, PY, PH, k == 5 ? rgb(40, 70, 50) : rgb(22, 40, 28));
  for (int k = 0; k <= 8; k++) hline(fb, PX, PY + k * PH / 8, PW, k == 4 ? rgb(40, 70, 50) : rgb(22, 40, 28));
  if (S.haveTrace)
    for (int i = 1; i < SHOW; i++) line(fb, PX + i - 1, yOf(S.trace[i - 1]), PX + i, yOf(S.trace[i]), WHITE);

  appHeader(fb, "Scope GP26", GREEN);
  char s[48], t[16];
  if (S.haveTrace) {
    if (S.freq > 0) {
      if (S.freq >= 1000) snprintf(t, sizeof(t), "%.2f kHz", S.freq / 1000);
      else snprintf(t, sizeof(t), "%.1f Hz", S.freq);
    } else strcpy(t, "-- Hz");
    snprintf(s, sizeof(s), "Vpp %.2f  Avg %.2f  %s", S.vmax - S.vmin, S.vavg, t);
    tiny(fb, s, W - 6 - tinyWidth(s), 8, WHITE);
  }
  tiny(fb, "3.3V", PX + 2, PY + 2, LIGHTGREY);
  tiny(fb, "0V", PX + 2, PY + PH - 10, LIGHTGREY);

  fmtTime(t, sizeof(t), PW / 10.0f / RATES[S.rate]);
  snprintf(s, sizeof(s), "%s/div", t);
  tiny(fb, s, PX + PW - 2 - tinyWidth(s), PY + PH - 10, LIGHTGREY);

  const char *labels[4] = {"Zoom -", "Zoom +", S.hold ? "Run" : "Hold", S.test ? "Test on" : "Test"};
  int hit = toolbar(fb, labels, 4, GREEN, S.test ? 3 : -1);
  if (hit == 0 && S.rate < NRATES - 1) S.rate++;            // slower sampling = more time on screen
  if (hit == 1 && S.rate > 0) S.rate--;
  if (hit == 2) S.hold = !S.hold;
  if (hit == 3) setTest(!S.test);
  if (hit == 0 || hit == 1) {                                // restart at the new rate
    if (S.capturing) {
      dma_channel_abort(S.dma);
      adc_run(false);
      S.capturing = false;
    }
    glowClear();
  }
  if (tapIn(PX, PY, PW, PH)) S.hold = !S.hold;
  if (S.hold) textCenter(fb, MEDIUM, "HOLD", PX + PW / 2, PY + 22, rgb(255, 200, 80));
  if (!S.haveTrace) textCenter(fb, SMALL, "Capturing...", PX + PW / 2, PY + PH / 2, WHITE);
}

static void leave() {
  dma_channel_abort(S.dma);
  adc_run(false);
  adc_fifo_drain();
  adc_fifo_setup(false, false, 0, false, false);   // leave the ADC as analogRead() expects
  adc_set_clkdiv(0);
  dma_channel_unclaim(S.dma);
  if (S.test) setTest(false);
}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  rect(fb, cx - 17, cy - 13, 34, 26, dim(col));
  int px = -1, py = 0;
  for (int x = -15; x <= 15; x++) {
    int y = cy + (int)(sinf(x * 0.42f) * 9);
    if (px >= 0) thickLine(fb, px, py, cx + x, y, WHITE);
    px = cx + x;
    py = y;
  }
}

}  // namespace scope
