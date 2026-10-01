// app_chindi.h
// Chindi, a virtual pet based on a real orange and white cat.
//
//   Needs     : hunger, energy, fun, clean, love. They change only while the Pico is on.
//   Touch     : stroke her head, chin or back; boop the nose; tap her eyes to slow-blink;
//               belly rubs are a gamble; do not pull her tail.
//   Rooms     : the room is the menu. Tap the fountain, the cat tree, the corner brush, the
//               window, the flowers, the photo frames, the light switch, the bed, the door,
//               the plants. She goes there and uses them.
//   Play      : laser pointer, feather wand, yarn ball, and three mini-games.
//   Life      : she wanders, grooms, kneads, loafs, flops on her side, stares at nothing,
//               gets the zoomies, pushes the cup off the table, sits in boxes, naps in the
//               sunbeam, watches the birds, lies on the warm laptop when your PC is busy.
//   Game      : three wishes a day earn treats. She brings you gifts to collect. Each new
//               thing you find her doing goes in the album. Bond levels unlock rooms, toys
//               and accessories.
//   Links     : real weather and time in the window, the Monitor app warms the laptop, her
//               mood shows on the home screen, and (if on) she sometimes walks on your
//               keyboard and types on your PC.
#pragma once

#include <Keyboard.h>
#include "core.h"
#include "net.h"
#include "app_clock.h"
#include "app_pcstats.h"
#include "chindi_cat.h"
#include "chindi_room.h"

namespace chindi {

using kitty::Look;

// ---------- Saved state ----------
static const int EE_PET = 256;
static const uint32_t MAGIC = 0x43484E32;    // "CHN2"
static const int NPHOTO = 12;

struct Photo {
  uint8_t room, pose, mouth, acc, flags, wx, eye, base;   // flags: 1 flip, 2 happy, 4 in box, 8 glasses. base: feet y
  int16_t x, hourQ;                                       // hour * 10
  int32_t day;                                            // days since 1970, or -1
};

struct Save {
  uint32_t magic;
  float hunger, energy, fun, clean, love;                 // 0..100
  uint32_t xp;
  uint16_t treats, streak;
  int32_t lastDay;
  uint8_t acc, room, kbWalk, nPhotos, tutorial, wishDone, wish[3], pad[3];
  uint16_t hiFish, hiMouse, hiLaser, gifts;               // gifts: one bit per gift collected
  uint32_t pets, meals, games, found;                     // found: one bit per discovery
  int32_t wishDay;                                        // the day these wishes are for
  Photo photos[NPHOTO];
};
static_assert(EE_PET + sizeof(Save) <= 1024, "Chindi's save data must fit in the 1 KB flash area");
static Save P;
static bool began = false, dirty = false, open = false;
static uint32_t lastSave = 0;

static void defaults() {
  memset(&P, 0, sizeof(P));
  P.magic = MAGIC;
  P.hunger = 70;
  P.energy = 80;
  P.fun = 60;
  P.clean = 85;
  P.love = 50;
  P.treats = 3;
  P.kbWalk = 1;
  P.lastDay = -1;
  P.wishDay = -9;
  P.wishDone = 3;
}

static void saveNow() {
  EEPROM.put(EE_PET, P);
  EEPROM.commit();
  dirty = false;
  lastSave = millis();
}

static void bump(float &v, float d) {
  v = constrain(v + d, 0.0f, 100.0f);
  dirty = true;
}

// ---------- Levels and unlocks ----------
static const uint32_t LV[10] = {0, 60, 150, 280, 450, 660, 920, 1230, 1600, 2050};
static const uint8_t ACC_LV[kitty::NACC] = {1, 2, 4, 6, 8, 10};
static const char *const ACC_NAMES[kitty::NACC] = {"None", "Bandana", "Bow", "Bell collar", "Party hat", "Crown"};
static const uint8_t TOY_LV[3] = {1, 2, 4};
static const char *const TOY_NAMES[3] = {"Laser", "Feather", "Yarn"};
static const uint8_t ROOM_LV[room::NROOM] = {1, 3, 5, 7};

static int level() {
  int l = 1;
  while (l < 10 && P.xp >= LV[l]) l++;
  return l;
}

static const char *unlockText(int lv) {
  switch (lv) {
    case 2: return "Feather wand and bandana";
    case 3: return "Dining room";
    case 4: return "Yarn ball and bow";
    case 5: return "Bedroom";
    case 6: return "Bell collar";
    case 7: return "Balcony";
    case 8: return "Party hat";
    case 9: return "One more level to the crown";
    case 10: return "Crown!";
    default: return "";
  }
}

static void load() {
  EEPROM.get(EE_PET, P);
  if (P.magic != MAGIC) {
    defaults();
    saveNow();
  }
  float *v[5] = {&P.hunger, &P.energy, &P.fun, &P.clean, &P.love};
  for (float *f : v)
    if (!(*f >= 0 && *f <= 100)) *f = 50;
  if (P.acc >= kitty::NACC) P.acc = 0;
  if (P.room >= room::NROOM) P.room = 0;
  if (P.nPhotos > NPHOTO) P.nPhotos = 0;
  if (P.wishDone > 3) P.wishDone = 3;
}

// ---------- Particles ----------
enum PK : uint8_t { PK_HEART, PK_SPARK, PK_Z, PK_CRUMB, PK_CONFETTI, PK_FUR, PK_SHARD, PK_DUST, PK_DROP, PK_RING, PK_PETAL };
struct Part {
  float x, y, vx, vy, life, max, size;
  uint16_t col;
  uint8_t kind;
};
static const int NPART = 96;
static Part parts[NPART];
static int partNext = 0;

static void emit(uint8_t k, float x, float y, float vx, float vy, float life, uint16_t col, float size = 3) {
  Part &p = parts[partNext];
  partNext = (partNext + 1) % NPART;
  p = {x, y, vx, vy, life, life, size, col, k};
}

static float frand(float a, float b) { return a + (b - a) * random(0, 10001) / 10000.0f; }

static void burst(uint8_t k, float x, float y, int n, float spd, float life, uint16_t col, float size = 3) {
  for (int i = 0; i < n; i++) {
    float a = frand(0, 6.283f), s = spd * frand(0.4f, 1);
    emit(k, x, y, cosf(a) * s, sinf(a) * s - (k == PK_CONFETTI ? spd * 0.6f : 0), life * frand(0.7f, 1.2f), col, size);
  }
}

static void confetti(float x, float y, int n) {
  for (int i = 0; i < n; i++) {
    float a = frand(3.6f, 5.8f), s = frand(120, 320);
    emit(PK_CONFETTI, x, y, cosf(a) * s, sinf(a) * s, frand(1.2f, 2.2f), hsv(random(0, 360)), frand(2, 4));
  }
}

static void updateParts(float dt) {
  for (auto &p : parts) {
    if (p.life <= 0) continue;
    p.life -= dt;
    float g = 0;
    switch (p.kind) {
      case PK_CRUMB: g = 380; break;
      case PK_CONFETTI: g = 170; p.vx *= powf(0.4f, dt); break;
      case PK_SHARD: g = 520; break;
      case PK_DROP: g = 300; break;
      case PK_FUR: g = 25; p.vx += sinf(p.life * 6) * 20 * dt; break;
      case PK_PETAL: g = 40; p.vx = sinf(p.life * 4 + p.size) * 26; break;
      case PK_HEART: case PK_Z: p.vx = sinf(p.life * 3 + p.size) * 12; break;
      case PK_RING: break;
      default: p.vx *= powf(0.2f, dt); p.vy *= powf(0.2f, dt);
    }
    p.vy += g * dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if ((p.kind == PK_SHARD || p.kind == PK_CRUMB) && p.y > room::CAT_Y - 3) {
      p.y = room::CAT_Y - 3;
      p.vy = -p.vy * 0.3f;
      p.vx *= 0.6f;
    }
  }
}

static void dotA(uint16_t *fb, int x, int y, uint16_t c, int a) {
  if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = blend(fb[y * W + x], c, a);
}

static void heartShape(float x, float y, float s, uint16_t col, int a) {
  cg::X.set(x, y, s, false);
  int l = cg::ell(-4, -2, 4.6f, 4.6f, 0, col);
  int r = cg::ell(4, -2, 4.6f, 4.6f, 0, col);
  int t = cg::tri(-8.6f, -0.5f, 8.6f, -0.5f, 0, 9, col);
  cg::alpha(l, a);
  cg::alpha(r, a);
  cg::alpha(t, a);
}

static void drawParts(uint16_t *fb) {
  cg::begin();
  for (auto &p : parts) {
    if (p.life <= 0) continue;
    float k = constrain(p.life / p.max, 0.0f, 1.0f);
    int a = (int)(255 * fminf(1, k * 2));
    int x = (int)p.x, y = (int)p.y;
    switch (p.kind) {
      case PK_HEART: heartShape(p.x, p.y, p.size / 9, p.col, a); break;
      case PK_Z: {
        char z[2] = {k > 0.5f ? 'Z' : 'z', 0};
        text(fb, p.size > 3 ? SMALL : TINY, z, x, y, blend(rgb(60, 70, 120), p.col, a));
        break;
      }
      case PK_SPARK:
        for (int d = -2; d <= 2; d++) {
          dotA(fb, x + d, y, p.col, a * (3 - abs(d)) / 3);
          dotA(fb, x, y + d, p.col, a * (3 - abs(d)) / 3);
        }
        break;
      case PK_CONFETTI: fillRect(fb, x, y, (int)p.size, (int)(p.size * (0.5f + 0.5f * fabsf(sinf(p.life * 9)))) + 1, p.col); break;
      case PK_SHARD: fillTriangle(fb, x, y, x + 3, y + 1, x + 1, y + 4, p.col); break;
      case PK_FUR: pixelA(fb, x, y, p.col, a / 255.0f); pixelA(fb, x + 1, y, p.col, a / 400.0f); break;
      case PK_DUST:
        for (int j = -2; j <= 2; j++)
          for (int i = -2; i <= 2; i++)
            if (i * i + j * j <= 4) dotA(fb, x + i, y + j, p.col, a / 3);
        break;
      case PK_RING: {                          // a ripple where you tapped
        float r = (1 - k) * p.size;
        for (int s = 0; s < 20; s++) {
          float an = s * 0.31416f;
          pixelA(fb, x + (int)(cosf(an) * r), y + (int)(sinf(an) * r * 0.55f), p.col, k);
        }
        break;
      }
      case PK_PETAL: {
        int e = cg::ell(p.x, p.y, 3.2f, 1.8f, p.life * 3, p.col);
        cg::alpha(e, a);
        break;
      }
      default: fillRect(fb, x, y, 2, 2, p.col);
    }
  }
  cg::X.set(0, 0, 1, false);
  cg::render(fb);
}

// ---------- Speech bubble and toast ----------
enum Icon : uint8_t {
  IC_NONE, IC_HEART, IC_FISH, IC_ZZZ, IC_BALL, IC_EXCL, IC_Q, IC_SPARK, IC_BOWL, IC_YARN, IC_BRUSH, IC_MOON,
  IC_DOTS, IC_TREAT, IC_LASER, IC_FEATHER, IC_PAD, IC_HANGER, IC_HOUSE, IC_CAMERA, IC_PHOTOS, IC_GEAR,
  IC_MOUSE, IC_BOLT, IC_LOCK, IC_X, IC_STAR, IC_KEYS, IC_GIFT, IC_DROP, IC_BIRD
};
static char bubbleText[28] = "";
static uint8_t bubbleIcon = IC_NONE;
static uint32_t bubbleUntil = 0;
static char toastText[52] = "";
static uint16_t toastCol = WHITE;
static uint32_t toastUntil = 0;

static void say(const char *t, uint8_t icon = IC_NONE, uint32_t ms = 2200) {
  strlcpy(bubbleText, t, sizeof(bubbleText));
  bubbleIcon = icon;
  bubbleUntil = millis() + ms;
}

static void toast(const char *t, uint16_t col = WHITE, uint32_t ms = 2600) {
  strlcpy(toastText, t, sizeof(toastText));
  toastCol = col;
  toastUntil = millis() + ms;
}

// ---------- Small icons (about 16 px) ----------
static void icon(uint16_t *fb, uint8_t ic, int cx, int cy, uint16_t c) {
  switch (ic) {
    case IC_HEART:
      fillCircle(fb, cx - 3, cy - 2, 4, c);
      fillCircle(fb, cx + 3, cy - 2, 4, c);
      fillTriangle(fb, cx - 7, cy, cx + 7, cy, cx, cy + 7, c);
      break;
    case IC_FISH:
      fillCircle(fb, cx - 2, cy, 4, c);
      fillRect(fb, cx - 5, cy - 3, 6, 7, c);
      fillTriangle(fb, cx + 2, cy, cx + 8, cy - 5, cx + 8, cy + 5, c);
      pixel(fb, cx - 4, cy - 1, BLACK);
      break;
    case IC_ZZZ: text(fb, TINY, "z", cx - 6, cy + 5, c); text(fb, TINY, "Z", cx, cy + 1, c); break;
    case IC_BALL:
    case IC_YARN:
      fillCircle(fb, cx, cy, 6, c);
      line(fb, cx - 5, cy - 2, cx + 5, cy + 2, dim(c));
      line(fb, cx - 4, cy + 3, cx + 3, cy - 5, dim(c));
      if (ic == IC_YARN) line(fb, cx + 5, cy + 3, cx + 9, cy + 7, c);
      break;
    case IC_EXCL: fillRoundRect(fb, cx - 1, cy - 7, 3, 9, 1, c); fillCircle(fb, cx, cy + 5, 1, c); break;
    case IC_Q: text(fb, SMALL, "?", cx - 3, cy + 5, c); break;
    case IC_SPARK:
    case IC_STAR:
      fillTriangle(fb, cx, cy - 8, cx - 3, cy, cx + 3, cy, c);
      fillTriangle(fb, cx, cy + 8, cx - 3, cy, cx + 3, cy, c);
      fillTriangle(fb, cx - 8, cy, cx, cy - 3, cx, cy + 3, c);
      fillTriangle(fb, cx + 8, cy, cx, cy - 3, cx, cy + 3, c);
      break;
    case IC_BOWL:
      fillTriangle(fb, cx - 9, cy - 1, cx + 9, cy - 1, cx + 5, cy + 6, c);
      fillTriangle(fb, cx - 9, cy - 1, cx + 5, cy + 6, cx - 5, cy + 6, c);
      fillCircle(fb, cx - 3, cy - 3, 2, rgb(170, 110, 60));
      fillCircle(fb, cx + 2, cy - 3, 2, rgb(170, 110, 60));
      break;
    case IC_BRUSH:
      fillRoundRect(fb, cx - 8, cy - 2, 16, 6, 2, c);
      for (int k = 0; k < 6; k++) vline(fb, cx - 7 + k * 3, cy + 4, 4, c);
      fillRect(fb, cx - 2, cy - 8, 4, 6, c);
      break;
    case IC_MOON:
      fillCircle(fb, cx, cy, 7, c);
      fillCircle(fb, cx + 4, cy - 3, 6, CARD);
      break;
    case IC_DOTS: for (int k = -1; k <= 1; k++) fillCircle(fb, cx + k * 6, cy, 2, c); break;
    case IC_TREAT:
      fillCircle(fb, cx, cy, 6, c);
      for (int k = 0; k < 4; k++) fillCircle(fb, cx - 3 + (k % 2) * 5, cy - 3 + (k / 2) * 5, 1, rgb(120, 70, 30));
      break;
    case IC_LASER:
      fillCircle(fb, cx, cy, 3, rgb(255, 50, 50));
      for (int k = 0; k < 4; k++) {
        float a = k * 1.5708f + 0.785f;
        line(fb, cx + (int)(cosf(a) * 5), cy + (int)(sinf(a) * 5), cx + (int)(cosf(a) * 8), cy + (int)(sinf(a) * 8), c);
      }
      break;
    case IC_FEATHER:
      thickLine(fb, cx - 6, cy + 7, cx + 6, cy - 7, c);
      for (int k = 0; k < 4; k++) line(fb, cx - 3 + k * 3, cy + 3 - k * 3, cx - 7 + k * 3, cy - 1 - k * 3, c);
      break;
    case IC_PAD:
      fillRoundRect(fb, cx - 9, cy - 5, 18, 11, 4, c);
      fillRect(fb, cx - 6, cy - 1, 5, 2, CARD);
      fillRect(fb, cx - 4, cy - 3, 2, 5, CARD);
      fillCircle(fb, cx + 4, cy - 1, 1, CARD);
      fillCircle(fb, cx + 6, cy + 1, 1, CARD);
      break;
    case IC_HANGER:
      line(fb, cx - 9, cy + 5, cx, cy - 2, c);
      line(fb, cx + 9, cy + 5, cx, cy - 2, c);
      hline(fb, cx - 9, cy + 5, 19, c);
      circle(fb, cx, cy - 5, 2, c);
      break;
    case IC_HOUSE:
      fillTriangle(fb, cx - 9, cy - 1, cx + 9, cy - 1, cx, cy - 9, c);
      fillRect(fb, cx - 6, cy - 1, 12, 9, c);
      fillRect(fb, cx - 2, cy + 3, 4, 5, CARD);
      break;
    case IC_CAMERA:
      fillRoundRect(fb, cx - 9, cy - 5, 18, 12, 3, c);
      fillRect(fb, cx - 4, cy - 8, 8, 3, c);
      fillCircle(fb, cx, cy + 1, 4, CARD);
      fillCircle(fb, cx, cy + 1, 2, c);
      break;
    case IC_PHOTOS:
      rect(fb, cx - 8, cy - 6, 14, 12, c);
      fillRect(fb, cx - 5, cy - 3, 14, 12, c);
      fillTriangle(fb, cx - 3, cy + 7, cx + 2, cy + 1, cx + 6, cy + 7, CARD);
      break;
    case IC_GEAR:
      for (int k = 0; k < 8; k++) {
        float a = k * 0.785f;
        fillCircle(fb, cx + (int)(cosf(a) * 7), cy + (int)(sinf(a) * 7), 2, c);
      }
      fillCircle(fb, cx, cy, 6, c);
      fillCircle(fb, cx, cy, 2, CARD);
      break;
    case IC_MOUSE:
      fillCircle(fb, cx, cy + 1, 5, c);
      fillCircle(fb, cx - 4, cy - 4, 3, c);
      fillCircle(fb, cx + 4, cy - 4, 3, c);
      line(fb, cx + 5, cy + 4, cx + 9, cy + 7, c);
      break;
    case IC_BOLT:
      fillTriangle(fb, cx + 2, cy - 8, cx - 4, cy + 1, cx + 1, cy + 1, c);
      fillTriangle(fb, cx - 1, cy - 1, cx + 4, cy - 1, cx - 2, cy + 8, c);
      break;
    case IC_LOCK:
      roundRect(fb, cx - 4, cy - 7, 8, 8, 3, c);
      fillRoundRect(fb, cx - 6, cy - 2, 12, 9, 2, c);
      break;
    case IC_X:
      thickLine(fb, cx - 5, cy - 5, cx + 5, cy + 5, c);
      thickLine(fb, cx - 5, cy + 5, cx + 5, cy - 5, c);
      break;
    case IC_KEYS:
      fillRoundRect(fb, cx - 9, cy - 5, 18, 11, 2, c);
      for (int r = 0; r < 2; r++)
        for (int k = 0; k < 4; k++) fillRect(fb, cx - 7 + k * 4 + r, cy - 3 + r * 4, 3, 2, CARD);
      break;
    case IC_GIFT:
      fillRoundRect(fb, cx - 7, cy - 2, 14, 9, 2, c);
      fillRoundRect(fb, cx - 8, cy - 5, 16, 4, 1, c);
      fillRect(fb, cx - 1, cy - 5, 2, 12, CARD);
      circle(fb, cx - 3, cy - 7, 2, c);
      circle(fb, cx + 3, cy - 7, 2, c);
      break;
    case IC_DROP:
      fillCircle(fb, cx, cy + 2, 5, c);
      fillTriangle(fb, cx - 4, cy, cx + 4, cy, cx, cy - 8, c);
      break;
    case IC_BIRD:
      fillCircle(fb, cx - 1, cy + 1, 5, c);
      fillCircle(fb, cx + 4, cy - 3, 3, c);
      fillTriangle(fb, cx + 6, cy - 4, cx + 10, cy - 3, cx + 6, cy - 1, c);
      fillTriangle(fb, cx - 5, cy, cx - 10, cy - 3, cx - 9, cy + 3, c);
      break;
    default: break;
  }
}

// ---------- The world ----------
static room::Env E;
static bool pcBusy = false;
static float worldT = 0;

static void updateEnv() {
  E = room::Env();
  if (net::timeValid() && clockapp::tzKnown) {
    time_t t = time(nullptr) + clockapp::tzOffset;
    struct tm tm;
    gmtime_r(&t, &tm);
    E.hour = tm.tm_hour + tm.tm_min / 60.0f;
    E.timeKnown = true;
  }
  if (clockapp::haveWeather) {
    switch (clockapp::kindOf(clockapp::code)) {
      case clockapp::K_PARTLY: case clockapp::K_CLOUDY: E.wx = room::W_CLOUDY; break;
      case clockapp::K_FOG: E.wx = room::W_FOG; break;
      case clockapp::K_DRIZZLE: case clockapp::K_RAIN: E.wx = room::W_RAIN; break;
      case clockapp::K_SNOW: E.wx = room::W_SNOW; break;
      case clockapp::K_STORM: E.wx = room::W_STORM; break;
      default: E.wx = room::W_CLEAR;
    }
  }
  pcBusy = pcstats::S.lastData && (int32_t)(millis() - pcstats::S.lastData) < 3000 && pcstats::S.v[0] > 55;
}

static int today() {
  if (!(net::timeValid() && clockapp::tzKnown)) return -1;
  return (int)((time(nullptr) + clockapp::tzOffset) / 86400);
}

static const room::Def &R() { return room::DEFS[P.room]; }

// ---------- Props state ----------
enum CupState : uint8_t { CUP_ON, CUP_FALLING, CUP_BROKEN };
static uint8_t cupState = CUP_ON;
static float cupX = room::TABLE_X0 + 12, cupY = 0, cupVx = 0, cupVy = 0, cupRot = 0;
static uint32_t cupBack = 0;
static float boxX = -1;                       // < 0: no box
static uint32_t boxUntil = 0;
static float bowlX = -1, bowlFood = 0;
static uint8_t foodKind = 0;
static float flowerSway = 0, flowerKick = 0;  // the vase: how far the flowers lean, and a push from her paw
static float birdX = -100;                    // a bird on the sill or the railing (< 0: none)
static uint32_t birdUntil = 0;
static bool birdFlying = false;
static int giftItem = -1;                     // a gift on the floor, waiting for you (-1: none)
static float giftX = 160;
static uint32_t giftUntil = 0, giftNext = 0;
static bool giftOwed = false;                 // all wishes done: she will bring one

// ---------- Chindi ----------
enum Act : uint8_t {
  IDLE, WANDER, LOAFING, GROOMING, KNEADING, STARING, ZOOMIES, CUPPUSH, BOXSIT, SUNBATHE, WINDOWWATCH,
  LAPTOP, SLEEPING, EATING, ANNOYED, SWAT, SNEEZE, KBWALK, YAWN, REFUSE, CELEBRATE, PLAY,
  FLOPPING, DRINK, CLIMB, SCRATCH, RUB, SNIFF, PLANTCHEW, BEDLOAF, GIFTING
};
struct Cat {
  float x = 130, hop = 0, hopV = 0, base = room::CAT_Y;
  uint8_t act = IDLE, stage = 0;
  float t = 0, dur = 4, goal = 130, aux = 0;
  bool left = false;
  float phase = 0;
  uint8_t pose = kitty::SIT;
  // expression, smoothed towards these
  float eyeT = 1, pupilT = 0.3f, earT = 0, tiltT = 0, lookXT = 0, lookYT = 0, purrT = 0, puffT = 0, swingT = 1;
  bool happy = false;
  uint8_t mouth = kitty::M_CLOSED;
  float blinkIn = 3, blinkLeft = 0, lookIn = 2, twitchIn = 4, slowBlink = 0;
  float petting = 0;
  Look L;
};
static Cat C;
static bool lightsOff = false;

static void setAct(uint8_t a, float dur = 4) {
  C.act = a;
  C.stage = 0;
  C.t = 0;
  C.dur = dur;
}

static bool walkTo(float gx, float speed, float dt) {
  float dx = gx - C.x;
  if (fabsf(dx) < 2) {
    C.x = gx;
    return true;
  }
  C.left = dx < 0;
  C.x += (dx > 0 ? 1 : -1) * fminf(speed * dt, fabsf(dx));
  C.pose = speed > 140 ? kitty::RUN : kitty::WALK;
  C.phase += dt * speed * (speed > 140 ? 0.07f : 0.11f);
  return false;
}

static void hopUp(float v) {
  if (C.hop <= 0) C.hopV = v;
}

// she is up on something (the perch, the table, the bed, the laptop)
static bool upHigh() { return C.base < room::CAT_Y - 1; }

// put her back on the floor at once (used when the room or the mode changes)
static void toFloor() {
  C.base = room::CAT_Y;
  C.hop = 0;
  C.hopV = 0;
  C.x = constrain(C.x, 34.0f, 290.0f);
}

// poses that show her face from the front
static bool frontPose() {
  return C.pose == kitty::SIT || C.pose == kitty::LOAF || C.pose == kitty::GROOM || C.pose == kitty::KNEAD ||
         C.pose == kitty::FLOP;
}

static void celebrate(const char *msg) {
  setAct(CELEBRATE, 1.8f);
  confetti(C.x, kitty::hit.top + 20, 50);
  say(msg, IC_HEART, 2600);
}

static void addXP(int n) {
  int before = level();
  P.xp += n;
  dirty = true;
  int now = level();
  if (now > before) {
    char s[52];
    snprintf(s, sizeof(s), "Level %d! %s", now, unlockText(now));
    toast(s, rgb(255, 200, 80), 4500);
    if (open && !upHigh()) celebrate("level up!");
    else if (open) confetti(C.x, 80, 50);
    saveNow();
  }
}

// first interaction of a new day: keep the streak going
static void touchDay() {
  int d = today();
  if (d < 0 || d == P.lastDay) return;
  P.streak = (P.lastDay >= 0 && d == P.lastDay + 1) ? P.streak + 1 : 1;
  P.lastDay = d;
  if (P.streak > 1) {
    P.treats++;
    char s[44];
    snprintf(s, sizeof(s), "Day %d streak! +1 treat", P.streak);
    toast(s, rgb(255, 170, 60), 3500);
    if (open) confetti(160, 60, 40);
  }
  saveNow();
}

// ---------- Discoveries: each new thing you find her doing ----------
enum Find : uint8_t {
  F_FOUNTAIN, F_TREE, F_SCRATCH, F_BRUSH, F_BIRD, F_FLOWERS, F_FRAMES, F_SWITCH, F_BED, F_DOOR, F_PLANT, F_CUP,
  F_BOX, F_ZOOM, F_BLINK, F_SNEEZE, F_BELLY, F_FLOP, F_KEYS, F_LAPTOP, NFIND
};
static const char *const FIND_NAMES[NFIND] = {
  "Water fountain", "Cat tree", "Scratching post", "Corner brush", "Bird watching", "Flower sniff", "Photo shelf",
  "Light switch", "Bed nap", "The door", "Plant nibble", "Cup crash", "The box", "Zoomies", "Slow blink", "Sneeze",
  "Belly trap", "The flop", "Keyboard walk", "Warm laptop"};

static int countBits(uint32_t v) {
  int n = 0;
  for (; v; v &= v - 1) n++;
  return n;
}

static void discover(uint8_t f) {
  if (P.found & (1u << f)) return;
  P.found |= 1u << f;
  char s[52];
  snprintf(s, sizeof(s), "Found: %s (%d/%d)", FIND_NAMES[f], countBits(P.found), NFIND);
  toast(s, rgb(150, 220, 255), 3200);
  addXP(8);
  saveNow();
}

// ---------- Wishes: three a day, one at a time. Each one earns a treat ----------
enum Wish : uint8_t { WI_PET, WI_FISH, WI_LASER, WI_BRUSH, WI_FOUNTAIN, WI_TREE, WI_CORNER, WI_WINDOW, WI_GAME, WI_BLINK, WI_PHOTO, NWISH };
static const char *const WISH_NAMES[NWISH] = {"some pets", "a fish", "the laser", "a brushing", "a drink", "her cat tree",
                                              "a cheek rub", "the window", "a mini-game", "a slow blink", "a photo"};
static const char *const WISH_HINTS[NWISH] = {
  "Stroke her head or back", "Dock: Feed, then Fish", "Dock: Play, then Laser", "Dock: Clean, then brush her",
  "Tap the fountain (living room)", "Tap the cat tree (living room)", "Tap the corner brush (living room)",
  "Tap the window", "Dock: Play, then Games", "Tap one of her eyes", "Dock: More, then Photo"};
static const uint8_t WISH_ICONS[NWISH] = {IC_HEART, IC_FISH, IC_LASER, IC_BRUSH, IC_DROP, IC_HOUSE, IC_SPARK, IC_BIRD, IC_PAD, IC_HEART, IC_CAMERA};
static bool bootWishes = true;                // no clock: one set of wishes each time the Pico starts

static void newWishes(int day) {
  for (int i = 0; i < 3; i++) {
    bool again = true;
    while (again) {
      P.wish[i] = random(0, NWISH);
      again = false;
      for (int j = 0; j < i; j++)
        if (P.wish[j] == P.wish[i]) again = true;
    }
  }
  P.wishDone = 0;
  P.wishDay = day;
  dirty = true;
}

// With internet time: a new set each day. With no clock: a new set each time the Pico starts
// (after a short wait, in case the time is still on its way).
static void checkWishes() {
  int d = today();
  if (d >= 0) {
    if (d != P.wishDay) newWishes(d);
    bootWishes = false;
  } else if (bootWishes && millis() > 20000) {
    if (P.wishDay < 0 || P.wishDone >= 3) newWishes(-1);
    bootWishes = false;
  }
}

static int wishNow() { return P.wishDone < 3 ? P.wish[P.wishDone] : -1; }

static void grant(uint8_t w) {
  if (wishNow() != w) return;
  P.wishDone++;
  P.treats++;
  char s[52];
  if (P.wishDone >= 3) {
    strlcpy(s, "All 3 wishes done! She has a gift for you", sizeof(s));
    giftOwed = true;
  } else {
    snprintf(s, sizeof(s), "Wish granted! +1 treat (%d/3)", P.wishDone);
  }
  toast(s, rgb(255, 200, 80), 3600);
  if (open) confetti(C.x, kitty::hit.top + 30, 36);
  addXP(10);
  saveNow();
}

// ---------- Background: needs, keyboard walk ----------
static uint32_t kbNext = 0, kbNextChar = 0, kbToastUntil = 0;
static char kbText[16] = "";
static uint8_t kbIdx = 0, kbLen = 0;

static void kbSchedule(uint32_t now, uint32_t inMs) { kbNext = now + inMs; }

static void kbStart(uint32_t now) {
  if (C.act == SLEEPING) {                   // asleep: no walking about
    kbSchedule(now, 10 * 60000UL);
    return;
  }
  static const char *const SOUNDS[] = {"mrrrp", "mrow", "prrrr", "mew", "nyaa"};
  char s[16];
  strlcpy(s, SOUNDS[random(0, 5)], sizeof(s));
  int mash = random(3, 7);                   // paws on random keys
  for (int i = 0; i < mash && strlen(s) < 14; i++) {
    char c[2] = {"jkl;asdf"[random(0, 8)], 0};
    strlcat(s, c, sizeof(s));
  }
  strlcpy(kbText, s, sizeof(kbText));
  kbLen = strlen(kbText);
  kbIdx = 0;
  kbNextChar = now + 600;
  kbToastUntil = now + 6000;
  if (open) {
    toFloor();
    setAct(KBWALK, 30);
  }
  kbSchedule(now, random(25, 61) * 60000UL);
}

static void begin() {
  load();
  began = true;
  lastSave = millis();
  kbSchedule(millis(), random(25, 61) * 60000UL);
  giftNext = millis() + random(20, 40) * 60000UL;
}

// Called every frame from the main loop, whatever app is open.
static void tick(uint32_t now, float dt) {
  if (!began) return;
  float h = dt / 3600;
  bool asleep = C.act == SLEEPING && C.stage >= 2;
  bump(P.hunger, -10 * h);
  bump(P.fun, (asleep ? -3 : -14) * h);
  bump(P.clean, -5 * h);
  bump(P.energy, (asleep ? 45 : -6) * h);
  bump(P.love, ((P.hunger < 20 || P.fun < 20) ? -8 : -3) * h);

  // keyboard walk
  if (P.kbWalk && (int32_t)(now - kbNext) >= 0) kbStart(now);
  if (kbIdx < kbLen && (int32_t)(now - kbNextChar) >= 0) {
    Keyboard.write((uint8_t)kbText[kbIdx++]);
    kbNextChar = now + random(90, 230);
  }

  if (dirty && now - lastSave > 5 * 60000UL) saveNow();
}

// ---------- Home screen helpers ----------
// one word on how she is, for the home screen (nullptr before start-up)
static const char *homeMood(uint16_t &col) {
  if (!began) return nullptr;
  if ((int32_t)(kbToastUntil - millis()) > 0) { col = rgb(255, 170, 60); return "Typed!"; }
  if (P.hunger < 25) { col = rgb(255, 160, 70); return "Hungry"; }
  if (P.energy < 15) { col = rgb(250, 210, 70); return "Sleepy"; }
  if (P.fun < 25) { col = rgb(255, 110, 170); return "Bored"; }
  if (P.clean < 25) { col = rgb(90, 200, 240); return "Messy"; }
  if (P.love < 25) { col = rgb(255, 90, 110); return "Lonely"; }
  if (C.act == SLEEPING) { col = rgb(170, 180, 255); return "Asleep"; }
  col = rgb(120, 230, 160);
  return P.hunger + P.energy + P.fun + P.clean + P.love > 350 ? "Happy" : "Okay";
}

// Now and then she peeks up from the bottom edge of the home screen.
static void homePeek(uint16_t *fb, uint32_t now) {
  static uint32_t next = 15000, start = 0;
  static float px = 250;
  if (!began) return;
  if (!start && (int32_t)(now - next) >= 0) {
    start = now;
    px = random(40, 280);
  }
  if (!start) return;
  float t = (now - start) / 1000.0f;
  if (t > 3.6f) {
    start = 0;
    next = now + random(40000, 80000);
    return;
  }
  float up = t < 0.6f ? t / 0.6f : (t > 3 ? (3.6f - t) / 0.6f : 1);
  up = up * up * (3 - 2 * up);
  float blink = (t > 1.6f && t < 1.8f) ? 0.05f : 1;
  kitty::drawHead(fb, px, H + 26 - up * 46, 0.55f, blink, sinf(t * 1.5f) * 0.8f);
}

static void appIcon(uint16_t *fb, int cx, int cy, uint16_t) {
  kitty::drawHead(fb, cx, cy + 4, 0.33f, 1, 0);
}

}  // namespace chindi

#include "chindi_play.h"
