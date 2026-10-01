// app_snake.h
// Snake: the snake moves by itself and grows when it eats. Tap beside its head to turn it:
// above or below the head when it goes sideways, left or right of the head when it goes up
// or down. A wall or its own body ends the game. Gold food is worth 5 and does not stay long.
// The high score is saved.
#pragma once

#include "core.h"

namespace snake {

static const int CELL = 10, GW = 30, GH = 20;   // a field of 30 x 20 cells
static const int OX = 10, OY = 32;              // its top-left corner on the screen
static const int NCELL = GW * GH;
static const uint16_t ACCENT = rgb(120, 230, 110);

struct State {
  uint16_t body[NCELL];      // a ring of cells, from the tail to the head. cell = y * GW + x
  uint8_t used[NCELL];       // 1 where the snake is
  int head, len;             // place of the head in the ring, and the snake's length
  int8_t dx, dy, ndx, ndy;   // direction now, and the turn that waits for the next step
  int food, gold;            // cells. gold = -1: there is none
  float goldLeft;            // seconds until the gold food goes away
  float stepIn;              // seconds until the next step
  int score;
  bool over, started;
};
APP_STATE(State, S)
static int hi = -1;

static int freeCell() {
  for (int tries = 0; tries < 2000; tries++) {
    int c = random(0, NCELL);
    if (!S.used[c] && c != S.food && c != S.gold) return c;
  }
  return -1;
}

static void newGame() {
  memset(&S, 0, sizeof(S));
  S.len = 4;
  for (int i = 0; i < S.len; i++) {
    S.body[i] = (GH / 2) * GW + 5 + i;
    S.used[S.body[i]] = 1;
  }
  S.head = S.len - 1;
  S.dx = S.ndx = 1;
  S.gold = -1;
  S.food = -1;
  S.food = freeCell();
}

static void enter() {
  if (hi < 0) {
    uint16_t v = 0;
    EEPROM.get(EE_SNAKE_HI, v);
    hi = v > NCELL * 5 ? 0 : v;                  // empty flash reads as 65535
  }
  newGame();
}

static void cellXY(int c, int &x, int &y) {
  x = OX + (c % GW) * CELL;
  y = OY + (c / GW) * CELL;
}

static void gameOver() {
  S.over = true;
  int x, y;
  cellXY(S.body[S.head], x, y);
  sparkBurst(x + CELL / 2, y + CELL / 2, 70, 280, 0.9f, 150 * 256);
  if (S.score > hi) {
    hi = S.score;
    EEPROM.put(EE_SNAKE_HI, (uint16_t)hi);
    EEPROM.commit();
  }
}

static void step() {
  if (S.ndx != -S.dx || S.ndy != -S.dy) { S.dx = S.ndx; S.dy = S.ndy; }   // it cannot turn back on itself
  int hx = S.body[S.head] % GW + S.dx, hy = S.body[S.head] / GW + S.dy;
  if (hx < 0 || hx >= GW || hy < 0 || hy >= GH) { gameOver(); return; }
  int n = hy * GW + hx;
  bool eat = n == S.food, eatGold = n == S.gold;
  if (!eat && !eatGold) {                       // no food: the tail moves on
    S.used[S.body[(S.head - S.len + 1 + NCELL) % NCELL]] = 0;
    S.len--;
  }
  if (S.used[n]) { gameOver(); return; }
  S.head = (S.head + 1) % NCELL;
  S.body[S.head] = n;
  S.used[n] = 1;
  S.len++;
  if (eat || eatGold) {
    int x, y;
    cellXY(n, x, y);
    S.score += eatGold ? 5 : 1;
    sparkBurst(x + CELL / 2, y + CELL / 2, eatGold ? 50 : 24, 200, 0.6f, (eatGold ? 150 : 90) * 256);
    if (eatGold) S.gold = -1;
    if (eat) {
      S.food = freeCell();
      if (S.gold < 0 && random(0, 100) < 22) {
        S.gold = freeCell();
        S.goldLeft = 6;
      }
    }
  }
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  bool ok = !inHomeCorner(T.startX, T.startY);
  if (S.over) {
    if (T.tap && ok) newGame();
  } else if (T.pressed && ok) {
    // a tap beside the head turns it that way
    int x, y;
    cellXY(S.body[S.head], x, y);
    if (S.dx != 0) { S.ndx = 0; S.ndy = T.y > y + CELL / 2 ? 1 : -1; }
    else { S.ndy = 0; S.ndx = T.x > x + CELL / 2 ? 1 : -1; }
    S.started = true;
  }

  if (S.started && !S.over) {
    if (S.gold >= 0) {
      S.goldLeft -= dt;
      if (S.goldLeft <= 0) S.gold = -1;
    }
    S.stepIn -= dt;
    if (S.stepIn <= 0) {                        // it gets quicker as it grows
      S.stepIn += fmaxf(0.07f, 0.17f - S.len * 0.0015f);
      if (S.stepIn < 0) S.stepIn = 0;
      step();
    }
  }

  sparksUpdate(dt);
  glowRender(fb, pal[PAL_PHOSPHOR], dt);

  // the field
  rect(fb, OX - 2, OY - 2, GW * CELL + 4, GH * CELL + 4, rgb(50, 90, 60));
  int x, y;
  if (S.food >= 0) {                            // an apple
    cellXY(S.food, x, y);
    fillCircle(fb, x + 5, y + 5, 4, rgb(255, 80, 70));
    fillRect(fb, x + 5, y, 2, 2, rgb(120, 220, 90));
  }
  if (S.gold >= 0 && (S.goldLeft > 2 || ((int)(S.goldLeft * 6) & 1))) {   // it blinks before it goes
    cellXY(S.gold, x, y);
    fillCircle(fb, x + 5, y + 5, 4, rgb(255, 210, 70));
    fillCircle(fb, x + 4, y + 4, 1, WHITE);
  }
  for (int i = 0; i < S.len; i++) {             // from the tail to the head
    cellXY(S.body[(S.head - S.len + 1 + i + NCELL) % NCELL], x, y);
    uint16_t col = blend(rgb(40, 130, 70), ACCENT, 80 + 176 * i / max(S.len - 1, 1));
    fillRoundRect(fb, x + 1, y + 1, CELL - 1, CELL - 1, 3, i == S.len - 1 ? rgb(190, 255, 170) : col);
  }
  cellXY(S.body[S.head], x, y);                 // two eyes, on the side it moves to
  for (int e = -1; e <= 1; e += 2) {
    int ex = x + 5 + S.dx * 2 + (S.dy != 0 ? e * 2 : 0), ey = y + 5 + S.dy * 2 + (S.dx != 0 ? e * 2 : 0);
    fillRect(fb, ex, ey, 2, 2, BLACK);
  }
  if (S.started && !S.over) glowSplat(x + 5, y + 5, 4, 40 * 256);

  appHeader(fb, "Snake", ACCENT);
  char s[32];
  snprintf(s, sizeof(s), "%d   Hi %d", S.score, max(hi, 0));
  tiny(fb, s, W - 8 - tinyWidth(s), 8, WHITE);

  if (S.over) {
    card(fb, 40, 96, W - 80, 64, ACCENT);
    textCenter(fb, MEDIUM, "Game over", W / 2, 124, WHITE);
    textCenter(fb, SMALL, S.score >= hi && S.score > 0 ? "New high score! Tap to play" : "Tap to play again", W / 2, 148, LIGHTGREY);
  } else if (!S.started) {
    textCenter(fb, SMALL, "Tap above or below the head to turn", W / 2, 84, LIGHTGREY);
  }
  (void)now;
}

static void leave() {}

// a curled snake with an apple
static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  fillRoundRect(fb, cx - 15, cy - 11, 24, 6, 3, col);
  fillRoundRect(fb, cx + 3, cy - 11, 6, 15, 3, col);
  fillRoundRect(fb, cx - 15, cy - 2, 24, 6, 3, col);
  fillRoundRect(fb, cx - 15, cy - 2, 6, 15, 3, col);
  fillRoundRect(fb, cx - 15, cy + 7, 20, 6, 3, col);
  fillRoundRect(fb, cx + 1, cy + 6, 9, 8, 3, WHITE);
  fillRect(fb, cx + 6, cy + 8, 2, 2, BLACK);
  fillCircle(fb, cx + 13, cy - 8, 3, rgb(255, 80, 70));
}

}  // namespace snake
