// chindi_games.h
// Chindi's mini-games. Included inside namespace chindi by chindi_play.h.
//   Fish Catch  : move Chindi to catch falling fish. Golden fish = 5. Avoid cucumbers
//                 (cats are scared of them). Missing a fish or catching a cucumber costs a life.
//   Mouse Whack : tap the mice before they hide. 30 seconds. Golden mouse = 3.
//   Laser Chase : keep the laser dot away from Chindi. She gets faster.
#pragma once

enum Game : uint8_t { G_FISH, G_MOUSE, G_LASER };
static uint8_t game = G_FISH;
static float gT = 0;
static int gScore = 0, gLives = 3, gReward = 0;
static bool gOver = false, gBest = false;

struct Fall {
  float x, y, vy, rot;
  uint8_t kind;          // 0 fish, 1 golden fish, 2 cucumber
  bool on;
};
static Fall falls[10];
static float gCatX = 160, spawnIn = 0, scare = 0;

static const int HOLES = 6;
static const int HOLE_X[HOLES] = {62, 160, 258, 62, 160, 258};
static const int HOLE_Y[HOLES] = {118, 118, 118, 192, 192, 192};
static float holeAge[HOLES], holeLife[HOLES];
static bool holeOn[HOLES], holeGold[HOLES];
static float pawT = -1, pawX = 0, pawY = 0;

static float gDotX = 260, gDotY = 170, gSpeed = 70;
static Cat gCat;

static void startGame(uint8_t g) {
  mode = MD_GAME;
  game = g;
  gT = 0;
  gScore = 0;
  gLives = 3;
  gOver = false;
  gBest = false;
  gReward = 0;
  for (auto &f : falls) f.on = false;
  spawnIn = 0.6f;
  scare = 0;
  gCatX = 160;
  for (int i = 0; i < HOLES; i++) holeOn[i] = false;
  pawT = -1;
  gDotX = 250;
  gDotY = 170;
  gSpeed = 70;
  gCat = Cat();
  gCat.x = 60;
  for (auto &p : parts) p.life = 0;
  toFloor();
  touchDay();
}

static void endGame() {
  gOver = true;
  P.games++;
  uint16_t *hi = game == G_FISH ? &P.hiFish : game == G_MOUSE ? &P.hiMouse : &P.hiLaser;
  if (gScore > *hi) {
    *hi = gScore;
    gBest = true;
  }
  int pts = game == G_LASER ? gScore / 10 : gScore;           // laser: seconds
  gReward = min(5, game == G_LASER ? pts / 8 : pts / 12);     // treats stay special
  P.treats += gReward;
  bump(P.fun, fminf(30, 8 + pts));
  bump(P.energy, -5);
  addXP(4 + pts / 2);
  grant(WI_GAME);
  saveNow();
}

// ---- drawing pieces ----
static void fishShape(float x, float y, float rot, uint8_t kind) {
  cg::X.set(x, y, 1, false, rot);
  if (kind == 2) {                             // cucumber
    int c = cg::cap(-13, 0, 13, 0, 6, rgb(70, 160, 70));
    cg::out(c, rgb(30, 90, 40), 1.1f);
    for (int k = -1; k <= 1; k++) {
      int s = cg::cap(-9 + k * 7, -2, -5 + k * 7, -2, 0.9f, rgb(140, 210, 120));
      cg::clipTo(s, c);
    }
    return;
  }
  uint16_t col = kind == 1 ? rgb(255, 205, 60) : rgb(110, 170, 230), dark = kind == 1 ? rgb(170, 120, 20) : rgb(50, 90, 150);
  int t = cg::tri(8, 0, 17, -7, 17, 7, col);
  cg::out(t, dark, 1);
  int b = cg::ell(0, 0, 11, 6.5f, 0, col);
  cg::grad(b, kind == 1 ? rgb(255, 240, 160) : rgb(200, 230, 255));
  cg::out(b, dark, 1.1f);
  cg::ell(-6, -1.5f, 1.6f, 1.6f, 0, BLACK);
  cg::cap(-1, -4, -1, 4, 0.6f, dark);
}

static void mouseShape(float x, float y, bool gold) {
  cg::X.set(x, y, 1, false);
  uint16_t body = gold ? rgb(250, 200, 70) : rgb(160, 160, 170), dark = gold ? rgb(160, 110, 20) : rgb(80, 80, 90);
  for (int s = -1; s <= 1; s += 2) {
    int e = cg::ell(s * 9, -14, 6.5f, 6.5f, 0, body);
    cg::out(e, dark, 1);
    cg::ell(s * 9, -14, 4, 4, 0, rgb(250, 170, 180));
  }
  int h = cg::ell(0, -4, 12, 11, 0, body);
  cg::out(h, dark, 1.1f);
  cg::ell(-4.5f, -6, 1.8f, 2.2f, 0, BLACK);
  cg::ell(4.5f, -6, 1.8f, 2.2f, 0, BLACK);
  cg::ell(0, 1, 2.2f, 1.8f, 0, rgb(250, 140, 160));
  for (int s = -1; s <= 1; s += 2)
    for (int k = 0; k < 2; k++) cg::cap(s * 4, 1 + k * 2, s * 16, -1 + k * 4, 0.4f, dark);
}

static void gameBar(uint16_t *fb, const char *title) {
  shadeRect(fb, 0, 0, W, 24, true);
  text(fb, SMALL, title, 32, 17, INK);
  char s[32];
  if (game == G_LASER) snprintf(s, sizeof(s), "%d.%d s", gScore / 10, gScore % 10);
  else snprintf(s, sizeof(s), "%d", gScore);
  textRight(fb, MEDIUM, s, W - 40, 19, ACCENT);
  if (game == G_FISH)
    for (int i = 0; i < 3; i++) icon(fb, IC_HEART, 170 + i * 16, 12, i < gLives ? rgb(255, 80, 110) : rgb(60, 60, 70));
  if (game == G_MOUSE) {
    float left = fmaxf(0, 30 - gT);
    fillRoundRect(fb, 150, 9, 80, 6, 3, rgb(40, 44, 60));
    fillRoundRect(fb, 150, 9, max(2, (int)(80 * left / 30)), 6, 3, left < 5 ? rgb(255, 90, 90) : rgb(120, 220, 140));
  }
  icon(fb, IC_X, W - 16, 12, MUTED);
  if (tapIn(W - 34, 0, 34, 28)) mode = MD_NORMAL;
}

static void gameOverCard(uint16_t *fb) {
  card(fb, 40, 62, 240, 128, ACCENT);
  textCenter(fb, MEDIUM, game == G_MOUSE ? "Time's up!" : "Game over", W / 2, 92, WHITE);
  char s[48];
  if (game == G_LASER) snprintf(s, sizeof(s), "%d.%d s%s", gScore / 10, gScore % 10, gBest ? "  - new best!" : "");
  else snprintf(s, sizeof(s), "Score %d%s", gScore, gBest ? "  - new best!" : "");
  textCenter(fb, SMALL, s, W / 2, 116, gBest ? ACCENT : WHITE);
  snprintf(s, sizeof(s), gReward ? "+%d treats for Chindi" : "Chindi had fun!", gReward);
  tinyCenter(fb, s, W / 2, 126, MUTED);
  button(fb, 56, 146, 100, 32, "Again", ACCENT, true);
  button(fb, 164, 146, 100, 32, "Back", rgb(150, 150, 170));
  if (tapIn(56, 146, 100, 32)) startGame(game);
  if (tapIn(164, 146, 100, 32)) mode = MD_NORMAL;
}

// ---- Fish Catch ----
static void fishGame(uint16_t *fb, float dt) {
  // warm kitchen background
  room::vgrad(fb, 0, 0, W, 160, rgb(255, 238, 214), rgb(250, 214, 178));
  for (int x = 0; x < W; x += 40) fillRect(fb, x, 0, 2, 160, rgb(250, 226, 196));
  room::vgrad(fb, 0, 160, W, 80, rgb(214, 170, 130), rgb(190, 146, 108));
  if (!gOver) {
    gT += dt;
    if (T.down) gCatX += (constrain(T.x, 30.0f, 290.0f) - gCatX) * (1 - expf(-dt / 0.07f));
    spawnIn -= dt;
    if (spawnIn <= 0) {
      spawnIn = fmaxf(0.32f, 1.05f - gScore * 0.02f);
      for (auto &f : falls)
        if (!f.on) {
          int r = random(0, 100);
          f = {frand(20, 300), -12, 70 + gScore * 3.0f + frand(0, 40), frand(-0.5f, 0.5f), (uint8_t)(r < 14 ? 2 : r < 20 ? 1 : 0), true};
          break;
        }
    }
  }
  scare = fmaxf(0, scare - dt);
  // Chindi, looking up with her mouth open
  Look L;
  L.x = gCatX;
  L.y = 236 - (scare > 0 ? sinf(scare * 3.14f) * 30 : 0);
  L.s = 0.6f;
  L.t = worldT;
  L.lookY = -1;
  L.pupil = scare > 0 ? 1 : 0.7f;
  L.earBack = scare > 0 ? 1 : 0;
  L.tailPuff = scare > 0 ? 1 : 0;
  L.acc = P.acc;
  L.mouth = M_CLOSED;
  for (auto &f : falls)
    if (f.on && f.kind != 2 && fabsf(f.x - gCatX) < 40 && f.y > 100) L.mouth = M_MEOW;
  float headX = gCatX, headY = L.y - 104 * L.s;
  for (auto &f : falls) {
    if (!f.on) continue;
    if (!gOver) {
      f.y += f.vy * dt;
      f.rot += dt * 2;
    }
    if (dist(f.x, f.y, headX, headY) < 24) {
      f.on = false;
      if (f.kind == 2) {
        gLives--;
        scare = 1;
        burst(PK_SPARK, f.x, f.y, 10, 120, 0.6f, rgb(120, 220, 120));
      } else {
        gScore += f.kind == 1 ? 5 : 1;
        burst(PK_SPARK, f.x, f.y, f.kind == 1 ? 18 : 8, 120, 0.6f, f.kind == 1 ? rgb(255, 220, 80) : WHITE);
      }
    } else if (f.y > H + 12) {
      f.on = false;
      if (f.kind != 2) gLives--;
    }
  }
  if (!gOver && gLives <= 0) endGame();
  cg::begin();
  for (auto &f : falls)
    if (f.on) fishShape(f.x, f.y, f.rot, f.kind);
  cg::render(fb);
  kitty::draw(fb, L);
  updateParts(dt);
  drawParts(fb);
  gameBar(fb, "Fish Catch");
  if (gOver) gameOverCard(fb);
  else if (gT < 2.5f) textCenter(fb, SMALL, "Drag to move. Avoid cucumbers!", W / 2, 60, rgb(120, 70, 40));
}

// ---- Mouse Whack ----
static void mouseGame(uint16_t *fb, float dt) {
  room::vgrad(fb, 0, 24, W, H - 24, rgb(120, 86, 60), rgb(96, 66, 46));
  for (int y = 40; y < H; y += 26) hline(fb, 0, y, W, rgb(88, 60, 42));
  if (!gOver) {
    gT += dt;
    spawnIn -= dt;
    if (spawnIn <= 0) {
      spawnIn = frand(0.35f, 0.75f) * fmaxf(0.5f, 1 - gT / 60);
      int i = random(0, HOLES);
      if (!holeOn[i]) {
        holeOn[i] = true;
        holeAge[i] = 0;
        holeLife[i] = fmaxf(0.6f, 1.3f - gT * 0.02f);
        holeGold[i] = random(0, 100) < 10;
      }
    }
    if (gT >= 30) endGame();
  }
  cg::begin();
  for (int i = 0; i < HOLES; i++) {
    cg::X.set(HOLE_X[i], HOLE_Y[i], 1, false);
    int rim = cg::ell(0, 0, 34, 12, 0, rgb(70, 48, 34));
    (void)rim;
    cg::ell(0, 1, 28, 9, 0, rgb(20, 14, 12));
  }
  cg::render(fb);
  for (int i = 0; i < HOLES; i++) {
    if (!holeOn[i]) continue;
    if (!gOver) holeAge[i] += dt;
    float u = holeAge[i] / holeLife[i];
    if (u >= 1) { holeOn[i] = false; continue; }
    float up = sinf(fminf(u, 1) * 3.1416f);
    // mouse rising out of the hole, clipped at the hole's edge
    cg::begin();
    mouseShape(HOLE_X[i], HOLE_Y[i] + 6 - up * 22, holeGold[i]);
    int n = cg::nsh;
    cg::X.set(HOLE_X[i], HOLE_Y[i], 1, false);
    int clip = cg::ell(0, -40, 40, 42, 0, BLACK);
    cg::alpha(clip, 0);
    for (int s = 0; s < n; s++) cg::SH[s].clip = clip;
    cg::render(fb);
    if (T.pressed && !gOver && dist(T.x, T.y, HOLE_X[i], HOLE_Y[i] - 10) < 30 && up > 0.3f) {
      holeOn[i] = false;
      gScore += holeGold[i] ? 3 : 1;
      pawT = 0;
      pawX = HOLE_X[i];
      pawY = HOLE_Y[i] - 6;
      burst(PK_SPARK, pawX, pawY, 12, 140, 0.6f, holeGold[i] ? rgb(255, 220, 80) : WHITE);
    }
  }
  // Chindi's paw slams down
  if (pawT >= 0) {
    pawT += dt;
    float d = pawT < 0.12f ? pawT / 0.12f : 1;
    cg::begin();
    cg::X.set(0, 0, 1, false);
    int leg = cg::cap(pawX + 30, -10, pawX, pawY - 10 - (1 - d) * 60, 14, kitty::FUR);
    cg::out(leg, kitty::LINE, 1.3f);
    int pw = cg::ell(pawX, pawY - 4 - (1 - d) * 60, 18, 12, 0, kitty::FUR);
    cg::out(pw, kitty::LINE, 1.3f);
    for (int k = -1; k <= 1; k++) cg::ell(pawX + k * 7, pawY + 2 - (1 - d) * 60, 3.4f, 2.6f, 0, kitty::PINK);
    cg::render(fb);
    if (pawT > 0.4f) pawT = -1;
  }
  updateParts(dt);
  drawParts(fb);
  gameBar(fb, "Mouse Whack");
  if (gOver) gameOverCard(fb);
  else if (gT < 2.5f) textCenter(fb, SMALL, "Tap the mice!", W / 2, 60, WHITE);
}

// ---- Laser Chase ----
static void laserGame(uint16_t *fb, float dt) {
  room::draw(fb, P.room, E, worldT);
  cg::begin();
  drawProps(P.room, E, true, room::TABLE_X0 + 12, 0);
  cg::X.set(0, 0, 1, false);
  cg::render(fb);
  if (!gOver) {
    gT += dt;
    gScore = (int)(gT * 10);
    gSpeed = 60 + gT * 7;
    if (T.down) {
      gDotX = constrain(T.x, 10.0f, 310.0f);
      gDotY = constrain(T.y, 60.0f, 232.0f);
    }
    // Chindi chases along the floor and jumps for dots up high
    float dx = gDotX - gCat.x;
    if (fabsf(dx) > 8) {
      gCat.left = dx < 0;
      gCat.x += (dx > 0 ? 1 : -1) * fminf(gSpeed * dt, fabsf(dx));
      gCat.phase += dt * gSpeed * 0.07f;
    }
    if (fabsf(dx) < 24 && gDotY < 160 && gCat.hop <= 0) gCat.hopV = 330;
    if (gCat.hop > 0 || gCat.hopV != 0) {
      gCat.hopV -= 900 * dt;
      gCat.hop += gCat.hopV * dt;
      if (gCat.hop <= 0) { gCat.hop = 0; gCat.hopV = 0; }
    }
    float catHeadY = room::CAT_Y - gCat.hop - 50;
    if (fabsf(dx) < 14 && fabsf(gDotY - catHeadY) < 26) {
      endGame();
      burst(PK_SPARK, gDotX, gDotY, 16, 140, 0.7f, rgb(255, 80, 80));
    }
  }
  Look L;
  L.x = gCat.x;
  L.y = room::CAT_Y - gCat.hop;
  L.s = 0.82f;
  L.t = worldT;
  L.pose = gOver ? SIT : (gCat.hop > 0 ? POUNCE : RUN);
  L.stretch = gCat.hop > 0 ? 1 : 0;
  L.flip = gCat.left;
  L.phase = gCat.phase;
  L.pupil = 1;
  L.acc = P.acc;
  L.happy = gOver;
  kitty::draw(fb, L);
  for (int r = 8; r >= 2; r--)
    for (int j = -r; j <= r; j++)
      for (int i = -r; i <= r; i++)
        if (i * i + j * j <= r * r) dotA(fb, (int)gDotX + i, (int)gDotY + j, rgb(255, 40, 40), 50 + (8 - r) * 28);
  fillCircle(fb, (int)gDotX, (int)gDotY, 2, rgb(255, 230, 230));
  updateParts(dt);
  drawParts(fb);
  gameBar(fb, "Laser Chase");
  if (gOver) gameOverCard(fb);
  else if (gT < 2.5f) textCenter(fb, SMALL, "Keep the dot away from her!", W / 2, 60, WHITE);
}

static void gameFrame(uint16_t *fb, float dt, uint32_t) {
  switch (game) {
    case G_FISH: fishGame(fb, dt); break;
    case G_MOUSE: mouseGame(fb, dt); break;
    default: laserGame(fb, dt);
  }
}
