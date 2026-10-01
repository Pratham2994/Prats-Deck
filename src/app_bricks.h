// app_bricks.h
// Bricks: Breakout with glowing explosions. Drag anywhere to move the paddle, tap to launch.
// Where the ball hits the paddle sets its angle. Faster every level. High score is saved.
#pragma once

#include "core.h"

namespace bricks {

static const int COLS = 10, ROWS = 6;
static const int BX = 1, BY = 32, BW = 30, BH = 10, BGAP = 2;
static const int PADDLE_Y = 222, PADDLE_H = 7, PADDLE_W = 60;
static const float RADIUS = 3;

static bool alive[ROWS][COLS];
static int left, score, hi = -1, lives, level;
static float paddleX, ballX, ballY, ballVx, ballVy, speed;
static bool stuck, over;

static uint16_t rowColor(int r) { return hsv(r * 50 + level * 40); }

static void newLevel() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) alive[r][c] = true;
  left = ROWS * COLS;
  stuck = true;
  speed = min(200.0f + level * 25, 360.0f);
}

static void newGame() {
  score = 0;
  lives = 3;
  level = 0;
  over = false;
  newLevel();
}

static void enter() {
  if (hi < 0) {
    EEPROM.get(EE_BRICKS_HI, hi);
    if (hi < 0 || hi > 1000000) hi = 0;          // empty flash reads as garbage
  }
  paddleX = W / 2;
  newGame();
}

// knock out the brick under (x, y) if any. Returns true on a hit.
static bool hitBrick(float x, float y) {
  int c = ((int)x - BX) / (BW + BGAP), r = ((int)y - BY) / (BH + BGAP);
  if (x < BX || y < BY || c >= COLS || r >= ROWS || !alive[r][c]) return false;
  alive[r][c] = false;
  left--;
  score += 10 * (ROWS - r) * (level + 1);
  speed = fminf(speed + 4, 420);
  float cx = BX + c * (BW + BGAP) + BW / 2, cy = BY + r * (BH + BGAP) + BH / 2;
  sparkBurst(cx, cy, 30, 260, 0.7f, 110 * 256, 300);
  return true;
}

static void launch() {
  float a = random(-35, 36) * 0.01745f;
  ballVx = sinf(a) * speed;
  ballVy = -cosf(a) * speed;
  stuck = false;
}

static void physics(float dt) {
  int n = (int)(speed * dt / 2) + 1;           // small steps so the ball never skips a brick
  float h = dt / n;
  for (int s = 0; s < n; s++) {
    ballX += ballVx * h;
    ballY += ballVy * h;
    if (ballX < RADIUS) { ballX = RADIUS; ballVx = fabsf(ballVx); }
    if (ballX > W - RADIUS) { ballX = W - RADIUS; ballVx = -fabsf(ballVx); }
    if (ballY < 24 + RADIUS) { ballY = 24 + RADIUS; ballVy = fabsf(ballVy); }
    // bricks: test the ball's left/right edge, then its top/bottom edge
    if (hitBrick(ballX + (ballVx > 0 ? RADIUS : -RADIUS), ballY)) ballVx = -ballVx;
    else if (hitBrick(ballX, ballY + (ballVy > 0 ? RADIUS : -RADIUS))) ballVy = -ballVy;
    // paddle: where it hits sets the angle, up to 60 degrees from straight up
    if (ballVy > 0 && ballY + RADIUS >= PADDLE_Y && ballY - RADIUS <= PADDLE_Y + PADDLE_H &&
        fabsf(ballX - paddleX) <= PADDLE_W / 2 + RADIUS) {
      float rel = constrain((ballX - paddleX) / (PADDLE_W / 2), -1, 1);
      float a = rel * 1.047f;
      ballVx = sinf(a) * speed;
      ballVy = -cosf(a) * speed;
      ballY = PADDLE_Y - RADIUS;
      sparkBurst(ballX, PADDLE_Y, 10, 150, 0.4f, 70 * 256);
    }
    if (ballY > H + RADIUS) {                    // missed
      lives--;
      stuck = true;
      sparkBurst(ballX, H - 4, 60, 300, 0.8f, 140 * 256, -200);
      if (lives <= 0) {
        over = true;
        if (score > hi) {
          hi = score;
          EEPROM.put(EE_BRICKS_HI, hi);
          EEPROM.commit();
        }
      }
      return;
    }
  }
  // keep the speed in step with the level
  float v = sqrtf(ballVx * ballVx + ballVy * ballVy);
  if (v > 0) {
    ballVx *= speed / v;
    ballVy *= speed / v;
  }
}

static void frame(uint16_t *fb, float dt, uint32_t /*now*/) {
  bool ok = !inHomeCorner(T.startX, T.startY);
  if (T.down && ok) paddleX += (constrain(T.x, PADDLE_W / 2, W - PADDLE_W / 2) - paddleX) * (1 - expf(-dt / 0.03f));
  if (T.tap && ok) {
    if (over) newGame();
    else if (stuck) launch();
  }

  if (stuck) {
    ballX = paddleX;
    ballY = PADDLE_Y - RADIUS - 1;
  } else if (!over) {
    physics(dt);
    glowSplat(ballX, ballY, 5, 90 * 256);        // trail
    if (left == 0) {
      level++;
      for (int k = 0; k < 6; k++) sparkBurst(random(40, W - 40), random(40, 140), 50, 350, 1.2f, 140 * 256, 120);
      newLevel();
    }
  }

  sparksUpdate(dt);
  glowRender(fb, pal[PAL_FIRE], dt);

  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (alive[r][c]) {
        int x = BX + c * (BW + BGAP), y = BY + r * (BH + BGAP);
        uint16_t col = rowColor(r);
        fillRect(fb, x, y, BW, BH, dim(col));
        fillRect(fb, x, y, BW, 3, col);
      }
  fillRoundRect(fb, (int)paddleX - PADDLE_W / 2, PADDLE_Y, PADDLE_W, PADDLE_H, 3, rgb(120, 220, 255));
  if (!over) fillCircle(fb, (int)ballX, (int)ballY, (int)RADIUS, WHITE);

  appHeader(fb, "Bricks", rgb(255, 150, 60));
  char s[40];
  snprintf(s, sizeof(s), "%d   Hi %d   Lv %d", score, max(hi, 0), level + 1);
  tiny(fb, s, 100, 8, WHITE);
  for (int i = 0; i < lives; i++) fillCircle(fb, W - 12 - i * 12, 11, 4, rgb(255, 80, 80));

  if (over) {
    card(fb, 40, 100, W - 80, 64, rgb(255, 150, 60));
    textCenter(fb, MEDIUM, "Game over", W / 2, 128, WHITE);
    textCenter(fb, SMALL, score >= hi && score > 0 ? "New high score! Tap to play" : "Tap to play again", W / 2, 152, LIGHTGREY);
  } else if (stuck) {
    textCenter(fb, SMALL, lives == 3 && score == 0 ? "Drag to move, tap to launch" : "Tap to launch", W / 2, 150, LIGHTGREY);
  }
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 4; c++)
      if ((r + c) % 3) fillRect(fb, cx - 17 + c * 9, cy - 14 + r * 6, 8, 5, hsv(r * 60 + 10));
  fillCircle(fb, cx + 4, cy + 6, 3, WHITE);
  fillRect(fb, cx - 10, cy + 13, 20, 3, col);
}

}  // namespace bricks
