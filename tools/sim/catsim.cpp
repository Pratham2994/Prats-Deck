// Renders Chindi's poses on a plain background, to check the drawing.
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
void gpio_put(uint, bool) {}
bool gpio_get(uint) { return true; }
void spi_write_blocking(spi_inst_t *, const uint8_t *, size_t) {}
void spi_read_blocking(spi_inst_t *, uint8_t, uint8_t *, size_t) {}
void dma_channel_configure(int, const dma_channel_config *, volatile void *, const volatile void *, uint, bool) {}
void dma_channel_set_read_addr(int, const volatile void *, bool) {}
void dma_channel_set_trans_count(int, uint, bool) {}

#include "../../src/core.h"
#include "../../src/chindi_cat.h"

static void save(const char *name, uint16_t *fb) {
  char path[256];
  snprintf(path, sizeof(path), "out/%s.ppm", name);
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; i++) {
    uint16_t c = fb[i];
    uint8_t p[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
    fwrite(p, 1, 3, f);
  }
  fclose(f);
}

static void bg(uint16_t *fb) {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y * W + x] = y < 150 ? rgb(240, 228, 206) : rgb(222, 212, 196);
}

int main() {
  uint16_t *fb = frame[0];
  using namespace kitty;
  struct { const char *name; uint8_t pose; } poses[] = {
    {"cat_sit", SIT}, {"cat_loaf", LOAF}, {"cat_walk", WALK}, {"cat_curl", CURL}, {"cat_crouch", CROUCH}, {"cat_eat", EAT},
    {"cat_flop", FLOP},
  };
  for (auto &p : poses) {
    bg(fb);
    Look L;
    L.pose = p.pose;
    L.x = 160;
    L.y = 205;
    L.s = 0.85f;
    L.t = 1.0f;
    L.phase = 1.0f;
    if (p.pose == CROUCH) { L.crouch = 1; L.pupil = 1; }
    if (p.pose == EAT) L.headDown = 1;
    if (p.pose == CURL || p.pose == FLOP || p.pose == LOAF || p.pose == SIT) L.s = 1.3f;
    if (p.pose == SIT) L.y = 232;
    draw(fb, L);
    save(p.name, fb);
  }
  // big close-up of the face, plus variations
  bg(fb);
  Look L;
  L.s = 1.5f;
  L.x = 160;
  L.y = 330;
  draw(fb, L);
  save("cat_face", fb);
  bg(fb);
  L.s = 0.7f; L.x = 60; L.y = 200; L.happy = true; L.acc = A_BANDANA; L.purr = 0; draw(fb, L);
  L = Look(); L.s = 0.7f; L.x = 160; L.y = 200; L.mouth = M_HISS; L.earBack = 1; L.tailPuff = 1; L.acc = A_BELL; draw(fb, L);
  L = Look(); L.s = 0.7f; L.x = 260; L.y = 200; L.inBox = true; L.acc = A_HAT; L.glasses = true; draw(fb, L);
  save("cat_moods", fb);
  bg(fb);
  L = Look(); L.s = 0.7f; L.x = 70; L.y = 200; L.pose = GROOM; L.groom = 1; L.mouth = M_TONGUE; L.eyeOpen = 0.5f; L.acc = A_CROWN; draw(fb, L);
  L = Look(); L.s = 0.7f; L.x = 175; L.y = 200; L.pose = POUNCE; L.stretch = 1; L.pupil = 1; draw(fb, L);
  L = Look(); L.s = 0.7f; L.x = 260; L.y = 200; L.pose = BAT; L.bat = 1; L.flip = true; L.acc = A_BOW; draw(fb, L);
  save("cat_moves", fb);
  printf("ok\n");
}
