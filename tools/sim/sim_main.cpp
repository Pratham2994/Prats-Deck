// Runs the real sketch code on the PC with fake hardware, drives touch, saves screenshots.
#include "Arduino.h"

uint64_t simUs = 1000000;
RP2040 rp2040;
SerialSim Serial;
EEPROMSim EEPROM;
KeyboardSim Keyboard;
WiFiSim WiFi;
NTPSim NTP;
spi_hw_t spiHw;
adc_hw_t adcHw;
float adcDiv = 0;

static bool simDown = false;
static int simX = 0, simY = 0;
static uint8_t gpioState[32];
static uint8_t lastCmd = 0;
static const volatile void *lastRead = nullptr;
static const uint16_t *lastSent = nullptr;

void gpio_put(uint p, bool v) { gpioState[p] = v; }
bool gpio_get(uint p) { return p == 17 ? !simDown : gpioState[p]; }
void spi_write_blocking(spi_inst_t *, const uint8_t *d, size_t n) {
  if (gpioState[16] == 0 && n == 1) lastCmd = d[0];
}
void spi_read_blocking(spi_inst_t *, uint8_t, uint8_t *d, size_t n) {
  if (gpioState[16] == 0 && n == 2) {
    int raw = lastCmd == 0xD0 ? (simX + 20) * 10 : (simY + 20) * 10;
    int v = raw << 3;
    d[0] = v >> 8;
    d[1] = v & 0xFF;
  }
}
void dma_channel_configure(int, const dma_channel_config *c, volatile void *w, const volatile void *r, uint n, bool) {
  if (c->dreq == DREQ_ADC) {               // fake ADC capture: mains hum + noise
    float rate = 48e6f / (fmaxf(adcDiv, 95) + 1);
    static double t0 = 0;
    uint16_t *b = (uint16_t *)w;
    for (uint i = 0; i < n; i++) {
      double t = t0 + i / rate;
      float v = 1.65f + 1.1f * sinf(6.2831853f * 50 * t) + 0.3f * sinf(6.2831853f * 150 * t) + (rand() % 100 - 50) / 1000.0f;
      b[i] = (uint16_t)constrain((int)(v / 3.3f * 4095), 0, 4095);
    }
    t0 += 0.0137;
  }
  lastRead = r;
}
void dma_channel_set_read_addr(int, const volatile void *r, bool) { lastRead = r; }
void dma_channel_set_trans_count(int, uint, bool) { lastSent = (const uint16_t *)lastRead; }

#include "../../Prats-Deck.ino"

static void step(int n = 1) {
  for (int i = 0; i < n; i++) {
    loop();
    simUs += 22000;
  }
}

static void shot(const char *name) {
  step(1);                                    // the frame drawn last is now "sent"
  char path[256];
  snprintf(path, sizeof(path), "out/%s.ppm", name);
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; i++) {
    int x = i % W, y = i / W;
    uint16_t c = lcdSync ? lastSent[x * H + (H - 1 - y)] : lastSent[i];   // tear-free mode sends a turned picture
    uint8_t rgb3[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
    fwrite(rgb3, 1, 3, f);
  }
  fclose(f);
  printf("shot %s  (app %d, fps var %d)\n", name, cur, fps);
}

static void down(int x, int y) { simDown = true; simX = x; simY = y; }
static void up() { simDown = false; }
static void tap(int x, int y) { up(); step(6); down(x, y); step(4); up(); step(7); }
static void tileTap(int i) { tap((i % 5) * 64 + 32, 54 + (i / 5) * 62 + 25); }
static void goHome() { down(10, 10); step(30); up(); step(8); }

int main() {
  Cal c = {CAL_MAGIC, false, 0.1f, -20, 0.1f, -20};
  EEPROM.put(0, c);
  setup();
  if (getenv("TOUCHTEST")) {                   // drag with stylus contact dropouts
    tileTap(1);
    step(20);
    int releases = 0;
    down(100, 100);
    for (int k = 0; k < 120; k++) {
      simX = 100 + k;
      simDown = (k % 10) < 7;                  // lose contact for 3 frames (66 ms) every 10
      step(1);
      if (T.released) releases++;
    }
    printf("drag with 66 ms dropouts: %d false lifts (want 0)\n", releases);
    up();
    int lifted = 0;
    for (int k = 0; k < 8; k++) { step(1); if (T.released) lifted++; }
    printf("real lift detected: %d (want 1)\n", lifted);
    return 0;
  }
  if (getenv("CLOCKONLY")) {                   // weather path, with Wi-Fi configured
    tileTap(4);
    step(30);
    shot("c1_connecting");
    step(200);
    char n[32];
    snprintf(n, sizeof(n), "c2_weather_%s", getenv("WCODE") ? getenv("WCODE") : "63");
    shot(n);
    goHome();
    shot("c3_home_with_time");
    return 0;
  }
  step(40);
  shot("01_home");

  tileTap(1);
  step(80);
  shot("02_galaxy");
  down(160, 120);
  for (int k = 0; k < 40; k++) { simX = 160 + (int)(60 * cosf(k * 0.3f)); simY = 120 + (int)(50 * sinf(k * 0.3f)); step(1); }
  shot("03_galaxy_drag");
  for (int k = 0; k < 4; k++) { simX += 40; step(1); }   // flick right
  up();
  step(12);
  shot("04_galaxy_flick");
  down(10, 10);
  step(14);
  shot("05_home_hold_ring");
  step(16);
  up();
  step(8);
  printf("after hold: app %d (want -1)\n", cur);

  tileTap(2);
  step(20);
  down(4 + 78 + 39, 28 + 34);
  step(3);
  shot("06_macro_press");
  up();
  step(8);
  printf("keys sent: %d (want 1)\n", Keyboard.sent);
  // the PC says which song plays: the strip shows under the keys, and a key still works
  Serial.in += "NP 1 Bohemian Rhapsody - Remastered 2011 (Live at the Rainbow)\tQueen\n";
  step(10);
  shot("06a_macro_song");
  printf("song state: %d (want 1), title: %s\n", pcstats::song(millis()), pcstats::S.npTitle);
  down(4 + 2 * 78 + 39, 28 + 2 * 57 + 28);       // Undo, in the lower layout
  step(3);
  up();
  step(8);
  printf("keys sent: %d (want 2)\n", Keyboard.sent);
  Serial.in += "NP 2 Song with no artist\t\n";
  step(10);
  shot("06b_macro_paused");
  step(600);                                     // 13 s with no news: the strip goes away
  printf("song state after silence: %d (want 0)\n", pcstats::song(millis()));
  goHome();

  tileTap(3);
  step(20);
  shot("07_pcstats_wait");
  for (int s = 0; s < 40; s++) {
    char line[96];
    snprintf(line, sizeof(line), "PC %.1f %.1f %.1f -1 %.1f %.1f %.1f 9.80 16.00\n", 20 + 30 * fabsf(sinf(s * 0.4f)), 61.2f, 35 + 20 * sinf(s * 0.2f), 55.0f, 120.5f, 30.2f);
    Serial.in += line;
    step(45);
  }
  shot("08_pcstats_live");
  goHome();

  tileTap(4);
  step(30);
  shot("09_clock_connecting");
  step(200);
  shot("10_clock_weather");
  goHome();

  tileTap(5);
  step(30);
  shot("11_wifi_scanning");
  step(120);
  shot("12_wifi_list");
  tap(287, 10);
  step(10);
  shot("13_wifi_chart");
  goHome();

  tileTap(6);
  step(60);
  shot("14_scope");
  goHome();

  tileTap(7);
  step(10);
  shot("15_guide_list");
  tap(160, 130);                               // Scope
  step(10);
  shot("16_guide_page");
  guide::page = 3;                               // Macros: the page with the most text
  step(5);
  shot("16a_guide_macros");
  goHome();

  tileTap(12);
  step(10);
  shot("17_settings");
  goHome();

  tileTap(8);
  step(5);
  down(100, 60);
  for (int k = 0; k < 40; k++) { simX = 100 + k * 3; simY = 60 + (int)(40 * sinf(k * 0.2f)); step(1); }
  up();
  step(10);
  shot("18_paint");
  goHome();

  tileTap(9);
  step(10);
  shot("19_bricks_start");
  tap(160, 150);
  step(60);
  shot("20_bricks_play");
  goHome();

  tileTap(10);
  step(80);
  shot("21_life");
  goHome();

  tileTap(11);
  step(20);
  shot("21a_snake_start");
  {
    using namespace snake;
    const int gw = snake::GW, gh = snake::GH;      // glow.h has a GW and GH too
    int wait = 0, hold = 0;
    bool playShot = false;
    for (int k = 0; k < 4000 && !S.over && S.score < 6; k++) {   // steer to the food with taps beside the head
      int hx = S.body[S.head] % gw, hy = S.body[S.head] / gw, fx = S.food % gw, fy = S.food / gw;
      bool turn;
      if (S.dx != 0) turn = hx == fx || (S.dx > 0 ? fx < hx || hx >= gw - 2 : fx > hx || hx <= 1);
      else turn = hy == fy || (S.dy > 0 ? fy < hy || hy >= gh - 2 : fy > hy || hy <= 1);
      if (!S.started) turn = true;
      if (hold > 0) hold--;                       // the stylus stays down for 3 frames
      else up();
      if (!simDown && turn && wait <= 0 && S.ndx == S.dx && S.ndy == S.dy) {
        int px = OX + hx * CELL + 5, py = OY + hy * CELL + 5;
        if (S.dx != 0) down(160, (fy != hy ? fy > hy : hy < gh / 2) ? min(py + 30, 236) : max(py - 30, 28));
        else down((fx != hx ? fx > hx : hx < gw / 2) ? min(px + 30, 316) : max(px - 30, 44), 130);
        wait = 10;
        hold = 3;
      }
      wait--;
      step(1);
      if (S.score >= 3 && !playShot) {
        shot("21b_snake_play");
        playShot = true;
      }
    }
    up();
    printf("snake: score %d (want 6), still alive %d (want 1)\n", S.score, !S.over);
    int sc = S.score;
    for (int k = 0; k < 600 && !S.over; k++) step(1);   // no more taps: it runs into a wall
    shot("21c_snake_over");
    printf("snake: over %d (want 1), high score %d (want %d)\n", S.over, hi, sc);
  }
  goHome();
  shot("22_home_end");
  return 0;
}
