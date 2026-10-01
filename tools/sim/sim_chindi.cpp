// Drives the Chindi app through its features on the PC, with screenshots and checks.
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

#include "../../Prats-Deck.ino"

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
  printf("shot %-24s act %2d stage %d pose %2d mode %d sheet %d  xp %lu\n", name, chindi::C.act, chindi::C.stage, chindi::C.pose,
         chindi::mode, chindi::sheet, (unsigned long)chindi::P.xp);
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
static void dockTap(int k) { tap(34 + 26 + k * 50, 216); }
// tile k of the open sheet (cols per row as in the sheet)
static void sheetTile(int k, int cols, int rows) {
  int ph = 34 + rows * 50 + 4, top = H - ph - 2;
  int tw = (W - 16 - (cols - 1) * 6) / cols;
  tap(12 + (k % cols) * (tw + 6) + tw / 2, top + 34 + (k / cols) * 50 + 22);
}
static void forceAct(uint8_t a, float dur = 20) {
  chindi::toFloor();
  chindi::setAct(a, dur);
}
// run until the act ends (or the frame limit), taking a shot when `at` says so
static void runAct(int limit, const char *name, bool (*at)()) {
  bool done = false;
  uint8_t a = chindi::C.act;
  for (int k = 0; k < limit && chindi::C.act == a; k++) {
    step(1);
    if (!done && at()) {
      shot(name);
      done = true;
    }
  }
  if (!done) shot(name);
}
static void setRoom(int r) {
  chindi::changeRoom(r);
  step(5);
}

int main() {
  using namespace chindi;
  int fails = 0;
  Cal c = {CAL_MAGIC, false, 0.1f, -20, 0.1f, -20};
  EEPROM.put(0, c);
  setup();
  step(30);
  fails += check(began, "Chindi state loaded at start-up");
  clockapp::tzKnown = true;                    // a sunny afternoon
  clockapp::haveWeather = true;
  clockapp::code = 0;
  clockapp::tzOffset = 0;
  {
    time_t t = time(nullptr);
    struct tm tm;
    gmtime_r(&t, &tm);
    clockapp::tzOffset = ((14 - tm.tm_hour + 24) % 24) * 3600;
  }

  // ---- open Chindi: the living room ----
  tileTap(0);
  step(60);
  fails += check(cur == 0, "Chindi app opens from the first tile");
  P.xp = 2100;                                 // everything unlocked for this tour
  forceAct(IDLE, 30);
  step(10);
  shot("c01_living");
  fails += check(wishNow() >= 0, "she has a wish");

  // pet her head
  float love0 = P.love;
  strokeAt((int)kitty::hit.hx, (int)kitty::hit.hy - 5, 50);
  fails += check(P.love > love0, "stroking her head raises love");
  down((int)kitty::hit.hx - 20, (int)kitty::hit.hy);
  for (int k = 0; k < 25; k++) { simX = (int)kitty::hit.hx + (int)(20 * sinf(k * 0.35f)); step(1); }
  shot("c02_petting");
  up();
  step(10);

  // poses: loaf and the flop
  forceAct(LOAFING, 20);
  step(30);
  shot("c03_loaf");
  forceAct(FLOPPING, 20);
  C.aux = 0;
  step(40);
  shot("c04_flop");
  fails += check(C.pose == kitty::FLOP, "she flops on her side");

  // room spots: fountain, cat tree, scratching post, corner brush, window
  forceAct(IDLE, 30);
  step(3);
  tap(room::FOUNTAIN_X, 160);
  fails += check(C.act == DRINK, "a tap on the fountain sends her to drink");
  runAct(900, "c05_fountain", [] { return C.act == DRINK && C.stage == 1 && C.t > 1.5f; });
  fails += check(P.found & (1u << F_FOUNTAIN), "the fountain is recorded as found");
  forceAct(IDLE, 30);
  step(3);
  tap(64, 120);
  fails += check(C.act == CLIMB, "a tap on the cat tree sends her up");
  runAct(900, "c06_cat_tree", [] { return C.act == CLIMB && C.stage == 2 && C.t > 1.0f; });
  forceAct(IDLE, 30);
  step(3);
  tap(24, 130);
  runAct(900, "c07_scratch", [] { return C.act == SCRATCH && C.stage == 1 && C.t > 1.0f; });
  forceAct(IDLE, 30);
  step(3);
  tap(304, 140);
  runAct(900, "c08_corner_brush", [] { return C.act == RUB && C.stage == 1 && C.t > 1.5f; });
  forceAct(IDLE, 30);
  step(3);
  tap(134, 70);
  runAct(900, "c09_bird_window", [] { return C.act == WINDOWWATCH && C.stage == 2; });

  // feeding, and a gift
  P.hunger = 30;
  forceAct(IDLE, 30);
  dockTap(0);
  step(10);
  shot("c10_feed_sheet");
  sheetTile(1, 3, 1);
  step(110);
  shot("c11_eating");
  step(300);
  fails += check(P.meals >= 1, "the meal is counted");
  giftOwed = true;
  forceAct(IDLE, 0.1f);
  for (int k = 0; k < 600 && giftItem < 0; k++) step(1);
  step(30);
  shot("c12_gift");
  fails += check(giftItem >= 0, "she brings a gift");
  step(120);
  tap((int)giftX, room::CAT_Y - 8);
  fails += check(giftItem < 0 && P.gifts != 0, "a tap collects the gift");

  // play: laser
  forceAct(IDLE, 30);
  dockTap(1);
  step(10);
  shot("c13_play_sheet");
  sheetTile(0, 2, 2);
  step(5);
  fails += check(mode == MD_LASER, "Play > Laser starts laser mode");
  down(250, 190);
  for (int k = 0; k < 60; k++) { simX = 250 - k * 2; step(1); }
  shot("c14_laser");
  up();
  tap(240, 220);                               // Done
  step(5);

  // ---- dining room ----
  setRoom(room::DINING);
  forceAct(IDLE, 30);
  step(20);
  shot("c15_dining");
  tap(294, 100);
  fails += check(C.act == SNIFF, "a tap on the flowers sends her up on the table");
  runAct(900, "c16_flowers", [] { return C.act == SNIFF && C.stage == 3 && C.t > 0.9f; });
  forceAct(SLEEPING, 1e9f);
  step(200);
  shot("c17_dining_sleep");
  forceAct(IDLE, 30);

  // ---- bedroom ----
  setRoom(room::BEDROOM);
  forceAct(IDLE, 30);
  step(20);
  shot("c18_bedroom");
  tap(260, 140);
  runAct(900, "c19_bed", [] { return C.act == BEDLOAF && C.stage == 2 && C.t > 1.0f; });
  forceAct(IDLE, 30);
  C.x = 60;                                    // out of the way of the table
  step(3);
  cupState = CUP_ON;
  tap(160, 142);
  int crashed = 0;
  for (int k = 0; k < 700 && C.act == CUPPUSH; k++) {
    step(1);
    if (cupState == CUP_BROKEN && !crashed) { crashed = 1; shot("c20_cup_crash"); }
  }
  fails += check(crashed == 1, "she pushes the cup off the table and it breaks");
  tap(247, 58);                                // light switch
  step(10);
  shot("c21_lights_off");
  fails += check(lightsOff, "the light switch works");
  tap(247, 58);
  step(5);

  // ---- balcony ----
  setRoom(room::BALCONY);
  forceAct(IDLE, 30);
  step(20);
  callBird(millis(), 60000);
  step(5);
  shot("c22_balcony");
  tap(296, 130);
  runAct(900, "c23_plant", [] { return C.act == PLANTCHEW && C.stage == 1 && C.t > 1.0f; });

  // ---- night in the living room: the fountain glows ----
  setRoom(room::LIVING);
  clockapp::tzOffset += 9 * 3600;
  forceAct(IDLE, 0.1f);
  step(500);
  shot("c24_night");
  fails += check(C.act == SLEEPING, "she goes to sleep late at night");
  clockapp::tzOffset -= 9 * 3600;
  forceAct(IDLE, 30);
  step(20);

  // ---- sheets and the album ----
  dockTap(4);
  step(10);
  shot("c25_more_sheet");
  sheetTile(2, 2, 3);                          // rooms
  step(10);
  shot("c26_rooms_sheet");
  sheet = SH_NONE;
  step(3);
  tap(250, 18);
  step(12);
  shot("c27_profile");
  sheet = SH_NONE;
  P.gifts = 0x2B5;
  mode = MD_ALBUM;
  albumTab = 0;
  step(5);
  shot("c28_album_gifts");
  albumTab = 1;
  step(5);
  shot("c29_album_found");
  mode = MD_NORMAL;
  step(5);
  dockTap(4);
  step(8);
  sheetTile(3, 2, 3);                          // photo
  step(10);
  tap(W / 2, 220);
  step(4);
  fails += check(P.nPhotos == 1, "the shutter saves a photo");
  tap(260, 220);                               // Gallery
  step(10);
  shot("c30_gallery");
  tap(291, 220);
  step(5);

  // ---- mini-games ----
  startGame(G_FISH);
  down(160, 200);
  for (int k = 0; k < 200; k++) { simX = 160 + (int)(120 * sinf(k * 0.05f)); step(1); }
  shot("c31_fish_catch");
  up();
  for (int k = 0; k < 1500 && !gOver; k++) step(1);
  shot("c32_fish_over");
  fails += check(gOver, "Fish Catch ends");
  tap(214, 162);                               // Back
  step(5);
  startGame(G_MOUSE);
  for (int k = 0; k < 160; k++) {
    for (int i = 0; i < HOLES; i++)
      if (holeOn[i] && holeAge[i] > 0.2f) { tap(HOLE_X[i], HOLE_Y[i] - 10); break; }
    step(1);
  }
  shot("c33_mouse_whack");
  fails += check(gScore > 0, "tapping mice scores");
  mode = MD_NORMAL;
  step(5);

  // ---- home ----
  goHome();
  step(20);
  shot("h1_home");

  // the saved state survives a restart
  saveNow();
  uint32_t xp = P.xp;
  P.xp = 0;
  load();
  fails += check(P.xp == xp, "Chindi's progress is saved in flash");

  printf("\nChindi frames: %ld, raster pixels per frame avg %ld max %ld, peak shapes %d / %d\n", frameCount,
         frameCount ? pixTotal / frameCount : 0, pixMax, cg::simPeak, cg::MAXSH);
  printf("%d screenshots, %d checks failed\n", shots, fails);
  return fails;
}
