// Drives the Chindi app through every feature on the PC, with screenshots and checks.
#define SIM_COUNT
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
void dma_channel_configure(int, const dma_channel_config *, volatile void *, const volatile void *r, uint, bool) { lastRead = r; }
void dma_channel_set_read_addr(int, const volatile void *r, bool) { lastRead = r; }
void dma_channel_set_trans_count(int, uint, bool) { lastSent = (const uint16_t *)lastRead; }

#include "../../picodeck.ino"

static long frameCount = 0, pixTotal = 0, pixMax = 0;
static void step(int n = 1) {
  for (int i = 0; i < n; i++) {
    long before = cg::simPixels;
    loop();
    long used = cg::simPixels - before;
    if (cur == 0) {
      frameCount++;
      pixTotal += used;
      pixMax = max(pixMax, used);
    }
    simUs += 22000;
  }
}

static int shots = 0;
static void shot(const char *name) {
  step(1);
  char path[256];
  snprintf(path, sizeof(path), "out/%s.ppm", name);
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; i++) {
    uint16_t c = lastSent[i];
    uint8_t p[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
    fwrite(p, 1, 3, f);
  }
  fclose(f);
  shots++;
  printf("shot %-28s act %2d pose %2d mode %d sheet %d  hunger %.0f fun %.0f love %.0f clean %.0f energy %.0f xp %lu\n", name,
         chindi::C.act, chindi::C.pose, chindi::mode, chindi::sheet, chindi::P.hunger, chindi::P.fun, chindi::P.love,
         chindi::P.clean, chindi::P.energy, (unsigned long)chindi::P.xp);
}

static void down(int x, int y) { simDown = true; simX = x; simY = y; }
static void up() { simDown = false; }
static void tap(int x, int y) { up(); step(6); down(x, y); step(4); up(); step(7); }
static void tileTap(int i) { tap((i % 4) * 80 + 40, 54 + (i / 4) * 62 + 25); }
static void goHome() { down(10, 10); step(30); up(); step(8); }
static int check(bool ok, const char *what) {
  printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  return ok ? 0 : 1;
}
// stroke back and forth across a point
static void strokeAt(int x, int y, int frames) {
  down(x - 20, y);
  for (int k = 0; k < frames; k++) {
    simX = x + (int)(20 * sinf(k * 0.35f));
    simY = y + (int)(3 * cosf(k * 0.35f));
    step(1);
  }
  up();
  step(8);
}
static void dockTap(int k) { tap(34 + 25 + k * 50, 216); }
// tile k of the open sheet (cols per row as in the sheet)
static void sheetTile(int k, int cols, int rows) {
  int ph = 34 + rows * 50 + 4, top = H - ph - 2;
  int tw = (W - 16 - (cols - 1) * 6) / cols;
  tap(12 + (k % cols) * (tw + 6) + tw / 2, top + 34 + (k / cols) * 50 + 22);
}
static void forceAct(uint8_t a, float dur = 20) {
  chindi::setAct(a, dur);
  chindi::C.base = room::CAT_Y;
  chindi::C.hop = 0;
}

int main() {
  int fails = 0;
  Cal c = {CAL_MAGIC, false, 0.1f, -20, 0.1f, -20};
  EEPROM.put(0, c);
  setup();
  step(30);
  shot("h1_home");
  fails += check(chindi::began, "Chindi state loaded at start-up");

  // ---- open Chindi ----
  tileTap(0);
  step(60);
  shot("c01_room_day");
  fails += check(cur == 0, "Chindi app opens from the first tile");

  // pet her head
  forceAct(chindi::IDLE, 30);
  step(5);
  float love0 = chindi::P.love;
  strokeAt((int)kitty::hit.hx, (int)kitty::hit.hy - 5, 50);
  fails += check(chindi::P.love > love0, "stroking her head raises love");
  fails += check(chindi::P.pets >= 1, "a pet is counted");
  down((int)kitty::hit.hx - 20, (int)kitty::hit.hy);
  for (int k = 0; k < 25; k++) { simX = (int)kitty::hit.hx + (int)(20 * sinf(k * 0.35f)); step(1); }
  shot("c02_petting");
  up();
  step(10);

  // slow blink: tap an eye
  tap((int)kitty::hit.ex[0], (int)kitty::hit.ey[0]);
  step(6);
  shot("c03_slow_blink");

  // boop until she sneezes once
  int sneezes = 0;
  for (int k = 0; k < 12 && !sneezes; k++) {
    forceAct(chindi::IDLE, 30);
    step(3);
    tap((int)kitty::hit.nx, (int)kitty::hit.ny);
    if (chindi::C.act == chindi::SNEEZE) sneezes++;
    step(20);
  }
  fails += check(sneezes > 0, "booping the nose can make her sneeze");

  // tail pull -> annoyed
  forceAct(chindi::IDLE, 30);
  step(3);
  down((int)kitty::hit.tx[2], (int)kitty::hit.ty[2]);
  for (int k = 0; k < 12; k++) { simX -= 3; step(1); }
  up();
  step(5);
  shot("c04_tail_pull_hiss");
  fails += check(chindi::C.act == chindi::ANNOYED, "pulling her tail annoys her");
  step(120);

  // ---- feeding ----
  chindi::P.hunger = 30;
  forceAct(chindi::IDLE, 30);
  dockTap(0);
  step(10);
  shot("c05_feed_sheet");
  sheetTile(0, 3, 1);
  step(90);
  shot("c06_eating");
  float h0 = chindi::P.hunger;
  step(300);
  fails += check(chindi::P.hunger > h0 + 20, "eating kibble fills her up");
  fails += check(chindi::P.meals >= 1, "the meal is counted");

  // ---- play: laser ----
  dockTap(1);
  step(10);
  shot("c07_play_sheet");
  sheetTile(0, 2, 2);
  step(5);
  fails += check(chindi::mode == chindi::MD_LASER, "Play > Laser starts laser mode");
  down(260, 190);
  for (int k = 0; k < 60; k++) { simX = 260 - k * 2; step(1); }
  shot("c08_laser_chase");
  step(40);                                    // keep still: she crouches and pounces
  shot("c09_laser_pounce");
  up();
  tap(240, 220);                               // Done
  step(5);
  fails += check(chindi::mode == chindi::MD_NORMAL, "Done ends the play mode");

  // feather and yarn need levels: give XP
  chindi::P.xp = 2100;
  dockTap(1);
  step(8);
  sheetTile(1, 2, 2);
  step(5);
  down(200, 120);
  for (int k = 0; k < 80; k++) { simX = 200 + (int)(60 * sinf(k * 0.1f)); simY = 130 + (int)(20 * sinf(k * 0.17f)); step(1); }
  shot("c10_feather");
  up();
  tap(240, 220);
  step(5);
  dockTap(1);
  step(8);
  sheetTile(2, 2, 2);
  step(5);
  down((int)chindi::yX, (int)chindi::yY);
  step(3);
  for (int k = 0; k < 5; k++) { simX -= 25; simY -= 10; step(1); }
  up();
  step(30);
  shot("c11_yarn");
  tap(240, 220);
  step(5);

  // ---- brush ----
  chindi::P.clean = 20;
  forceAct(chindi::IDLE, 60);
  step(5);
  shot("c12_dirty");
  dockTap(2);
  step(3);
  float cl0 = chindi::P.clean;
  strokeAt((int)kitty::hit.bx, (int)kitty::hit.by, 60);
  fails += check(chindi::P.clean > cl0 + 5, "brushing makes her clean");
  down((int)kitty::hit.bx - 20, (int)kitty::hit.by);
  for (int k = 0; k < 20; k++) { simX = (int)kitty::hit.bx + (int)(20 * sinf(k * 0.35f)); step(1); }
  shot("c13_brushing");
  up();
  tap(240, 220);
  step(5);

  // ---- behaviours ----
  struct { uint8_t act; const char *name; int frames; } acts[] = {
    {chindi::GROOMING, "c14_groom", 40}, {chindi::KNEADING, "c15_knead", 40}, {chindi::LOAFING, "c16_loaf", 30},
    {chindi::STARING, "c17_stare", 30}, {chindi::ZOOMIES, "c18_zoomies", 25},
  };
  for (auto &a : acts) {
    forceAct(a.act, 20);
    step(a.frames);
    shot(a.name);
  }
  // cup push
  chindi::cupState = chindi::CUP_ON;
  chindi::cupX = room::TABLE_X0 + 12;
  chindi::C.x = 60;
  forceAct(chindi::CUPPUSH, 30);
  int crashed = 0;
  for (int k = 0; k < 600 && chindi::C.act == chindi::CUPPUSH; k++) {
    step(1);
    if (chindi::C.stage == 1 && k % 40 == 0 && !crashed) { shot("c19_cup_pat"); crashed = -1; }
    if (chindi::cupState == chindi::CUP_BROKEN && crashed <= 0) { crashed = 1; shot("c20_cup_crash"); }
  }
  fails += check(crashed == 1, "she pushes the cup off the table and it breaks");
  // box
  chindi::boxX = 230;
  chindi::boxUntil = millis() + 600000;
  forceAct(chindi::BOXSIT, 30);
  for (int k = 0; k < 400 && !(chindi::C.act == chindi::BOXSIT && chindi::C.stage == 2); k++) step(1);
  step(20);
  shot("c21_box");
  fails += check(chindi::C.act == chindi::BOXSIT && chindi::C.stage == 2, "she climbs into the box");
  chindi::boxX = -1;
  forceAct(chindi::IDLE, 5);

  // weather + time: rainy afternoon, she watches the window
  clockapp::tzKnown = true;
  clockapp::haveWeather = true;
  clockapp::code = 63;
  clockapp::tzOffset = 0;
  forceAct(chindi::WINDOWWATCH, 20);
  step(150);
  shot("c22_window_rain");
  // sunny: sunbeam nap
  clockapp::code = 0;
  forceAct(chindi::SUNBATHE, 40);
  step(300);
  shot("c23_sunbeam_nap");
  // PC busy: laptop
  pcstats::S.v[0] = 85;
  for (int k = 0; k < 400; k++) {
    pcstats::S.lastData = millis();
    if (k == 0) forceAct(chindi::LAPTOP, 60);
    step(1);
  }
  shot("c24_warm_laptop");
  fails += check(chindi::C.act == chindi::LAPTOP && chindi::C.stage == 2, "she lies on the warm laptop when the PC is busy");
  pcstats::S.lastData = 0;
  step(60);

  // keyboard walk
  Keyboard.typed.clear();
  chindi::kbSchedule(millis(), 100);
  step(20);
  shot("c28_keyboard_walk");
  step(200);
  printf("  typed on PC: \"%s\"\n", Keyboard.typed.c_str());
  fails += check(Keyboard.typed.size() >= 4, "keyboard walk types on the PC");

  // night: she sleeps, room dark
  clockapp::tzOffset = 9 * 3600;               // shift the clock so it is night here
  {
    time_t t = time(nullptr) + clockapp::tzOffset;
    struct tm tm;
    gmtime_r(&t, &tm);
    clockapp::tzOffset += ((23 - tm.tm_hour + 24) % 24) * 3600;   // make it 23:xx
  }
  clockapp::code = 0;
  forceAct(chindi::IDLE, 0.1f);
  step(400);
  shot("c29_night_sleep");
  fails += check(chindi::C.act == chindi::SLEEPING, "she goes to sleep late at night");
  clockapp::tzKnown = false;
  forceAct(chindi::IDLE, 5);
  step(20);

  // ---- sheets ----
  dockTap(4);
  step(10);
  shot("c30_more_sheet");
  sheetTile(1, 2, 3);                          // wardrobe
  step(10);
  shot("c31_wardrobe");
  sheetTile(3, 2, 3);                          // bell collar
  step(5);
  fails += check(chindi::P.acc == kitty::A_BELL, "wardrobe puts on the bell collar");
  tap(W - 22, 60);                             // close (approximate)
  chindi::sheet = chindi::SH_NONE;
  dockTap(4);
  step(8);
  sheetTile(2, 2, 3);                          // rooms
  step(8);
  sheetTile(1, 1, 3);                          // study
  step(40);
  shot("c32_study");
  dockTap(4);
  step(8);
  sheetTile(2, 2, 3);
  step(8);
  sheetTile(2, 1, 3);                          // balcony
  step(40);
  shot("c33_balcony");
  fails += check(chindi::P.room == room::BALCONY, "rooms sheet moves her to the balcony");
  chindi::P.room = room::BEDROOM;
  // photo + gallery
  dockTap(4);
  step(8);
  sheetTile(3, 2, 3);
  step(10);
  shot("c34_photo_mode");
  tap(W / 2, 220);
  step(4);
  tap(55, 220);                                // Done
  step(5);
  fails += check(chindi::P.nPhotos == 1, "the shutter saves a photo");
  dockTap(4);
  step(8);
  sheetTile(4, 2, 3);
  step(10);
  shot("c35_gallery");
  tap(291, 220);
  step(5);
  // profile
  tap(250, 18);
  step(12);
  shot("c36_profile");
  chindi::sheet = chindi::SH_NONE;
  // settings sheet
  dockTap(4);
  step(8);
  sheetTile(5, 2, 3);
  step(10);
  shot("c37_settings_sheet");
  chindi::sheet = chindi::SH_NONE;

  // ---- mini-games ----
  chindi::startGame(chindi::G_FISH);
  down(160, 200);
  for (int k = 0; k < 200; k++) { simX = 160 + (int)(120 * sinf(k * 0.05f)); step(1); }
  shot("c38_fish_catch");
  up();
  for (int k = 0; k < 1500 && !chindi::gOver; k++) step(1);
  shot("c39_fish_over");
  fails += check(chindi::gOver, "Fish Catch ends");
  tap(214, 162);                               // Back
  step(5);
  chindi::startGame(chindi::G_MOUSE);
  for (int k = 0; k < 400; k++) {
    for (int i = 0; i < chindi::HOLES; i++)
      if (chindi::holeOn[i] && chindi::holeAge[i] > 0.2f) { tap(chindi::HOLE_X[i], chindi::HOLE_Y[i] - 10); break; }
    step(1);
    if (k == 150) shot("c40_mouse_whack");
  }
  for (int k = 0; k < 1500 && !chindi::gOver; k++) step(1);
  printf("  mouse score %d\n", chindi::gScore);
  fails += check(chindi::gScore > 0, "tapping mice scores");
  shot("c41_mouse_over");
  tap(214, 162);
  step(5);
  chindi::startGame(chindi::G_LASER);
  down(280, 200);
  for (int k = 0; k < 150; k++) { simX = 160 + (int)(140 * sinf(k * 0.04f)); simY = 120 + (int)(60 * cosf(k * 0.07f)); step(1); }
  shot("c42_laser_chase_game");
  up();
  for (int k = 0; k < 2000 && !chindi::gOver; k++) step(1);
  fails += check(chindi::gOver, "Laser Chase ends when she catches the dot");
  tap(214, 162);
  step(5);

  // ---- home: badge and peek ----
  chindi::P.hunger = 10;
  goHome();
  step(800);
  for (int k = 0; k < 4000; k++) {
    step(1);
    if (k % 10 == 0 && simUs > 0) {
    }
  }
  shot("h2_home_badge");
  // force a peek
  step(1);
  tileTap(11);
  step(30);
  shot("s1_settings");
  fails += check(cur == 11, "Settings opens from the last tile");
  goHome();

  // the saved state survives a restart
  chindi::saveNow();
  uint32_t xp = chindi::P.xp;
  chindi::P.xp = 0;
  chindi::load();
  fails += check(chindi::P.xp == xp, "Chindi's progress is saved in flash");

  printf("\nChindi frames: %ld, raster pixels per frame avg %ld max %ld, peak shapes %d / %d\n", frameCount,
         frameCount ? pixTotal / frameCount : 0, pixMax, cg::simPeak, cg::MAXSH);
  printf("%d screenshots, %d checks failed\n", shots, fails);
  return fails;
}
