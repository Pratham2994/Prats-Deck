// chindi_play.h
// Chindi's behaviour, touch, play modes, mini-games, photos and screens.
// Included at the end of app_chindi.h (same namespace).
#pragma once

namespace chindi {

using namespace kitty;   // poses, mouths, accessories

// ---------- Modes and sheets ----------
enum Mode : uint8_t { MD_NORMAL, MD_LASER, MD_FEATHER, MD_YARN, MD_BRUSH, MD_PHOTO, MD_GALLERY, MD_GAME, MD_ALBUM };
enum Sheet : uint8_t { SH_NONE, SH_FEED, SH_PLAY, SH_MORE, SH_GAMES, SH_WARDROBE, SH_ROOMS, SH_SETTINGS, SH_PROFILE };
static uint8_t mode = MD_NORMAL, sheet = SH_NONE;
static float sheetAnim = 0;
static uint32_t resetArm = 0;
static int albumTab = 0;

static const uint16_t ACCENT = rgb(255, 160, 70);
static const uint16_t STAT_COL[5] = {rgb(255, 160, 70), rgb(250, 210, 70), rgb(255, 110, 170), rgb(90, 200, 240), rgb(255, 90, 110)};

// ---------- Behaviour ----------
static float needIn = 15;

static bool lateNight() { return E.timeKnown && (E.hour >= 22.5f || E.hour < 6); }

// a bird lands on the window sill (or the balcony railing) for her to watch
static float birdY = 0;
static void callBird(uint32_t now, uint32_t ms) {
  if (E.night() || E.wet()) return;
  if (P.room == room::BALCONY) {
    birdX = random(70, 250);
    birdY = 103;
  } else {
    birdX = room::WIN_X + random(12, 36);          // left half of the window: she sits at the right
    birdY = room::WIN_Y + room::WIN_H;
  }
  birdFlying = false;
  birdUntil = now + ms;
}

static void birdLeaves() {
  if (birdX < 0 || birdFlying) return;
  if (P.room == room::BALCONY) birdFlying = true;        // open sky: it flies off
  else {                                                 // behind the glass: gone in a flutter
    for (int k = 0; k < 4; k++) emit(PK_FUR, birdX + frand(-4, 4), birdY - 6, frand(-20, 20), frand(-30, 0), 0.8f, rgb(134, 100, 72), 2);
    birdX = -100;
  }
}

static void pickGift() {
  int fresh[room::NGIFT], n = 0;
  for (int i = 0; i < room::NGIFT; i++)
    if (!(P.gifts & (1u << i))) fresh[n++] = i;
  giftItem = (n && random(0, 100) < 75) ? fresh[random(0, n)] : random(0, room::NGIFT);
}

static void chooseNext(uint32_t now) {
  if (P.energy < 18 || (lateNight() && P.energy < 90)) { setAct(SLEEPING, 1e9f); return; }
  if (giftItem < 0 && (giftOwed || ((int32_t)(now - giftNext) >= 0 && P.love > 55))) {
    giftOwed = false;
    giftNext = now + random(30, 60) * 60000UL;
    setAct(GIFTING, 4);
    return;
  }
  const room::Def &d = R();
  if (boxX < 0 && random(0, 100) < 7) {        // a box appears now and then
    boxX = random(d.boxLo, d.boxHi);
    boxUntil = now + 180000;
  }
  struct Wt { uint8_t a; float w; };
  Wt ws[24];
  int n = 0;
  auto add = [&](uint8_t a, float w) { if (w > 0 && n < 24) ws[n++] = {a, w}; };
  bool living = P.room == room::LIVING;
  add(IDLE, 2.5f);
  add(WANDER, 2.5f);
  add(LOAFING, 1.2f);
  add(FLOPPING, P.love > 40 ? 1.6f : 0.5f);
  add(GROOMING, P.clean < 60 ? 3 : 1);
  add(KNEADING, P.love > 50 ? 1.0f : 0.2f);
  add(STARING, 0.5f);
  add(ZOOMIES, P.energy > 55 ? (E.timeKnown && (E.hour < 5 || E.hour > 21) ? 1.8f : 0.35f) : 0);
  add(CUPPUSH, d.table && cupState == CUP_ON && P.fun < 85 ? 0.9f : 0);
  add(BOXSIT, boxX >= 0 ? 2.5f : 0);
  add(SUNBATHE, d.window && E.sunny() && P.energy < 85 ? 2 : 0);
  add(WINDOWWATCH, d.window ? (E.wet() ? 2.0f : 0.7f) : (!E.night() && !E.wet() ? 1.0f : 0));
  add(LAPTOP, pcBusy ? 4 : 0);
  add(DRINK, living ? (P.hunger < 70 ? 1.2f : 0.5f) : 0);
  add(CLIMB, living ? 1.0f : 0);
  add(SCRATCH, living ? (P.fun < 80 ? 1.0f : 0.4f) : 0);
  add(RUB, living ? (P.clean < 70 ? 1.0f : 0.4f) : 0);
  add(SNIFF, P.room == room::DINING ? 0.9f : 0);
  add(PLANTCHEW, P.room == room::BALCONY ? 0.9f : 0);
  add(BEDLOAF, P.room == room::BEDROOM ? 0.9f : 0);
  float total = 0;
  for (int i = 0; i < n; i++) total += ws[i].w;
  float r = frand(0, total);
  uint8_t pick = IDLE;
  for (int i = 0; i < n; i++) {
    if (r < ws[i].w) { pick = ws[i].a; break; }
    r -= ws[i].w;
  }
  switch (pick) {
    case IDLE: setAct(IDLE, frand(3, 7)); break;
    case LOAFING: setAct(LOAFING, frand(8, 16)); break;
    case FLOPPING: setAct(FLOPPING, frand(9, 18)); C.aux = 0; break;
    case GROOMING: setAct(GROOMING, frand(5, 8)); break;
    case KNEADING: setAct(KNEADING, frand(5, 7)); break;
    case STARING: setAct(STARING, frand(4, 6)); break;
    case BOXSIT: setAct(BOXSIT, frand(15, 35)); break;
    case SUNBATHE: setAct(SUNBATHE, frand(20, 40)); break;
    case WINDOWWATCH: setAct(WINDOWWATCH, frand(8, 14)); break;
    case LAPTOP: setAct(LAPTOP, frand(30, 60)); break;
    case CLIMB: setAct(CLIMB, frand(14, 24)); break;
    case BEDLOAF: setAct(BEDLOAF, frand(12, 22)); break;
    case PLANTCHEW: setAct(PLANTCHEW, 4); C.aux = random(0, 2); break;
    default: setAct(pick, 30);
  }
}

static void needsBubble() {
  if (P.hunger < 25) say("feed me!", IC_FISH, 2600);
  else if (P.energy < 20) say("so sleepy...", IC_ZZZ, 2600);
  else if (P.fun < 25) say("play with me?", IC_BALL, 2600);
  else if (P.clean < 25) say("brush me?", IC_SPARK, 2600);
  else if (P.love < 25) say("pet me?", IC_HEART, 2600);
}

static int litKey = -1;

// Jump up onto something at (x, base). Uses stages 0 (walk to it) and 1 (in the air).
// True on the frame she lands; the stage is then 2.
static bool jumpOnto(float x, float base, float dt) {
  if (C.stage == 0) {
    float from = constrain(x + (C.x < x ? -48.0f : 48.0f), 34.0f, 290.0f);
    if (walkTo(from, 70, dt)) {
      C.stage = 1;
      C.t = 0;
      hopUp(sqrtf(2 * 900 * (room::CAT_Y - base + 22)));
    }
    return false;
  }
  C.pose = POUNCE;
  C.L.stretch = 0.5f;
  C.left = x < C.x;
  C.x += (x - C.x) * (1 - expf(-dt / 0.14f));
  if (C.hopV < 0 && C.hop <= room::CAT_Y - base) {
    C.base = base;
    C.hop = 0;
    C.hopV = 0;
    C.x = x;
    C.stage = 2;
    C.t = 0;
    return true;
  }
  return false;
}

// Jump down to the floor, towards goalX. Call every frame. True when she is down.
static bool jumpOff(float goalX, float dt) {
  if (upHigh()) {                              // first call: leave the ledge
    C.hop = room::CAT_Y - C.base;
    C.base = room::CAT_Y;
    C.hopV = 150;
  }
  C.pose = POUNCE;
  C.L.stretch = 0.4f;
  C.left = goalX < C.x;
  C.x += (constrain(goalX, 34.0f, 290.0f) - C.x) * (1 - expf(-dt / 0.14f));
  return C.hop <= 0;
}

// acts during which she may be up on something
static bool highAct() {
  return C.act == CLIMB || C.act == SNIFF || C.act == BEDLOAF || C.act == LAPTOP || C.act == SLEEPING;
}

static void behave(float dt, uint32_t now) {
  Look &L = C.L;
  switch (C.act) {
    case IDLE:
      if (C.petting <= 0 && C.t > C.dur) chooseNext(now);
      needIn -= dt;
      if (needIn <= 0) {
        needIn = 25;
        needsBubble();
      }
      break;
    case WANDER:
      if (C.stage == 0) { C.goal = frand(40, 280); C.stage = 1; }
      if (walkTo(C.goal, C.stage == 2 ? 95 : 55, dt)) setAct(IDLE, frand(3, 6));
      break;
    case LOAFING:
      C.pose = LOAF;
      C.eyeT = 0.55f;
      if (C.t > C.dur) setAct(IDLE, 3);
      break;
    case FLOPPING:                             // flat on her side: her favourite
      if (C.stage == 0) {
        if (C.aux < 0.5f || walkTo(C.goal, 60, dt)) {
          C.stage = 1;
          C.t = 0;
          C.left = random(0, 2);
          discover(F_FLOP);
        }
        break;
      }
      C.pose = FLOP;
      C.eyeT = 0.75f;
      C.swingT = 1.5f;
      C.happy = sinf(C.t * 0.6f) > 0.7f;
      if (C.t > C.dur) setAct(IDLE, 3);
      break;
    case GROOMING:
      C.pose = GROOM;
      L.groom = 0.82f + 0.18f * sinf(C.t * 7);
      C.mouth = sinf(C.t * 7) > 0 ? M_TONGUE : M_CLOSED;
      C.eyeT = 0.35f;
      C.tiltT = 0.12f;
      bump(P.clean, 1.0f * dt);
      if (C.t > C.dur) setAct(IDLE, 3);
      break;
    case KNEADING:
      C.pose = KNEAD;
      L.pawL = fmaxf(0, sinf(C.t * 7));
      L.pawR = fmaxf(0, sinf(C.t * 7 + 3.1416f));
      C.happy = true;
      C.purrT = 0.5f;
      bump(P.love, 0.5f * dt);
      if (C.t > C.dur) setAct(LOAFING, 8);
      break;
    case STARING:
      if (C.stage == 0) {
        C.aux = random(0, 2) ? 1 : -1;
        say("", IC_Q, 1500);
        C.stage = 1;
      }
      C.lookXT = C.aux;
      C.lookYT = -0.9f;
      C.pupilT = 1;
      if (C.t > C.dur) setAct(IDLE, 3);
      break;
    case ZOOMIES:
      if (C.stage == 0) { C.goal = C.x < 160 ? 290 : 34; C.stage = 1; }
      if (C.stage <= 4) {
        C.pupilT = 1;
        C.earT = 0.3f;
        if (random(0, 100) < 40) emit(PK_DUST, C.x + (C.left ? 24 : -24), room::CAT_Y - 3, C.left ? 30 : -30, -10, 0.6f, rgb(200, 190, 175));
        if (walkTo(C.goal, 300, dt)) {
          C.stage++;
          C.goal = C.goal > 160 ? 34 : 290;
        }
      } else {
        if (C.stage == 5) {
          C.stage = 6;
          C.t = 0;
          say("!", IC_EXCL, 1200);
          bump(P.fun, 8);
          bump(P.energy, -5);
          addXP(2);
          discover(F_ZOOM);
        }
        C.pupilT = 1;
        if (C.t > 1.6f) setAct(IDLE, 3);
      }
      break;
    case CUPPUSH: {
      const float stand = cupX - 34;          // beside the table, paw reaches the cup
      if (C.stage == 0) {
        if (cupState != CUP_ON || !R().table) { setAct(IDLE, 2); break; }
        if (walkTo(stand, 60, dt)) { C.stage = 1; C.t = 0; C.aux = 0; C.left = false; }
      } else if (C.stage == 1) {
        C.pose = BAT;
        C.left = false;
        float s = sinf(C.t * 4.2f);
        L.bat = fmaxf(0, s);
        int pats = (int)(C.t * 4.2f / 6.2832f + 0.25f);
        if (pats > C.aux) {                   // each pat nudges the cup to the edge
          C.aux = pats;
          cupX += 7;
          if (pats >= 2) say("...", IC_NONE, 900);
        }
        C.x += (cupX - 34 - C.x) * (1 - expf(-dt / 0.2f));
        if (cupX > room::TABLE_X1 + 2) {
          cupState = CUP_FALLING;
          cupY = room::TABLE_TOP - 7;
          cupVx = 35;
          cupVy = -30;
          C.stage = 2;
          C.t = 0;
        }
      } else {
        C.happy = C.t > 0.8f;                 // looks at you, innocent
        if (C.t > 0.8f && C.stage == 2) {
          say("mrrp :)", IC_NONE, 2000);
          C.stage = 3;
        }
        if (C.t > 3.5f) {
          setAct(IDLE, 3);
          bump(P.fun, 6);
          addXP(2);
        }
      }
      break;
    }
    case BOXSIT:
      if (boxX < 0) { setAct(IDLE, 2); break; }
      if (C.stage == 0) {
        float stand = boxX + (C.x < boxX ? -50 : 50);
        if (walkTo(stand, 60, dt)) { C.stage = 1; C.t = 0; hopUp(320); }
      } else if (C.stage == 1) {
        C.pose = POUNCE;
        L.stretch = 0.5f;
        C.left = boxX < C.x;
        C.x += (boxX - C.x) * (1 - expf(-dt / 0.12f));
        if (C.t > 0.2f && C.hop <= 0) {
          C.stage = 2;
          C.t = 0;
          C.x = boxX;
          discover(F_BOX);
        }
      } else if (C.stage == 2) {
        L.inBox = true;
        C.happy = sinf(C.t * 0.8f) > 0.6f;
        if (C.t > C.dur) { C.stage = 3; C.t = 0; hopUp(320); C.goal = boxX + (boxX < 160 ? 56 : -56); }
      } else {
        C.pose = POUNCE;
        L.stretch = 0.5f;
        C.left = C.goal < C.x;
        C.x += (C.goal - C.x) * (1 - expf(-dt / 0.12f));
        if (C.t > 0.2f && C.hop <= 0) { boxUntil = millis() + 60000; setAct(IDLE, 3); }
      }
      break;
    case SUNBATHE: {
      if (!E.sunny()) { setAct(IDLE, 2); break; }
      float spot = constrain(E.sunX(), 60.0f, 260.0f);
      if (C.stage == 0) {
        if (walkTo(spot, 50, dt)) { C.stage = 1; C.t = 0; C.left = random(0, 2); }
      } else {
        C.pose = C.t > 5 ? FLOP : LOAF;       // she melts in the warm spot
        C.happy = true;
        bump(P.energy, 10 * dt / 60);
        if (C.t > C.dur) setAct(IDLE, 3);
      }
      break;
    }
    case WINDOWWATCH:
      if (C.stage == 0) {
        if (birdX < 0) callBird(now, (uint32_t)(C.dur * 1000) + 6000);
        // sit to one side of what she watches, so you can see it too
        float gx = R().window ? room::WIN_X + room::WIN_W + 30 : (birdX >= 0 ? birdX + (birdX < 200 ? 56 : -56) : 160);
        C.goal = gx;
        if (walkTo(constrain(gx, 40.0f, 280.0f), 55, dt)) { C.stage = 1; C.t = 0; }
      } else {
        float tx = birdX >= 0 ? birdX : room::WIN_X + room::WIN_W / 2;
        C.lookYT = -1;
        C.lookXT = (tx < C.x ? -0.8f : 0.8f) + 0.1f * sinf(C.t);
        C.tiltT = tx < C.x ? -0.1f : 0.1f;
        C.pupilT = 0.95f;
        C.swingT = 2;
        bool prey = birdX >= 0 || E.wet();                   // a bird, or the rain: she chatters at it
        if (fmodf(C.t, 3.2f) > 2.2f) {
          if (prey) C.mouth = sinf(C.t * 40) > 0 ? M_MEOW : M_CLOSED;
          if (C.stage == 1) {
            C.stage = 2;
            if (prey) say("ek ek ek", birdX >= 0 ? IC_BIRD : IC_NONE, 1400);
            if (birdX >= 0) discover(F_BIRD);
            grant(WI_WINDOW);
          }
        }
        if (C.t > C.dur) {
          birdLeaves();
          setAct(IDLE, 3);
        }
      }
      break;
    case LAPTOP:
      if (C.stage <= 1) {
        if (jumpOnto(R().laptopX - 30, room::CAT_Y - 10, dt)) {
          say("warm...", IC_HEART, 2200);
          discover(F_LAPTOP);
        }
      } else if (C.stage == 2) {
        C.pose = LOAF;
        C.happy = true;
        C.purrT = 0.3f;
        if (!pcBusy || C.t > C.dur) { C.stage = 3; C.t = 0; }
      } else if (jumpOff(R().laptopX - 84, dt) && C.t > 0.2f) {
        setAct(IDLE, 3);
      }
      break;
    case SLEEPING: {
      const room::Def &d = R();
      if (C.stage <= 1) {                      // go to bed: the perch, the cushion or the bed
        if (d.sleepY < room::CAT_Y) jumpOnto(d.sleepX, d.sleepY, dt);
        else if (walkTo(d.sleepX, 50, dt)) { C.stage = 2; C.t = 0; }
      } else if (C.stage == 2) {
        C.pose = CURL;
        C.eyeT = 0;
        if (fmodf(C.t, 1.3f) < dt) emit(PK_Z, C.x - 30, kitty::hit.top - 4, 0, -16, 2.2f, rgb(200, 210, 255), random(0, 2) ? 4 : 2);
        if (P.energy >= 99 && !lateNight()) { C.stage = 3; C.t = 0; }
      } else {
        C.pose = upHigh() ? LOAF : SIT;
        C.mouth = C.t < 1.4f ? M_YAWN : M_CLOSED;
        C.eyeT = C.t < 1.4f ? 0 : 1;
        if (C.t > 1.5f && C.stage == 3) { say("mrrow~", IC_HEART, 1800); C.stage = 4; }
        lightsOff = false;
        if (C.t > 2.5f) setAct(IDLE, 3);
      }
      break;
    }
    case EATING: {
      if (bowlX < 0) { setAct(IDLE, 2); break; }
      bool right = bowlX > C.x;
      if (C.stage == 0) {
        float stand = bowlX + (right ? -44 : 44);
        if (walkTo(stand, 70, dt)) { C.stage = 1; C.t = 0; C.left = !right; say("nom nom", IC_FISH, 1800); }
      } else if (C.stage == 1) {
        C.pose = EAT;
        L.headDown = fminf(1, C.t * 3);
        bowlFood = fmaxf(0, 1 - C.t / 4);
        if (random(0, 100) < 25) emit(PK_CRUMB, bowlX + frand(-8, 8), room::CAT_Y - 12, frand(-40, 40), frand(-120, -60), 0.8f, rgb(150, 95, 50), 2);
        if (C.t > 4) {
          C.stage = 2;
          C.t = 0;
          if (foodKind == 0) { bump(P.hunger, 30); }
          else if (foodKind == 1) { bump(P.hunger, 45); bump(P.love, 6); }
          else { bump(P.hunger, 12); bump(P.love, 12); bump(P.fun, 6); }
          P.meals++;
          addXP(5);
          if (foodKind == 1) grant(WI_FISH);
        }
      } else {
        C.happy = true;
        C.mouth = fmodf(C.t, 0.5f) < 0.25f ? M_TONGUE : M_CLOSED;
        if (C.t > 1.6f) {
          bowlX = -1;
          setAct(IDLE, 4);
        }
      }
      break;
    }
    case DRINK: {                              // the fountain
      const float stand = room::FOUNTAIN_X - 58;
      if (C.stage == 0) {
        if (walkTo(stand, 70, dt)) { C.stage = 1; C.t = 0; C.left = false; }
      } else if (C.stage == 1) {
        C.pose = EAT;
        C.left = false;
        L.headDown = fminf(0.6f, C.t * 2.4f);
        C.mouth = fmodf(C.t, 0.3f) < 0.15f ? M_TONGUE : M_CLOSED;
        if (random(0, 100) < 22) emit(PK_DROP, room::FOUNTAIN_X - 8 + frand(-4, 4), 150, frand(-30, 30), frand(-70, -20), 0.5f, rgb(160, 205, 255), 2);
        if (C.t > 3.6f) {
          C.stage = 2;
          C.t = 0;
          bump(P.hunger, 6);
          bump(P.clean, 2);
          bump(P.fun, 2);
          addXP(3);
          discover(F_FOUNTAIN);
          grant(WI_FOUNTAIN);
        }
      } else {
        C.happy = true;
        C.mouth = fmodf(C.t, 0.5f) < 0.25f ? M_TONGUE : M_CLOSED;
        if (C.t > 1.4f) setAct(IDLE, 3);
      }
      break;
    }
    case CLIMB:                                // up on the perch of her cat tree
      if (C.stage <= 1) {
        if (jumpOnto(room::PERCH_X, room::PERCH_Y, dt)) {
          say("mine.", IC_NONE, 1600);
          discover(F_TREE);
          grant(WI_TREE);
        }
      } else if (C.stage == 2) {
        C.pose = LOAF;
        C.eyeT = 0.6f;
        C.happy = sinf(C.t * 0.7f) > 0.5f;
        bump(P.energy, 4 * dt / 60);
        if (C.t > C.dur) { C.stage = 3; C.t = 0; }
      } else if (jumpOff(room::PERCH_X + 62, dt) && C.t > 0.2f) {
        setAct(IDLE, 3);
      }
      break;
    case SCRATCH: {                            // claws on the rope post
      const float stand = room::TREE_POST_X + 42;
      if (C.stage == 0) {
        if (walkTo(stand, 80, dt)) { C.stage = 1; C.t = 0; }
      } else {
        C.pose = BAT;
        C.left = true;
        L.bat = 0.55f + 0.45f * sinf(C.t * 16);
        C.pupilT = 0.8f;
        if (random(0, 100) < 35) emit(PK_FUR, room::TREE_POST_X + frand(-4, 6), kitty::hit.pawY + frand(-6, 6), frand(10, 40), frand(-10, 30), 0.7f, rgb(198, 152, 100), 2);
        if (C.t > 3) {
          setAct(IDLE, 3);
          bump(P.fun, 9);
          bump(P.energy, -2);
          addXP(3);
          discover(F_SCRATCH);
          grant(WI_TREE);
        }
      }
      break;
    }
    case RUB: {                                // her cheek on the corner brush
      const float stand = room::BRUSH_X - 66;
      if (C.stage == 0) {
        if (walkTo(stand, 70, dt)) { C.stage = 1; C.t = 0; }
      } else {
        C.pose = BAT;
        C.left = false;
        C.x = stand + 3 * sinf(C.t * 5);
        C.tiltT = 0.28f * sinf(C.t * 5);
        C.happy = true;
        C.purrT = 0.6f;
        if (fmodf(C.t, 0.6f) < dt) emit(PK_HEART, room::BRUSH_X - 14, 132, 0, -26, 1.2f, rgb(255, 90, 120), 8);
        if (random(0, 100) < 25) emit(PK_FUR, room::BRUSH_X - 4, 138 + frand(0, 16), frand(-30, -5), frand(-20, 10), 1.0f, random(0, 2) ? kitty::ORG : WHITE, 2);
        if (C.t > 4) {
          setAct(IDLE, 3);
          bump(P.clean, 12);
          bump(P.love, 5);
          addXP(3);
          discover(F_BRUSH);
          grant(WI_CORNER);
        }
      }
      break;
    }
    case SNIFF:                                // up on the dining table, at the flowers
      if (C.stage <= 1) {
        jumpOnto(room::DTABLE_X, room::DTABLE_Y, dt);
      } else if (C.stage == 2) {
        C.pose = BAT;
        C.left = false;
        C.lookXT = 1;
        C.tiltT = 0.06f * sinf(C.t * 18);     // nose working
        if (C.t > 1.6f) {
          C.stage = 3;
          C.t = 0;
          C.aux = random(0, 100) < 45 ? 1 : 0;
          discover(F_FLOWERS);
        }
      } else if (C.stage == 3) {
        C.pose = BAT;
        C.left = false;
        if (C.aux > 0) {                       // the pollen makes her sneeze
          if (C.t < 0.45f) {
            C.eyeT = 0.3f;
            C.tiltT = -0.15f;
            C.mouth = M_MEOW;
          } else {
            C.eyeT = 0;
            C.tiltT = 0.2f;
            if (C.aux < 1.5f) {
              C.aux = 2;
              say("achoo!", IC_NONE, 1400);
              flowerKick = 5;
              for (int k = 0; k < 5; k++)
                emit(PK_PETAL, room::VASE_X + frand(-14, 12), room::DTABLE_Y - frand(44, 62), 0, 12, 2.6f, rgb(240, 130, 176), frand(0, 6));
              discover(F_SNEEZE);
            }
          }
        } else {                               // she pats a flower
          L.bat = fmaxf(0, sinf(C.t * 5));
          if (L.bat > 0.9f && flowerKick < 1) {
            flowerKick = 6;
            emit(PK_PETAL, room::VASE_X - 12, room::DTABLE_Y - 52, 0, 12, 2.6f, rgb(240, 130, 176), frand(0, 6));
          }
        }
        if (C.t > 2.4f) {
          C.stage = 4;
          C.t = 0;
          bump(P.fun, 6);
          addXP(3);
        }
      } else if (jumpOff(room::DTABLE_X - 72, dt) && C.t > 0.2f) {
        setAct(IDLE, 3);
      }
      break;
    case PLANTCHEW: {                          // a nibble at a balcony plant
      bool leftPlant = C.aux < 0.5f;
      float px = room::PLANT_X[leftPlant ? 0 : 1], stand = leftPlant ? px + 64 : px - 64;
      if (C.stage == 0) {
        if (walkTo(stand, 70, dt)) { C.stage = 1; C.t = 0; }
      } else {
        C.pose = EAT;
        C.left = leftPlant;
        L.headDown = 0.12f;
        C.mouth = fmodf(C.t, 0.3f) < 0.15f ? M_MEOW : M_CLOSED;
        if (random(0, 100) < 18) emit(PK_FUR, px + frand(-8, 8), 126 + frand(-6, 6), frand(-30, 30), frand(-30, 0), 1.0f, rgb(90, 175, 95), 2);
        if (C.t > 3) {
          setAct(IDLE, 3);
          if (random(0, 100) < 50) say("bleh", IC_NONE, 1400);
          bump(P.fun, 5);
          addXP(2);
          discover(F_PLANT);
        }
      }
      break;
    }
    case BEDLOAF:                              // up on the bed
      if (C.stage <= 1) {
        if (jumpOnto(room::BED_X, room::BED_Y, dt)) discover(F_BED);
      } else if (C.stage == 2) {
        C.pose = C.t > 5 ? FLOP : LOAF;
        C.left = true;
        C.eyeT = 0.6f;
        C.happy = sinf(C.t * 0.7f) > 0.5f;
        bump(P.energy, 5 * dt / 60);
        if (C.t > C.dur) { C.stage = 3; C.t = 0; }
      } else if (jumpOff(room::BED_X - 76, dt) && C.t > 0.2f) {
        setAct(IDLE, 3);
      }
      break;
    case GIFTING:                              // she brings you something
      if (C.stage == 0) {
        C.goal = frand(100, 220);
        C.stage = 1;
      } else if (C.stage == 1) {
        if (walkTo(C.goal, 70, dt)) { C.stage = 2; C.t = 0; }
      } else {
        C.happy = C.t > 0.4f;
        if (C.t > 0.4f && C.stage == 2) {
          C.stage = 3;
          pickGift();
          giftX = constrain(C.x + (C.x < 160 ? 58.0f : -58.0f), 24.0f, 296.0f);
          giftUntil = now + 240000;
          burst(PK_SPARK, giftX, room::CAT_Y - 8, 10, 70, 0.7f, rgb(255, 220, 120));
          say("for you!", IC_GIFT, 2600);
        }
        if (C.t > 3) setAct(IDLE, 4);
      }
      break;
    case ANNOYED:
      if (C.stage == 0) {
        C.mouth = M_HISS;
        C.earT = 1;
        C.puffT = 1;
        C.eyeT = 0.7f;
        C.pupilT = 0.15f;
        if (C.t > 1.2f) { C.stage = 1; C.goal = C.x < 160 ? 280 : 40; }
      } else if (C.stage == 1) {
        C.earT = 0.8f;
        if (walkTo(C.goal, 110, dt)) { C.stage = 2; C.t = 0; }
      } else {
        C.earT = 0.8f;
        C.eyeT = 0.55f;
        C.swingT = 3.5f;
        C.lookXT = C.x > 160 ? 1 : -1;
        if (C.t > C.dur) setAct(IDLE, 3);
      }
      break;
    case SWAT:
      C.pose = BAT;
      L.bat = sinf(fminf(C.t / 0.45f, 1) * 3.1416f);
      C.earT = 0.8f;
      C.mouth = C.t < 0.3f ? M_HISS : M_CLOSED;
      if (C.t > 0.6f) {
        if (C.aux > 0) { setAct(ANNOYED, 15); C.stage = 2; }
        else setAct(IDLE, 3);
      }
      break;
    case SNEEZE:
      if (C.t < 0.5f) {
        C.eyeT = 0.3f;
        C.tiltT = -0.12f;
        C.mouth = M_MEOW;
      } else {
        C.tiltT = 0.22f;
        C.eyeT = 0;
        if (C.stage == 0) {
          C.stage = 1;
          say("achoo!", IC_NONE, 1400);
          burst(PK_DROP, kitty::hit.nx, kitty::hit.ny, 8, 120, 0.5f, rgb(170, 210, 255), 2);
          discover(F_SNEEZE);
        }
        if (C.t > 1.3f) setAct(IDLE, 3);
      }
      break;
    case KBWALK:
      if (C.stage == 0) {
        if (walkTo(60, 90, dt)) C.stage = 1;
      } else {
        bool done = walkTo(262, 36, dt);
        litKey = (C.x > 110 && C.x < 210) ? ((int)(C.x - 110) / 10 + (sinf(C.phase) > 0 ? 10 : 0)) : -1;
        if (done) {
          litKey = -1;
          setAct(IDLE, 3);
          say("mrrp :)", IC_NONE, 2000);
          discover(F_KEYS);
        }
      }
      break;
    case YAWN:
      C.mouth = M_YAWN;
      C.eyeT = 0;
      if (C.t > 1.8f) setAct(IDLE, 3);
      break;
    case REFUSE:
      C.tiltT = 0.35f;
      C.eyeT = 0.05f;
      if (C.t > 1.8f) setAct(IDLE, 3);
      break;
    case CELEBRATE:
      C.happy = true;
      if (C.hop <= 0 && C.t < 1.1f) hopUp(240);
      if (C.t > C.dur) setAct(IDLE, 3);
      break;
    default: break;
  }
}

// ---------- Touching Chindi ----------
enum Reg : uint8_t { R_NONE, R_HEAD, R_CHIN, R_EYE, R_NOSE, R_BODY, R_BELLY, R_TAIL };
static uint8_t touchReg = R_NONE;
static bool strokeCounted = false, tailPulled = false, bellyDecided = false;
static float heartIn = 0, petXP = 0, brushTime = 0;

static float dist(float ax, float ay, float bx, float by) { return sqrtf((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }

static uint8_t regionAt(float x, float y) {
  const Hit &h = hit;
  for (int k = 0; k < 4; k++)
    if (dist(x, y, h.tx[k], h.ty[k]) < 13) return R_TAIL;
  bool fr = frontPose() && !C.L.inBox;
  if (fr || C.L.inBox) {
    for (int e = 0; e < 2; e++)
      if (dist(x, y, h.ex[e], h.ey[e]) < 10) return R_EYE;
    if (dist(x, y, h.nx, h.ny) < 8) return R_NOSE;
    if (dist(x, y, h.cx, h.cy) < 12) return R_CHIN;
  }
  if (dist(x, y, h.hx, h.hy) < h.hr) return R_HEAD;
  float dx = (x - h.bx) / h.brx, dy = (y - h.by) / h.bry;
  if (dx * dx + dy * dy < 1.15f) {
    bool belly = (C.pose == SIT && y > h.by && fabsf(x - h.bx) < h.brx * 0.55f) || (C.pose == FLOP && y > h.by - 2);
    return fr && belly ? R_BELLY : R_BODY;
  }
  return R_NONE;
}

static bool busyAct() {
  switch (C.act) {
    case EATING: case ZOOMIES: case CUPPUSH: case BOXSIT: case LAPTOP: case KBWALK: case SWAT: case SNEEZE: case CELEBRATE:
    case DRINK: case SCRATCH: case RUB: case SNIFF: case PLANTCHEW: case GIFTING:
      return true;
    case CLIMB: case BEDLOAF: return C.stage != 2;
    case SLEEPING: return C.stage < 2;
    default: return false;
  }
}

// acts that petting does not interrupt: she stays as she is and enjoys it
static bool restingAct() {
  return C.act == IDLE || C.act == LOAFING || C.act == SUNBATHE || C.act == FLOPPING || C.act == CLIMB || C.act == BEDLOAF;
}

static void doSwat(bool annoyedAfter) {
  setAct(SWAT, 0.6f);
  C.aux = annoyedAfter ? 1 : 0;
  C.left = T.x < C.x;
  bump(P.love, -1);
  say("!", IC_EXCL, 1000);
}

static void stroke(uint8_t reg, float dt) {
  if (C.act == SLEEPING && C.stage == 2) {      // stroking in her sleep: a sleepy purr
    bump(P.love, 3 * dt);
    C.purrT = 0.6f;
  } else if (C.act == ANNOYED || C.act == SWAT) {
    if (C.act == ANNOYED && !strokeCounted) doSwat(true);
    strokeCounted = true;
    return;
  } else {
    if (reg == R_BELLY && !bellyDecided && mode != MD_BRUSH) {
      bellyDecided = true;
      if (random(0, 100) < 35 && !upHigh()) {
        doSwat(false);
        strokeCounted = true;
        discover(F_BELLY);
        return;
      }
    }
    if (C.act == SWAT) return;
    if (!busyAct() && !restingAct()) setAct(IDLE, 4);
    if (C.act == IDLE) C.t = 0;
    if (mode == MD_BRUSH) {
      bump(P.clean, 10 * dt);
      bump(P.love, 1.5f * dt);
      if (random(0, 100) < 50) emit(PK_FUR, T.x, T.y, frand(-30, 30), frand(-40, -10), 1.4f, random(0, 2) ? kitty::ORG : WHITE, 2);
      if (random(0, 100) < 30) emit(PK_SPARK, T.x + frand(-10, 10), T.y + frand(-10, 10), 0, -10, 0.6f, rgb(200, 240, 255));
      brushTime += dt;
      if (brushTime > 2) {
        brushTime = 0;
        grant(WI_BRUSH);
      }
    } else {
      bump(P.love, 5 * dt);
      bump(P.fun, 1 * dt);
    }
    C.petting = 0.6f;
    if (reg == R_CHIN) C.tiltT = -0.22f;
    else if (reg == R_HEAD) C.tiltT = constrain((T.x - hit.hx) / 200, -0.2f, 0.2f);
    else C.swingT = 0.4f;
  }
  heartIn -= dt;
  if (heartIn <= 0 && mode != MD_BRUSH) {
    emit(PK_HEART, T.x, T.y - 10, 0, -30, 1.4f, rgb(255, 90, 120), 9);
    heartIn = 0.35f;
  }
  if (!strokeCounted) {
    strokeCounted = true;
    P.pets++;
    touchDay();
  }
  petXP += dt;
  if (petXP > 3) {
    petXP = 0;
    addXP(2);
    if (mode != MD_BRUSH) grant(WI_PET);
  }
}

static void onTap(uint8_t reg) {
  if (C.act == SLEEPING && C.stage == 2) {
    if (P.energy < 50) {
      say("mrrr...", IC_ZZZ, 1500);            // grumpy, goes back to sleep
      C.eyeT = 0.4f;
    } else {
      C.stage = 3;
      C.t = 0;
    }
    return;
  }
  if (busyAct() || C.act == ANNOYED) return;
  touchDay();
  switch (reg) {
    case R_EYE:
      C.slowBlink = 1.4f;
      bump(P.love, 4);
      emit(PK_HEART, hit.hx, hit.top, 0, -30, 1.6f, rgb(255, 90, 120), 11);
      say("", IC_HEART, 1500);
      addXP(2);
      discover(F_BLINK);
      grant(WI_BLINK);
      break;
    case R_NOSE:
      bump(P.love, 1);
      addXP(1);
      if (random(0, 100) < 40 && !upHigh()) setAct(SNEEZE, 1.3f);
      else {
        C.blinkLeft = 0.25f;
        say("mrrp!", IC_NONE, 1200);
      }
      break;
    case R_TAIL: say("hm?", IC_Q, 1000); C.swingT = 3; break;
    default:
      say(random(0, 2) ? "mrrp?" : "mew", IC_NONE, 1200);
      C.L.twitchL = 1;
      if (!restingAct()) setAct(IDLE, 4);
  }
}

static void collectGift() {
  bool isNew = !(P.gifts & (1u << giftItem));
  P.gifts |= 1u << giftItem;
  char s[52];
  if (isNew) snprintf(s, sizeof(s), "New gift: %s (%d/%d)", room::GIFT_NAMES[giftItem], countBits(P.gifts), room::NGIFT);
  else {
    P.treats++;
    snprintf(s, sizeof(s), "%s again: +1 treat", room::GIFT_NAMES[giftItem]);
  }
  toast(s, rgb(255, 214, 120), 3400);
  burst(PK_SPARK, giftX, room::CAT_Y - 8, 14, 110, 0.7f, rgb(255, 220, 120));
  giftItem = -1;
  bump(P.love, 6);
  if (!busyAct() && C.act != SLEEPING) say("purr", IC_HEART, 1600);
  addXP(isNew ? 12 : 4);
  saveNow();
}

// You tapped something in the room: she goes there and uses it.
static void useSpot(uint8_t spot, float x, float y) {
  if (spot == room::SP_SWITCH) {               // the light works whatever she is doing
    lightsOff = !lightsOff;
    toast(lightsOff ? "Lights off" : "Lights on", rgb(255, 230, 150), 1400);
    discover(F_SWITCH);
    return;
  }
  if (spot == room::SP_FRAMES) {
    discover(F_FRAMES);
    mode = MD_GALLERY;
    return;
  }
  if (spot == room::SP_DOOR) {
    discover(F_DOOR);
    sheet = SH_ROOMS;
    sheetAnim = 0;
    return;
  }
  if (C.act == SLEEPING) {
    say("zzz", IC_ZZZ, 1200);
    return;
  }
  if (busyAct() || C.act == ANNOYED) return;
  touchDay();
  switch (spot) {
    case room::SP_FOUNTAIN: setAct(DRINK, 6); break;
    case room::SP_TREE:
      if (C.act == CLIMB && C.stage == 2) { C.stage = 3; C.t = 0; }       // tap again: she comes down
      else if (x < 42 && y > 96) setAct(SCRATCH, 4);
      else setAct(CLIMB, frand(14, 24));
      break;
    case room::SP_BRUSH: setAct(RUB, 5); break;
    case room::SP_WINDOW: setAct(WINDOWWATCH, 9); break;
    case room::SP_FLOWERS: setAct(SNIFF, 8); break;
    case room::SP_BED:
      if (C.act == BEDLOAF && C.stage == 2) { C.stage = 3; C.t = 0; }
      else setAct(BEDLOAF, frand(12, 22));
      break;
    case room::SP_PLANT:
      setAct(PLANTCHEW, 4);
      C.aux = x < 160 ? 0 : 1;
      break;
    case room::SP_CUP:
      if (cupState == CUP_ON) setAct(CUPPUSH, 30);
      break;
    case room::SP_CUSHION:
      setAct(FLOPPING, frand(10, 18));
      C.aux = 1;
      C.goal = R().sleepX;
      break;
    default: break;
  }
}

static bool onGift(float x, float y) { return giftItem >= 0 && dist(x, y, giftX, room::CAT_Y - 9) < 20; }

// a tap that was not on her
static void roomTap(float x, float y) {
  if (onGift(x, y)) {
    collectGift();
    return;
  }
  if (birdX >= 0 && !birdFlying && dist(x, y, birdX, birdY - 6) < 16) {
    birdLeaves();
    if (!busyAct() && C.act != SLEEPING) say("!", IC_BIRD, 1000);
    return;
  }
  emit(PK_RING, x, y, 0, 0, 0.45f, WHITE, 16);
  uint8_t spot = room::spotAt(P.room, x, y);
  if (spot) {
    useSpot(spot, x, y);
    return;
  }
  // a tap on the open floor: she comes over
  if (y > room::FLOOR_Y + 8 && !busyAct() && C.act != SLEEPING && C.act != ANNOYED) {
    setAct(WANDER, 4);
    C.stage = 2;
    C.goal = constrain(x, 34.0f, 290.0f);
  }
}

static void interact(float dt) {
  if (T.pressed) {
    touchReg = (inHomeCorner(T.startX, T.startY) || onGift(T.startX, T.startY)) ? (uint8_t)R_NONE : regionAt(T.startX, T.startY);
    strokeCounted = tailPulled = bellyDecided = false;
  }
  if (T.down && touchReg) {
    float sp = sqrtf(T.vx * T.vx + T.vy * T.vy);
    if (touchReg == R_TAIL) {
      if (!tailPulled && T.moved > 14 && C.act != SLEEPING && !upHigh()) {
        tailPulled = true;
        setAct(ANNOYED, 25);
        bump(P.love, -6);
        say("HSSS!", IC_EXCL, 1500);
      }
    } else if (T.moved > 8 && sp > 25 && regionAt(T.x, T.y) != R_NONE) {
      stroke(touchReg, dt);
    }
  }
  if (T.tap) {
    if (touchReg) onTap(touchReg);
    else if (mode == MD_NORMAL && !inHomeCorner(T.startX, T.startY)) roomTap(T.startX, T.startY);
  }
  if (T.released) touchReg = R_NONE;
}

// ---------- Commands from the dock ----------
static void feed(uint8_t kind) {
  touchDay();
  if (kind == 2 && P.treats == 0) {
    toast("No treats left. Grant her wishes!", rgb(255, 170, 60));
    return;
  }
  if (P.hunger > 92 && !(kind == 1 && wishNow() == WI_FISH)) {
    setAct(REFUSE, 1.8f);
    say("not hungry", IC_FISH, 1800);
    return;
  }
  if (kind == 2) P.treats--;
  foodKind = kind;
  bowlX = constrain(C.x + (C.x < 160 ? 80 : -80), 60.0f, 260.0f);
  bowlFood = 1;
  setAct(EATING, 10);
}

static void sleepCmd() {
  if (P.energy > 85 && !lateNight()) {
    setAct(REFUSE, 1.8f);
    say("not sleepy!", IC_EXCL, 1800);
    return;
  }
  lightsOff = true;
  setAct(SLEEPING, 1e9f);
}

// ---------- Play modes ----------
static float laserX = -100, laserY = 0, laserSeen = 0, laserStill = 0, lastLX = 0, lastLY = 0;

static void laserMode(float dt, bool ui) {
  Look &L = C.L;
  if (T.down && !ui) {
    laserX = T.x;
    laserY = T.y;
    laserSeen = 1.2f;
  } else {
    laserSeen -= dt;
  }
  float mv = dist(laserX, laserY, lastLX, lastLY);
  lastLX = laserX;
  lastLY = laserY;
  laserStill = mv < 1.5f ? laserStill + dt : 0;
  C.act = PLAY;
  C.t += dt;
  if (laserSeen <= 0 && C.stage == 0) return;
  float tx = constrain(laserX, 34.0f, 290.0f), dx = tx - C.x;
  C.pupilT = 1;
  C.earT = -0.15f;
  C.lookXT = constrain(dx / 60, -1.0f, 1.0f);
  C.lookYT = constrain((laserY - hit.hy) / 60, -1.0f, 1.0f);
  if (C.stage == 0) {
    if (fabsf(dx) > 70) walkTo(tx, 230, dt);
    else if (fabsf(dx) > 24) walkTo(tx, 90, dt);
    bump(P.fun, 6 * dt);
    bump(P.energy, -2 * dt);
    if (laserStill > 0.35f && fabsf(dx) < 110 && C.hop <= 0) {
      C.stage = 1;
      C.t = 0;
      C.left = dx < 0;
    }
  } else if (C.stage == 1) {                   // wiggle, then pounce
    C.pose = CROUCH;
    L.crouch = 1;
    L.wiggle = sinf(C.t * 28) * 3;
    if (C.t > 0.7f) {
      C.stage = 2;
      C.t = 0;
      hopUp(laserY < room::FLOOR_Y ? 430 : 270);
      C.goal = tx;
    }
  } else {
    C.pose = POUNCE;
    L.stretch = 1;
    C.x += (C.goal - C.x) * (1 - expf(-dt / 0.1f));
    if (C.t > 0.15f && C.hop <= 0) {
      C.stage = 0;
      burst(PK_SPARK, laserX, laserY, 10, 80, 0.5f, rgb(255, 80, 80));
      bump(P.fun, 3);
      addXP(1);
      grant(WI_LASER);
    }
  }
}

static const int NF = 8;
static float fX[NF], fY[NF], fOX[NF], fOY[NF];
static bool featherReady = false;
static float wandX = 230, wandY = 50, batCool = 0;

static void featherMode(float dt, bool ui) {
  Look &L = C.L;
  if (!featherReady) {
    for (int i = 0; i < NF; i++) { fX[i] = fOX[i] = wandX; fY[i] = fOY[i] = wandY + i * 9; }
    featherReady = true;
  }
  if (T.down && !ui) { wandX = T.x; wandY = T.y; }
  fX[0] = fOX[0] = wandX;
  fY[0] = fOY[0] = wandY;
  for (int i = 1; i < NF; i++) {               // string physics
    float vx = (fX[i] - fOX[i]) * 0.985f, vy = (fY[i] - fOY[i]) * 0.985f;
    fOX[i] = fX[i];
    fOY[i] = fY[i];
    fX[i] += vx;
    fY[i] += vy + 900 * dt * dt;
  }
  for (int it = 0; it < 4; it++)
    for (int i = 1; i < NF; i++) {
      float dx = fX[i] - fX[i - 1], dy = fY[i] - fY[i - 1], d = sqrtf(dx * dx + dy * dy) + 1e-4f, k = (d - 9) / d;
      if (i > 1) { fX[i - 1] += dx * k * 0.5f; fY[i - 1] += dy * k * 0.5f; fX[i] -= dx * k * 0.5f; fY[i] -= dy * k * 0.5f; }
      else { fX[i] -= dx * k; fY[i] -= dy * k; }
    }
  float tipX = fX[NF - 1], tipY = fY[NF - 1];
  C.act = PLAY;
  C.t += dt;
  batCool -= dt;
  C.pupilT = 1;
  C.lookXT = constrain((tipX - hit.hx) / 60, -1.0f, 1.0f);
  C.lookYT = constrain((tipY - hit.hy) / 60, -1.0f, 1.0f);
  float dx = tipX - C.x;
  if (C.hop > 0) {
    C.pose = POUNCE;
    L.stretch = 0.8f;
    C.x += (constrain(tipX, 34.0f, 290.0f) - C.x) * (1 - expf(-dt / 0.15f));
  } else if (fabsf(dx) > 46) {
    walkTo(constrain(tipX - (dx > 0 ? 34 : -34), 34.0f, 290.0f), fabsf(dx) > 100 ? 200 : 90, dt);
  } else if (tipY > 120) {
    C.pose = BAT;
    C.left = dx < 0;
    L.bat = fmaxf(0, sinf(C.t * 7));
    if (L.bat > 0.7f && batCool <= 0 && dist(hit.pawX, hit.pawY, tipX, tipY) < 26) {
      fOX[NF - 1] -= (C.left ? -1 : 1) * 7;
      fOY[NF - 1] += 5;
      batCool = 0.4f;
      emit(PK_SPARK, tipX, tipY, 0, -10, 0.5f, rgb(255, 200, 240));
      bump(P.fun, 2);
      addXP(1);
    }
  } else if (tipY > 60 && random(0, 1000) < 15) {
    hopUp(380);
  }
  bump(P.fun, 3 * dt);
  bump(P.energy, -1.5f * dt);
}

static float yX = 220, yY = 0, yVx = 0, yVy = 0, yRot = 0;
static bool yHeld = false;
static float trailX[16], trailY[16];
static int trailN = 0;
static float trailIn = 0;

static void yarnMode(float dt, bool ui) {
  Look &L = C.L;
  const float floorY = room::CAT_Y - 9;
  if (yY == 0) yY = floorY;
  if (T.pressed && !ui && dist(T.x, T.y, yX, yY) < 28) yHeld = true;
  if (yHeld) {
    if (T.down) {
      yX = T.x;
      yY = fminf(T.y, floorY);
    } else {
      yHeld = false;
      yVx = constrain(T.relVx * 0.9f, -700.0f, 700.0f);
      yVy = constrain(T.relVy * 0.9f, -700.0f, 700.0f);
    }
  } else {
    yVy += 900 * dt;
    yX += yVx * dt;
    yY += yVy * dt;
    if (yY >= floorY) {
      yY = floorY;
      yVy = fabsf(yVy) > 60 ? -yVy * 0.45f : 0;
      yVx *= powf(0.3f, dt);
    }
    if (yX < 12) { yX = 12; yVx = fabsf(yVx) * 0.7f; }
    if (yX > W - 12) { yX = W - 12; yVx = -fabsf(yVx) * 0.7f; }
    if (yY < 30) { yY = 30; yVy = fabsf(yVy); }
  }
  yRot += yVx * dt / 9;
  trailIn -= dt;
  if (trailIn <= 0) {
    trailIn = 0.03f;
    memmove(trailX + 1, trailX, sizeof(float) * 15);
    memmove(trailY + 1, trailY, sizeof(float) * 15);
    trailX[0] = yX;
    trailY[0] = yY;
    if (trailN < 16) trailN++;
  }
  C.act = PLAY;
  C.t += dt;
  batCool -= dt;
  C.pupilT = 1;
  C.lookXT = constrain((yX - hit.hx) / 60, -1.0f, 1.0f);
  C.lookYT = constrain((yY - hit.hy) / 60, -1.0f, 1.0f);
  float dx = yX - C.x;
  float speed = sqrtf(yVx * yVx + yVy * yVy);
  if (fabsf(dx) > 34) {
    walkTo(constrain(yX - (dx > 0 ? 30 : -30), 34.0f, 290.0f), fabsf(dx) > 80 ? 220 : 100, dt);
    bump(P.fun, 3 * dt);
  } else if (!yHeld && yY > floorY - 20 && speed < 140 && batCool <= 0) {
    C.pose = BAT;
    C.left = dx < 0;
    L.bat = 1;
    yVx = (C.left ? -1 : 1) * frand(160, 300);
    yVy = -frand(80, 220);
    batCool = 0.7f;
    emit(PK_SPARK, yX, yY, 0, -10, 0.5f, rgb(255, 160, 200));
    bump(P.fun, 4);
    addXP(1);
  } else if (batCool > 0.4f) {
    C.pose = BAT;
    C.left = dx < 0;
    L.bat = (batCool - 0.4f) / 0.3f;
  }
  bump(P.energy, -1.5f * dt);
}

// ---------- Photos ----------
static uint32_t flashAt = 0;

static void savePhoto() {
  Photo p;
  p.room = P.room;
  p.pose = C.L.pose;
  p.mouth = C.L.mouth;
  p.acc = P.acc;
  p.flags = (C.L.flip ? 1 : 0) | (C.L.happy ? 2 : 0) | (C.L.inBox ? 4 : 0) | (C.L.glasses ? 8 : 0);
  p.wx = E.wx;
  p.eye = (uint8_t)(constrain(C.L.eyeOpen, 0.0f, 1.0f) * 100);
  p.base = (uint8_t)constrain(C.base - C.hop, 0.0f, 255.0f);
  p.x = (int16_t)C.x;
  p.hourQ = (int16_t)(E.hour * 10);
  p.day = today();
  if (P.nPhotos == NPHOTO) {
    memmove(P.photos, P.photos + 1, sizeof(Photo) * (NPHOTO - 1));
    P.nPhotos--;
  }
  P.photos[P.nPhotos++] = p;
  flashAt = millis();
  char s[40];
  snprintf(s, sizeof(s), "Saved to gallery (%d/%d)", P.nPhotos, NPHOTO);
  toast(s, rgb(120, 220, 255));
  addXP(3);
  grant(WI_PHOTO);
  saveNow();
}

// ---------- Lights off ----------
// The room is never dark by the clock: at night it stays at full brightness. Only her light
// switch (or Sleep) dims it, and only to 75%, because half brightness is too dark on the screen.
static void dimRoom(uint16_t *fb) {
  for (uint16_t *p = fb, *end = fb + W * H; p < end; p++) *p = *p - dim4(*p);
}

// the things that stand in a room but are not part of its background picture
static void drawProps(int rm, const room::Env &e, bool cupOn, float cx, float sway) {
  const room::Def &d = room::DEFS[rm];
  if (d.window) room::sunbeam(e);
  if (d.cushion) room::cushion(d.sleepX);
  if (d.table) room::table(cupOn, cx);
  if (rm == room::DINING) room::flowers(sway);
}

// ---------- UI: stats card, wish chip, dock, sheets ----------
// The 5 stat rings have the same shape. How much of each pixel the ring covers, and the angle
// of the pixel, are worked out once. After that a ring is a walk through two small tables,
// with no square root and no arctangent.
static const int RING_R0 = 9, RING_R1 = 12, RING_N = 2 * RING_R1 + 1;
static uint8_t ringCov[RING_N * RING_N], ringAng[RING_N * RING_N];   // cover 0..255; angle 0..255 = one turn from the top
static void ringTables() {
  for (int y = -RING_R1, i = 0; y <= RING_R1; y++)
    for (int x = -RING_R1; x <= RING_R1; x++, i++) {
      float d = sqrtf((float)(x * x + y * y));
      ringCov[i] = (uint8_t)(constrain(fminf(d - RING_R0, RING_R1 - d) + 0.5f, 0.0f, 1.0f) * 255);
      float a = atan2f((float)x, (float)-y);
      if (a < 0) a += 6.2831853f;
      ringAng[i] = (uint8_t)min(255, (int)(a * (256 / 6.2831853f)));
    }
}

// a grey ring, with the part from the top to v percent in a colour that goes c0 -> c1
static void ring(uint16_t *fb, int cx, int cy, float v, uint16_t c0, uint16_t c1) {
  if (!ringCov[RING_R1]) ringTables();             // the top pixel of the ring: 0 until the tables are made
  if (cx < RING_R1 || cx >= W - RING_R1 || cy < RING_R1 || cy >= H - RING_R1) return;
  const uint16_t base = rgb(40, 44, 62);
  const int end = v > 1 ? (int)(v * 2.56f) : 0;    // the colour goes up to this angle
  const uint8_t *cov = ringCov, *ang = ringAng;
  for (int y = -RING_R1; y <= RING_R1; y++) {
    uint16_t *p = fb + (cy + y) * W + cx - RING_R1;
    for (int x = 0; x < RING_N; x++, cov++, ang++) {
      if (*cov < 6) continue;
      uint16_t c = *ang < end ? blend(c0, c1, *ang * 256 / end) : base;
      p[x] = *cov >= 250 ? c : blend(p[x], c, *cov + 1);
    }
  }
}

static void statRing(uint16_t *fb, int cx, int cy, float v, int k, uint32_t now) {
  uint16_t col = STAT_COL[k];
  if (v < 25) col = blend(col, rgb(255, 60, 60), (int)(128 + 127 * sinf(now / 160.0f)));
  ring(fb, cx, cy, v, dim(col), col);
  switch (k) {
    case 0:                                    // food
      fillCircle(fb, cx - 1, cy, 3, col);
      fillTriangle(fb, cx + 1, cy, cx + 5, cy - 3, cx + 5, cy + 3, col);
      break;
    case 1:                                    // energy
      fillTriangle(fb, cx + 1, cy - 5, cx - 3, cy + 1, cx + 1, cy + 1, col);
      fillTriangle(fb, cx - 1, cy - 1, cx + 3, cy - 1, cx - 1, cy + 5, col);
      break;
    case 2: fillCircle(fb, cx, cy, 3, col); break;   // fun
    case 3:                                    // clean
      fillTriangle(fb, cx, cy - 5, cx - 2, cy, cx + 2, cy, col);
      fillTriangle(fb, cx, cy + 5, cx - 2, cy, cx + 2, cy, col);
      fillTriangle(fb, cx - 5, cy, cx, cy - 2, cx, cy + 2, col);
      fillTriangle(fb, cx + 5, cy, cx, cy - 2, cx, cy + 2, col);
      break;
    default:                                   // love
      fillCircle(fb, cx - 2, cy - 1, 2, col);
      fillCircle(fb, cx + 2, cy - 1, 2, col);
      fillTriangle(fb, cx - 4, cy, cx + 4, cy, cx, cy + 4, col);
  }
}

static void statsCard(uint16_t *fb, uint32_t now) {
  shadeRoundRect(fb, 30, 3, 286, 34, 11);
  roundRect(fb, 30, 3, 286, 34, 11, rgb(62, 66, 88));
  float v[5] = {P.hunger, P.energy, P.fun, P.clean, P.love};
  for (int k = 0; k < 5; k++) statRing(fb, 48 + k * 30, 20, v[k], k, now);
  int lv = level();
  text(fb, SMALL, "Chindi", 202, 19, INK);
  char s[12];
  snprintf(s, sizeof(s), "LV %d", lv);
  int pw = tinyWidth(s) + 12;
  fillRoundRect(fb, 308 - pw, 8, pw, 14, 7, blend(CARD, ACCENT, 120));
  tiny(fb, s, 314 - pw, 11, WHITE);
  float frac = lv >= 10 ? 1 : (float)(P.xp - LV[lv - 1]) / (LV[lv] - LV[lv - 1]);
  fillRoundRect(fb, 202, 27, 106, 4, 2, rgb(44, 48, 66));
  if (frac > 0.02f) fillRoundRect(fb, 202, 27, max(4, (int)(106 * frac)), 4, 2, ACCENT);
  if (tapIn(30, 3, 286, 34) && mode == MD_NORMAL) sheet = sheet == SH_PROFILE ? SH_NONE : SH_PROFILE, sheetAnim = 0;
}

// what she wishes for now. Tap it for a hint.
static const int CHIP_X = 30, CHIP_Y = 41;
static int chipWidth() {
  int w = wishNow();
  return w < 0 ? 0 : tinyWidth("wants ") + tinyWidth(WISH_NAMES[w]) + 30;
}

static void wishChip(uint16_t *fb, uint32_t now) {
  int w = wishNow();
  if (w < 0) return;
  int x = CHIP_X, cw = chipWidth();
  const uint16_t col = rgb(255, 214, 120);
  shadeRoundRect(fb, x, CHIP_Y, cw, 18, 9);
  roundRect(fb, x, CHIP_Y, cw, 18, 9, blend(CARD_EDGE, col, 120 + (int)(60 * sinf(now / 500.0f))));
  icon(fb, WISH_ICONS[w], x + 12, CHIP_Y + 9, col);
  int tx = text(fb, TINY, "wants ", x + 24, CHIP_Y + 12, MUTED);
  text(fb, TINY, WISH_NAMES[w], tx, CHIP_Y + 12, INK);
  if (tapIn(x, CHIP_Y - 2, cw, 22) && mode == MD_NORMAL) toast(WISH_HINTS[w], col, 3200);
}

// where her speech bubble goes. false: there is no bubble now
static bool bubbleBox(uint32_t now, int &x, int &y, int &w, int &h) {
  if ((int32_t)(bubbleUntil - now) <= 0) return false;
  w = (bubbleText[0] ? textWidth(SMALL, bubbleText) : 0) + (bubbleIcon ? 20 : 0) + 18;
  h = 24;
  x = constrain((int)hit.hx - w / 2, 4, W - 4 - w);
  y = max(42, (int)hit.top - h - 8);
  return true;
}

static void drawBubble(uint16_t *fb, uint32_t now) {
  int x, y, w, h;
  if (!bubbleBox(now, x, y, w, h)) return;
  int tw = bubbleText[0] ? textWidth(SMALL, bubbleText) : 0;
  int iw = bubbleIcon ? 20 : 0;
  fillRoundRect(fb, x + 1, y + 2, w, h, 12, rgb(40, 30, 30));      // soft shadow
  fillRoundRect(fb, x, y, w, h, 12, WHITE);
  int tx = constrain((int)hit.hx, x + 12, x + w - 12);
  fillTriangle(fb, tx - 5, y + h - 1, tx + 5, y + h - 1, tx, y + h + 6, WHITE);
  if (bubbleIcon) icon(fb, bubbleIcon, x + 9 + iw / 2, y + h / 2, bubbleIcon == IC_HEART ? rgb(255, 80, 110) : rgb(255, 140, 50));
  if (tw) text(fb, SMALL, bubbleText, x + 9 + iw, y + 17, rgb(50, 40, 40));
}

static const int TOAST_Y = 40, TOAST_H = 22;     // the toast takes the row of the wish chip
static void drawToast(uint16_t *fb, uint32_t now) {
  if ((int32_t)(toastUntil - now) <= 0) return;
  int w = textWidth(SMALL, toastText) + 24;
  int x = (W - w) / 2, y = TOAST_Y;
  fillRoundRect(fb, x, y, w, TOAST_H, 11, CARD);
  roundRect(fb, x, y, w, TOAST_H, 11, toastCol);
  text(fb, SMALL, toastText, x + 12, y + 16, toastCol);
}

// The space under the stats bar shows one message at a time, so that they do not pile up
// over her face: a toast first, then her speech bubble, then the wish chip.
static bool chipShown = false;
static void messages(uint16_t *fb, uint32_t now, bool chip) {
  bool toastUp = (int32_t)(toastUntil - now) > 0;
  int bx, by, bw, bh;
  bool bubbleUp = bubbleBox(now, bx, by, bw, bh);
  if (bubbleUp && toastUp && by < TOAST_Y + TOAST_H + 2) bubbleUp = false;
  chipShown = chip && !toastUp && !(bubbleUp && bx < CHIP_X + chipWidth() + 2 && by < CHIP_Y + 20);
  if (chipShown) wishChip(fb, now);
  if (bubbleUp) drawBubble(fb, now);
  drawToast(fb, now);
}

struct Tile {
  const char *label;
  char sub[24];
  uint8_t icon;
  uint16_t col;
  bool locked, selected;
};

// the panel that slides up from the bottom. Returns its top edge.
static int sheetPanel(uint16_t *fb, int ph, const char *title) {
  sheetAnim = fminf(1, sheetAnim + 0.12f);
  float e = 1 - (1 - sheetAnim) * (1 - sheetAnim) * (1 - sheetAnim);
  int top = H - (int)(ph * e) - 2;
  shadeRect(fb, 0, 0, W, top, true);
  fillRoundRect(fb, 4, top, W - 8, ph + 12, 14, rgb(15, 17, 27));
  roundRect(fb, 4, top, W - 8, ph + 12, 14, CARD_EDGE);
  fillRoundRect(fb, W / 2 - 16, top + 5, 32, 3, 1, rgb(64, 68, 90));
  text(fb, SMALL, title, 16, top + 25, INK);
  icon(fb, IC_X, W - 22, top + 19, MUTED);
  return top;
}

// sheet panel with a grid of tiles. Returns the index tapped, or -1.
static int tiles(uint16_t *fb, const char *title, Tile *t, int n, int cols) {
  int rows = (n + cols - 1) / cols;
  int ph = 34 + rows * 50 + 4;
  int top = sheetPanel(fb, ph, title);
  if (tapIn(W - 40, top, 40, 32)) { sheet = SH_NONE; return -1; }
  int tw = (W - 8 - 8 - (cols - 1) * 6) / cols, hit2 = -1;
  for (int i = 0; i < n; i++) {
    int x = 12 + (i % cols) * (tw + 6), y = top + 34 + (i / cols) * 50;
    bool pressed = T.down && inBox(T.startX, T.startY, x, y, tw, 44) && inBox(T.x, T.y, x, y, tw, 44);
    uint16_t c = t[i].locked ? GREY : t[i].col;
    fillRoundRect(fb, x, y, tw, 44, 10, pressed ? blend(CARD, c, 110) : (t[i].selected ? blend(CARD, c, 60) : CARD));
    roundRect(fb, x, y, tw, 44, 10, t[i].selected ? c : CARD_EDGE);
    icon(fb, t[i].locked ? (uint8_t)IC_LOCK : t[i].icon, x + 16, y + 22, c);
    text(fb, SMALL, t[i].label, x + 31, y + 20, t[i].locked ? MUTED : INK);
    tiny(fb, t[i].sub, x + 31, y + 27, MUTED);
    if (tapIn(x, y, tw, 44)) hit2 = i;
  }
  // tap above the sheet closes it
  if (T.tap && T.startY < top) sheet = SH_NONE;
  return hit2;
}

static void profileSheet(uint16_t *fb) {
  int top = sheetPanel(fb, 164, "");
  int lv = level();
  char s[64];
  snprintf(s, sizeof(s), "Chindi  -  level %d", lv);
  text(fb, SMALL, s, 16, top + 25, INK);
  if (lv < 10) snprintf(s, sizeof(s), "%lu / %lu XP. Next: %s", (unsigned long)P.xp, (unsigned long)LV[lv], unlockText(lv + 1));
  else snprintf(s, sizeof(s), "%lu XP. Everything unlocked!", (unsigned long)P.xp);
  tiny(fb, s, 16, top + 31, ACCENT);
  const char *lbl[6] = {"STREAK", "TREATS", "PETS", "MEALS", "GAMES", "PHOTOS"};
  uint32_t val[6] = {P.streak, P.treats, P.pets, P.meals, P.games, P.nPhotos};
  for (int i = 0; i < 6; i++) {
    int x = 12 + (i % 3) * 100, y = top + 46 + (i / 3) * 40;
    card(fb, x, y, 96, 35);
    tiny(fb, lbl[i], x + 9, y + 5, MUTED);
    snprintf(s, sizeof(s), "%lu", (unsigned long)val[i]);
    text(fb, SMALL, s, x + 9, y + 29, INK);
  }
  // the album: gifts she brought, and the things you found her doing
  snprintf(s, sizeof(s), "Gifts %d/%d", countBits(P.gifts), room::NGIFT);
  button(fb, 12, top + 130, 146, 28, s, rgb(255, 214, 120));
  char s2[24];
  snprintf(s2, sizeof(s2), "Found %d/%d", countBits(P.found), NFIND);
  button(fb, 162, top + 130, 146, 28, s2, rgb(150, 220, 255));
  if (tapIn(12, top + 126, 148, 36)) { sheet = SH_NONE; mode = MD_ALBUM; albumTab = 0; }
  else if (tapIn(160, top + 126, 150, 36)) { sheet = SH_NONE; mode = MD_ALBUM; albumTab = 1; }
  else if (T.tap && (T.startY < top || inBox(T.startX, T.startY, W - 40, top, 40, 32))) sheet = SH_NONE;
}

static void startGame(uint8_t g);

static void changeRoom(int r) {
  P.room = r;
  saveNow();
  toFloor();
  setAct(IDLE, 3);
  birdX = -100;
  boxX = -1;
  bowlX = -1;
  cupState = CUP_ON;
  cupX = room::TABLE_X0 + 12;
  if (giftItem >= 0) giftX = 160;
}

static void sheets(uint16_t *fb) {
  Tile t[6];
  int lv = level();
  auto set = [&](int i, const char *label, const char *sub, uint8_t ic, uint16_t col, bool locked = false, bool sel = false) {
    t[i].label = label;
    strlcpy(t[i].sub, sub, sizeof(t[i].sub));
    t[i].icon = ic;
    t[i].col = col;
    t[i].locked = locked;
    t[i].selected = sel;
  };
  char sub[24];
  switch (sheet) {
    case SH_FEED: {
      set(0, "Kibble", "+30 food", IC_BOWL, rgb(255, 160, 70));
      set(1, "Fish", "+45 +love", IC_FISH, rgb(90, 180, 255));
      if (P.treats) snprintf(sub, sizeof(sub), "%d left", P.treats);
      else strlcpy(sub, "from wishes", sizeof(sub));
      set(2, "Treat", sub, IC_TREAT, rgb(255, 200, 80), P.treats == 0);
      int h = tiles(fb, "Feed Chindi", t, 3, 3);
      if (h >= 0) { sheet = SH_NONE; feed(h); }
      break;
    }
    case SH_PLAY: {
      for (int k = 0; k < 3; k++) {
        if (lv < TOY_LV[k]) snprintf(sub, sizeof(sub), "level %d", TOY_LV[k]);
        else strlcpy(sub, k == 0 ? "she pounces" : k == 1 ? "swing it" : "throw it", sizeof(sub));
        set(k, TOY_NAMES[k], sub, k == 0 ? IC_LASER : k == 1 ? IC_FEATHER : IC_YARN, rgb(255, 110, 170), lv < TOY_LV[k]);
      }
      set(3, "Games", "3 mini-games", IC_PAD, rgb(120, 220, 140));
      int h = tiles(fb, "Play", t, 4, 2);
      if (h == 3) { sheet = SH_GAMES; sheetAnim = 0.6f; }
      else if (h >= 0 && lv >= TOY_LV[h]) {
        sheet = SH_NONE;
        mode = h == 0 ? MD_LASER : h == 1 ? MD_FEATHER : MD_YARN;
        C.act = PLAY;
        C.stage = 0;
        featherReady = false;
        touchDay();
        toast(h == 0 ? "Move the laser with your stylus" : h == 1 ? "Drag the wand, she swats it" : "Flick the ball", rgb(255, 160, 200));
      } else if (h >= 0) {
        snprintf(sub, sizeof(sub), "Unlocks at level %d", TOY_LV[h]);
        toast(sub, MUTED);
      }
      break;
    }
    case SH_MORE: {
      set(0, "Games", "mini-games", IC_PAD, rgb(120, 220, 140));
      set(1, "Wardrobe", ACC_NAMES[P.acc], IC_HANGER, rgb(240, 120, 200));
      set(2, "Rooms", room::NAMES[P.room], IC_HOUSE, rgb(255, 190, 90));
      set(3, "Photo", "take a picture", IC_CAMERA, rgb(120, 220, 255));
      snprintf(sub, sizeof(sub), "%d gifts, %d found", countBits(P.gifts), countBits(P.found));
      set(4, "Album", sub, IC_GIFT, rgb(255, 214, 120));
      set(5, "Settings", P.kbWalk ? "keyboard walk on" : "keyboard walk off", IC_GEAR, rgb(180, 180, 200));
      int h = tiles(fb, "More", t, 6, 2);
      if (h == 0) { sheet = SH_GAMES; sheetAnim = 0.6f; }
      if (h == 1) { sheet = SH_WARDROBE; sheetAnim = 0.6f; }
      if (h == 2) { sheet = SH_ROOMS; sheetAnim = 0.6f; }
      if (h == 3) { sheet = SH_NONE; mode = MD_PHOTO; }
      if (h == 4) { sheet = SH_NONE; mode = MD_ALBUM; albumTab = 0; }
      if (h == 5) { sheet = SH_SETTINGS; sheetAnim = 0.6f; }
      break;
    }
    case SH_GAMES: {
      snprintf(sub, sizeof(sub), "best %d", P.hiFish);
      set(0, "Fish Catch", sub, IC_FISH, rgb(90, 180, 255));
      snprintf(sub, sizeof(sub), "best %d", P.hiMouse);
      set(1, "Mouse Whack", sub, IC_MOUSE, rgb(200, 200, 210));
      snprintf(sub, sizeof(sub), "best %d.%d s", P.hiLaser / 10, P.hiLaser % 10);
      set(2, "Laser Chase", sub, IC_LASER, rgb(255, 90, 90));
      int h = tiles(fb, "Mini-games", t, 3, 1);
      if (h >= 0) { sheet = SH_NONE; startGame(h); }
      break;
    }
    case SH_WARDROBE: {
      for (int k = 0; k < NACC; k++) {
        if (lv < ACC_LV[k]) snprintf(sub, sizeof(sub), "level %d", ACC_LV[k]);
        else strlcpy(sub, P.acc == k ? "wearing" : "", sizeof(sub));
        set(k, ACC_NAMES[k], sub, k == 0 ? IC_X : IC_HANGER, rgb(240, 120, 200), lv < ACC_LV[k], P.acc == k);
      }
      int h = tiles(fb, "Wardrobe", t, NACC, 2);
      if (h >= 0) {
        if (lv >= ACC_LV[h]) { P.acc = h; saveNow(); if (h) say("mrrp!", IC_HEART); }
        else {
          snprintf(sub, sizeof(sub), "Unlocks at level %d", ACC_LV[h]);
          toast(sub, MUTED);
        }
      }
      break;
    }
    case SH_ROOMS: {
      static const uint8_t RI[room::NROOM] = {IC_HOUSE, IC_BOWL, IC_MOON, IC_STAR};
      for (int k = 0; k < room::NROOM; k++) {
        if (lv < ROOM_LV[k]) snprintf(sub, sizeof(sub), "level %d", ROOM_LV[k]);
        else strlcpy(sub, P.room == k ? "here now" : "", sizeof(sub));
        set(k, room::NAMES[k], sub, RI[k], rgb(255, 190, 90), lv < ROOM_LV[k], P.room == k);
      }
      int h = tiles(fb, "Rooms", t, room::NROOM, 2);
      if (h >= 0) {
        if (lv >= ROOM_LV[h]) {
          sheet = SH_NONE;
          changeRoom(h);
        } else {
          snprintf(sub, sizeof(sub), "Unlocks at level %d", ROOM_LV[h]);
          toast(sub, MUTED);
        }
      }
      break;
    }
    case SH_SETTINGS: {
      set(0, "Keyboard walk", P.kbWalk ? "ON: types on PC" : "OFF", IC_KEYS, rgb(255, 170, 60), false, P.kbWalk);
      set(1, "Walk now", "types in 3 s", IC_KEYS, rgb(255, 170, 60), !P.kbWalk);
      bool armed = (int32_t)(millis() - resetArm) < 3000;
      set(2, armed ? "Sure?" : "Reset", armed ? "tap again" : "start over", IC_X, rgb(255, 90, 90));
      int h = tiles(fb, "Settings", t, 3, 1);
      if (h == 0) { P.kbWalk = !P.kbWalk; saveNow(); }
      if (h == 1 && P.kbWalk) { kbSchedule(millis(), 3000); toast("Click into a text box on your PC!", rgb(255, 170, 60), 3000); sheet = SH_NONE; }
      if (h == 2) {
        if (armed) {
          defaults();
          saveNow();
          resetArm = 0;
          sheet = SH_NONE;
          toast("Fresh start. Hello again!", WHITE);
          changeRoom(room::LIVING);
          giftItem = -1;
          bootWishes = true;
        } else resetArm = millis();
      }
      break;
    }
    case SH_PROFILE: profileSheet(fb); break;
    default: break;
  }
}

static void dock(uint16_t *fb) {
  static const uint8_t IC[5] = {IC_BOWL, IC_YARN, IC_BRUSH, IC_MOON, IC_DOTS};
  static const char *const LB[5] = {"Feed", "Play", "Clean", "Sleep", "More"};
  const int x0 = 34, w = 252, y0 = 203, h = 34;
  shadeRoundRect(fb, x0, y0, w, h, 16);
  roundRect(fb, x0, y0, w, h, 16, rgb(62, 66, 88));
  for (int k = 0; k < 5; k++) {
    int cx = x0 + 26 + k * 50;
    bool pressed = T.down && inBox(T.startX, T.startY, cx - 25, y0, 50, h) && inBox(T.x, T.y, cx - 25, y0, 50, h);
    if (pressed) fillRoundRect(fb, cx - 22, y0 + 3, 44, h - 6, 12, blend(CARD, ACCENT, 110));
    icon(fb, IC[k], cx, y0 + 12, pressed ? WHITE : (k == 0 ? ACCENT : rgb(226, 229, 242)));
    tinyCenter(fb, LB[k], cx, y0 + 23, pressed ? WHITE : MUTED);
    if (tapIn(cx - 25, y0, 50, h)) {
      sheetAnim = 0;
      if (k == 0) sheet = SH_FEED;
      if (k == 1) sheet = SH_PLAY;
      if (k == 2) { mode = MD_BRUSH; brushTime = 0; toast("Brush her with your stylus", rgb(90, 200, 240)); }
      if (k == 3) sleepCmd();
      if (k == 4) sheet = SH_MORE;
    }
  }
}

// bottom bar while a play mode is on
static void modeBar(uint16_t *fb, const char *name) {
  const int x0 = 50, w = 220, y0 = 203, h = 34;
  shadeRoundRect(fb, x0, y0, w, h, 16);
  roundRect(fb, x0, y0, w, h, 16, rgb(62, 66, 88));
  text(fb, SMALL, name, x0 + 16, y0 + 22, INK);
  button(fb, x0 + w - 78, y0 + 4, 70, h - 8, "Done", ACCENT);
  if (tapIn(x0 + w - 80, y0, 80, h)) {
    mode = MD_NORMAL;
    setAct(IDLE, 3);
    touchReg = R_NONE;
  }
}

static bool onUI(float x, float y) {
  if (inHomeCorner(x, y)) return true;
  if (mode == MD_PHOTO) return y > 196;
  if (x >= 30 && y < 40) return true;              // stats card
  if (chipShown && x >= CHIP_X && x < CHIP_X + chipWidth() && y >= CHIP_Y - 2 && y < CHIP_Y + 20) return true;   // wish chip
  if (sheet != SH_NONE) return true;
  if (y >= 203 && x >= 34 && x < 286) return true; // dock or mode bar
  return false;
}

// ---------- Mini-games ----------
#include "chindi_games.h"

// ---------- Gallery ----------
static int galIdx = 0;

static void dateText(int32_t day, char *s, int n) {
  if (day < 0) { strlcpy(s, "", n); return; }
  time_t t = (time_t)day * 86400;
  struct tm tm;
  gmtime_r(&t, &tm);
  static const char *const M[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  snprintf(s, n, "%d %s %d", tm.tm_mday, M[tm.tm_mon], tm.tm_year + 1900);
}

static void galleryFrame(uint16_t *fb) {
  fill(fb, rgb(24, 22, 30));
  if (P.nPhotos == 0) {
    textCenter(fb, MEDIUM, "No photos yet", W / 2, 100, INK);
    textCenter(fb, SMALL, "More > Photo to take one", W / 2, 126, MUTED);
  } else {
    galIdx = constrain(galIdx, 0, P.nPhotos - 1);
    const Photo &p = P.photos[galIdx];
    int rm = p.room < room::NROOM ? p.room : 0;
    room::Env e;
    e.hour = p.hourQ / 10.0f;
    e.timeKnown = true;
    e.wx = p.wx;
    room::draw(fb, rm, e, worldT);
    cg::begin();
    drawProps(rm, e, true, room::TABLE_X0 + 12, 0);
    cg::X.set(0, 0, 1, false);
    cg::render(fb);
    Look L;
    L.x = p.x;
    L.y = p.base;
    L.s = 0.82f;
    L.pose = p.pose;
    L.mouth = p.mouth;
    L.acc = p.acc;
    L.flip = p.flags & 1;
    L.happy = p.flags & 2;
    L.inBox = p.flags & 4;
    L.glasses = p.flags & 8;
    L.eyeOpen = p.eye / 100.0f;
    L.t = 1;
    kitty::draw(fb, L);
    // polaroid frame
    uint16_t paper = rgb(250, 248, 242);
    fillRect(fb, 0, 0, W, 8, paper);
    fillRect(fb, 0, 0, 8, H, paper);
    fillRect(fb, W - 8, 0, 8, H, paper);
    fillRect(fb, 0, 194, W, H - 194, paper);
    char d[24], s[48];
    dateText(p.day, d, sizeof(d));
    snprintf(s, sizeof(s), "Chindi  %s", d);
    text(fb, SMALL, s, 14, 212, rgb(60, 50, 50));
    snprintf(s, sizeof(s), "%d / %d", galIdx + 1, P.nPhotos);
    tiny(fb, s, 14, 219, rgb(140, 130, 130));
  }
  // buttons
  button(fb, 150, 206, 30, 28, "<", rgb(150, 150, 180));
  button(fb, 184, 206, 30, 28, ">", rgb(150, 150, 180));
  button(fb, 218, 206, 46, 28, "Del", rgb(240, 100, 100));
  button(fb, 268, 206, 46, 28, "Close", rgb(150, 150, 180));
  if (tapIn(150, 200, 32, 40) && galIdx > 0) galIdx--;
  if (tapIn(184, 200, 32, 40) && galIdx < P.nPhotos - 1) galIdx++;
  if (tapIn(218, 200, 48, 40) && P.nPhotos > 0) {
    memmove(P.photos + galIdx, P.photos + galIdx + 1, sizeof(Photo) * (P.nPhotos - galIdx - 1));
    P.nPhotos--;
    saveNow();
  }
  if (tapIn(268, 200, 52, 40)) mode = MD_NORMAL;
}

// ---------- Album: the gifts she brought, and what you have found her doing ----------
static void albumFrame(uint16_t *fb) {
  backdrop(fb);
  appHeader(fb, "Album", rgb(255, 214, 120));
  button(fb, 150, 2, 58, 20, "Gifts", rgb(255, 214, 120), albumTab == 0);
  button(fb, 212, 2, 58, 20, "Found", rgb(150, 220, 255), albumTab == 1);
  icon(fb, IC_X, W - 18, 12, MUTED);
  if (tapIn(146, 0, 64, 26)) albumTab = 0;
  if (tapIn(210, 0, 64, 26)) albumTab = 1;
  if (tapIn(W - 40, 0, 40, 30)) mode = MD_NORMAL;
  char s[48];
  if (albumTab == 0) {
    for (int i = 0; i < room::NGIFT; i++) {
      int x = 6 + (i % 4) * 78, y = 30 + (i / 4) * 62;
      bool have = P.gifts & (1u << i);
      card(fb, x, y, 74, 58, have ? blend(CARD_EDGE, rgb(255, 214, 120), 110) : CARD_EDGE);
      if (have) {
        cg::begin();
        room::gift(i, x + 37, y + 24, 1.8f);
        cg::X.set(0, 0, 1, false);
        cg::render(fb);
        tinyCenter(fb, room::GIFT_NAMES[i], x + 37, y + 44, INK);
      } else {
        textCenter(fb, MEDIUM, "?", x + 37, y + 32, rgb(70, 74, 96));
        tinyCenter(fb, "not yet", x + 37, y + 44, rgb(84, 88, 110));
      }
    }
    snprintf(s, sizeof(s), "%d of %d gifts. Finish 3 wishes to get one.", countBits(P.gifts), room::NGIFT);
    tinyCenter(fb, s, W / 2, 222, MUTED);
  } else {
    for (int i = 0; i < NFIND; i++) {
      int x = 8 + (i / 10) * 154, y = 31 + (i % 10) * 18;
      bool have = P.found & (1u << i);
      fillCircle(fb, x + 7, y + 7, 5, have ? rgb(150, 220, 255) : rgb(44, 48, 66));
      if (have) {
        strokeLine(fb, x + 4.5f, y + 7, x + 6.5f, y + 9.5f, 1.6f, BLACK);
        strokeLine(fb, x + 6.5f, y + 9.5f, x + 10, y + 4.5f, 1.6f, BLACK);
      }
      text(fb, SMALL, have ? FIND_NAMES[i] : "? ? ?", x + 18, y + 12, have ? INK : rgb(84, 88, 110));
    }
    snprintf(s, sizeof(s), "%d of %d found. Tap things, and watch her.", countBits(P.found), NFIND);
    tinyCenter(fb, s, W / 2, 222, MUTED);
  }
}

// ---------- The app ----------
static void enter() {
  open = true;
  mode = MD_NORMAL;
  sheet = SH_NONE;
  if (C.act != SLEEPING) {
    toFloor();
    setAct(IDLE, 4);
    say("mrrp!", IC_HEART, 1800);
  }
  if (!P.tutorial) {
    toast("Stroke her. Tap things in her room.", rgb(255, 160, 70), 5000);
    P.tutorial = 1;
    dirty = true;
  }
  for (auto &p : parts) p.life = 0;
}

static void leave() {
  open = false;
  bowlX = -1;
  mode = MD_NORMAL;
  if (C.act != SLEEPING) {
    toFloor();
    setAct(IDLE, 3);
  }
  if (dirty) saveNow();
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  PROF_START();
  worldT += dt;
  updateEnv();
  checkWishes();
  if (mode == MD_GAME) { gameFrame(fb, dt, now); return; }
  if (mode == MD_GALLERY) { galleryFrame(fb); return; }
  if (mode == MD_ALBUM) { albumFrame(fb); return; }

  bool ui = (T.down || T.released) && onUI(T.startX, T.startY);

  // world
  if (cupState == CUP_FALLING) {
    cupVy += 900 * dt;
    cupX += cupVx * dt;
    cupY += cupVy * dt;
    cupRot += 7 * dt;
    if (cupY >= room::CAT_Y - 6) {
      cupState = CUP_BROKEN;
      cupBack = now + 120000;
      burst(PK_SHARD, cupX, room::CAT_Y - 4, 16, 160, 1.4f, rgb(90, 160, 220));
      burst(PK_SHARD, cupX, room::CAT_Y - 4, 8, 140, 1.4f, WHITE);
      say("CRASH!", IC_EXCL, 1500);
      C.puffT = 1;
      discover(F_CUP);
    }
  }
  if (cupState == CUP_BROKEN && (int32_t)(now - cupBack) >= 0) {
    cupState = CUP_ON;
    cupX = room::TABLE_X0 + 12;
  }
  if (boxX >= 0 && (int32_t)(now - boxUntil) >= 0 && !(C.act == BOXSIT)) boxX = -1;
  if (giftItem >= 0 && (int32_t)(now - giftUntil) >= 0) giftItem = -1;
  // the bird: it leaves when its time is up, and on the balcony it flies away
  if (birdX >= 0) {
    if (birdFlying) {
      birdX += 90 * dt;
      birdY -= 60 * dt;
      if (birdY < -12 || birdX > W + 12) { birdX = -100; birdFlying = false; }
    } else if ((int32_t)(now - birdUntil) >= 0) {
      birdLeaves();
    }
  } else if (P.room == room::BALCONY && random(0, 4000) < 3) {
    callBird(now, 15000);                          // one lands on the railing now and then
  }
  flowerKick *= expf(-dt * 2.5f);
  flowerSway = flowerKick * sinf(worldT * 9) + 0.7f * sinf(worldT * 0.9f);

  // per-frame expression defaults; behaviour then overrides them
  Look &L = C.L;
  C.pose = SIT;
  C.eyeT = P.energy < 25 ? 0.55f : 1;
  C.happy = false;
  C.mouth = M_CLOSED;
  C.earT = 0;
  C.puffT = fmaxf(0, C.puffT - dt);
  C.purrT = 0;
  C.tiltT = 0;
  C.swingT = 1;
  C.pupilT = E.night() ? 0.75f : 0.3f;
  L.groom = L.pawL = L.pawR = L.crouch = L.wiggle = L.stretch = L.bat = L.headDown = 0;
  L.inBox = false;

  if (sheet == SH_NONE && !ui && (mode == MD_NORMAL || mode == MD_BRUSH || mode == MD_PHOTO)) interact(dt);
  if (upHigh() && (!highAct() || mode == MD_LASER || mode == MD_FEATHER || mode == MD_YARN)) {
    C.hop = room::CAT_Y - C.base;                              // she is up on something and has
    C.base = room::CAT_Y;                                      // other plans now: she jumps down
    C.hopV = 130;
  }
  if (mode == MD_LASER) laserMode(dt, ui);
  else if (mode == MD_FEATHER) featherMode(dt, ui);
  else if (mode == MD_YARN) yarnMode(dt, ui);
  else {
    C.t += dt;
    behave(dt, now);
  }

  // jumping
  if (C.hop > 0 || C.hopV != 0) {
    C.hopV -= 900 * dt;
    C.hop += C.hopV * dt;
    if (C.hop <= 0) { C.hop = 0; C.hopV = 0; }
  }

  // petting, blinking, ears, where she looks
  if (C.petting > 0) {
    C.petting -= dt;
    C.happy = true;
    C.purrT = fmaxf(C.purrT, 1);
  }
  bool sideP = !frontPose() && C.pose != CURL;
  bool playing = mode == MD_LASER || mode == MD_FEATHER || mode == MD_YARN;
  if (T.down && !ui && !playing && C.act != STARING && C.act != WINDOWWATCH) {
    C.lookXT = constrain((T.x - hit.hx) / 80, -1.0f, 1.0f);
    C.lookYT = constrain((T.y - hit.hy) / 80, -1.0f, 1.0f);
  } else if (!playing) {
    C.lookIn -= dt;
    if (C.lookIn <= 0 && C.act != STARING && C.act != WINDOWWATCH && C.act != ANNOYED && C.act != SNIFF) {
      bool atYou = random(0, 100) < 45;
      C.lookXT = atYou ? 0 : frand(-0.9f, 0.9f);
      C.lookYT = atYou ? 0 : frand(-0.4f, 0.4f);
      C.lookIn = frand(1.5f, 4);
    }
  }
  float k = 1 - expf(-dt / 0.12f);
  float eye = C.eyeT;
  if (C.slowBlink > 0) {
    C.slowBlink -= dt;
    eye = C.slowBlink > 0.5f ? 0.05f : 1;
    C.happy = false;
  }
  L.eyeOpen += (eye - L.eyeOpen) * (C.slowBlink > 0 ? k * 0.5f : k * 1.6f);
  if (!C.happy && C.eyeT > 0.3f && C.slowBlink <= 0) {
    C.blinkIn -= dt;
    if (C.blinkIn <= 0) { C.blinkLeft = 0.15f; C.blinkIn = frand(2, 6); }
  }
  if (C.blinkLeft > 0) { C.blinkLeft -= dt; L.eyeOpen = 0.05f; }
  C.twitchIn -= dt;
  if (C.twitchIn <= 0) {
    if (random(0, 2)) L.twitchL = 1; else L.twitchR = 1;
    C.twitchIn = frand(3, 8);
  }
  L.twitchL *= expf(-dt * 8);
  L.twitchR *= expf(-dt * 8);
  L.pupil += (C.pupilT - L.pupil) * k;
  L.earBack += (C.earT - L.earBack) * k;
  L.tilt += (C.tiltT - L.tilt) * k;
  L.lookX += (C.lookXT - L.lookX) * k;
  L.lookY += (C.lookYT - L.lookY) * k;
  L.purr += (C.purrT - L.purr) * k;
  L.tailPuff += (fminf(C.puffT, 1) - L.tailPuff) * k;
  L.tailSwing += (C.swingT - L.tailSwing) * k * 0.5f;
  L.happy = C.happy;
  L.mouth = C.mouth;
  L.pose = C.pose;
  L.flip = (sideP || C.pose == FLOP) ? C.left : false;
  L.x = C.x;
  L.y = C.base - C.hop;
  L.s = 0.82f;
  L.t = worldT;
  L.phase = C.phase;
  L.breath += dt * (C.pose == CURL ? 1.5f : 2.4f);
  L.acc = P.acc;
  L.glasses = false;
  L.dirty = P.clean < 30;
  if (C.act == BOXSIT && C.stage == 2) L.inBox = true;

  // ---- draw the room ----
  const room::Def &d = R();
  PROF_MARK(P_LOGIC);
  room::draw(fb, P.room, E, worldT);
  PROF_MARK(P_ROOM);
  cg::begin();
  drawProps(P.room, E, cupState == CUP_ON, cupX, flowerSway);
  if (birdX >= 0) room::bird(birdX, birdY, false, worldT, birdFlying);
  if (boxX >= 0 && !(C.act == BOXSIT && C.stage == 2)) room::box(boxX);
  if (pcBusy || (C.act == LAPTOP && C.stage >= 1)) room::laptop(d.laptopX, worldT, pcBusy);
  if (C.act == KBWALK) room::keyboard(160, litKey);
  if (bowlX >= 0) room::bowl(bowlX, bowlFood);
  if (giftItem >= 0) {
    int sh = cg::ell(giftX, room::CAT_Y - 2, 9, 2.5f, 0, BLACK);
    cg::alpha(sh, 40);
    room::gift(giftItem, giftX, room::CAT_Y - 11 - fabsf(sinf(worldT * 3)) * 3, 1.5f);
  }
  if (mode == MD_YARN && trailN > 1) {           // the yarn thread unrolls behind the ball
    cg::X.set(0, 0, 1, false);
    for (int i = 1; i < trailN; i++) {
      int s = cg::cap(trailX[i - 1], trailY[i - 1] + 6, trailX[i], trailY[i] + 6, 0.9f, rgb(230, 80, 120));
      cg::alpha(s, 220 - i * 13);
    }
  }
  cg::X.set(0, 0, 1, false);
  cg::render(fb);
  PROF_MARK(P_PROPS);
  kitty::draw(fb, L);
  PROF_MARK(P_CAT);
  cg::begin();
  if (cupState == CUP_FALLING) room::fallingCup(cupX, cupY, cupRot);
  if (mode == MD_YARN) room::yarn(yX, yY, yRot);
  if (mode == MD_FEATHER) {
    cg::X.set(0, 0, 1, false);
    cg::cap(wandX, wandY, wandX - 30, wandY - 40, 2, rgb(150, 110, 70));
    for (int i = 1; i < NF; i++) cg::cap(fX[i - 1], fY[i - 1], fX[i], fY[i], 0.7f, rgb(240, 240, 240));
    room::feather(fX[NF - 1], fY[NF - 1], atan2f(fX[NF - 2] - fX[NF - 1], fY[NF - 1] - fY[NF - 2]));
  }
  cg::X.set(0, 0, 1, false);
  cg::render(fb);
  if (mode == MD_LASER && laserSeen > 0) {
    for (int r = 9; r >= 2; r--) {
      int a = 40 + (9 - r) * 25;
      for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
          if (i * i + j * j <= r * r) dotA(fb, (int)laserX + i, (int)laserY + j, rgb(255, 40, 40), min(a, 255));
    }
    fillCircle(fb, (int)laserX, (int)laserY, 2, rgb(255, 230, 230));
  }
  if (giftItem >= 0 && fmodf(worldT, 1.2f) < dt)   // a twinkle, so you see the gift
    emit(PK_SPARK, giftX + frand(-8, 8), room::CAT_Y - 14 + frand(-8, 4), 0, -12, 0.7f, rgb(255, 230, 150));
  updateParts(dt);
  drawParts(fb);
  if (lightsOff) dimRoom(fb);
  if (P.room == room::LIVING) room::fountainGlow(fb, lightsOff ? 1.1f : (E.timeKnown && E.night() ? 0.72f : 0));
  PROF_MARK(P_FX);

  // ---- UI ----
  if (mode == MD_PHOTO) {
    uint16_t c = WHITE;
    for (int s2 = 0; s2 < 4; s2++) {             // viewfinder corners
      int cx = s2 % 2 ? W - 14 : 14, cy = s2 / 2 ? 190 : 14, dx = s2 % 2 ? -1 : 1, dy = s2 / 2 ? -1 : 1;
      fillRect(fb, min(cx, cx + dx * 18), cy, 18, 2, c);
      fillRect(fb, cx, min(cy, cy + dy * 18), 2, 18, c);
    }
    fillCircle(fb, W / 2, 220, 16, WHITE);
    fillCircle(fb, W / 2, 220, 13, rgb(30, 30, 40));
    fillCircle(fb, W / 2, 220, 11, WHITE);
    button(fb, 20, 206, 70, 28, "Done", ACCENT);
    button(fb, 230, 206, 70, 28, "Gallery", rgb(160, 170, 255));
    if (tapIn(W / 2 - 22, 198, 44, 42)) savePhoto();
    if (tapIn(16, 200, 80, 40)) mode = MD_NORMAL;
    if (tapIn(226, 200, 80, 40)) mode = MD_GALLERY;
    uint32_t fl = now - flashAt;
    if (flashAt && fl < 250) {
      int a = 255 - fl;
      for (int i = 0; i < W * H; i++) fb[i] = blend(fb[i], WHITE, a);
    }
    messages(fb, now, false);
    return;
  }
  bool sheetWasOpen = sheet != SH_NONE;          // a sheet opened by this frame's tap draws next frame
  statsCard(fb, now);
  PROF_MARK(P_STATS);
  messages(fb, now, mode == MD_NORMAL && !sheetWasOpen);
  PROF_MARK(P_MSG);
  if (mode == MD_NORMAL) {
    if (sheetWasOpen) sheets(fb);
    else if (sheet == SH_NONE) dock(fb);
  } else if (mode == MD_LASER || mode == MD_FEATHER || mode == MD_YARN || mode == MD_BRUSH) {
    modeBar(fb, mode == MD_LASER ? "Laser pointer" : mode == MD_FEATHER ? "Feather wand" : mode == MD_YARN ? "Yarn ball" : "Brushing");
    if (mode == MD_BRUSH && T.down && !ui) icon(fb, IC_BRUSH, (int)T.x + 10, (int)T.y - 10, rgb(90, 200, 240));
  }
  PROF_MARK(P_DOCK);
}

}  // namespace chindi
