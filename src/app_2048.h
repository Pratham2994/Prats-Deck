// app_2048.h
// 2048: slide the tiles. Two tiles with the same number join into one with their sum.
// Swipe on the board in the direction you want. After each move a new tile comes.
// The game ends when the board is full and nothing can join. The best score is saved.
#pragma once

#include "core.h"

namespace g2048 {

static const uint16_t ACCENT = rgb(240, 180, 70);
static const int N = 4, TILE = 44, GAP = 5;
static const int BX = 8, BY = 31, BSIZE = N * TILE + (N + 1) * GAP;   // the board, 201 px square
static const int PX = BX + BSIZE + 8;                                 // the panel at its right

// The board stays as it is while you use other apps (it is lost at power-off).
static uint8_t cell[N][N];        // 0 = empty, or the power of 2 (1 = "2", 11 = "2048")
static float pop[N][N];           // seconds since this tile came or grew: it swells for a moment
static uint32_t score = 0, best = 0;
static bool begun = false, over = false, won = false;

static bool canMove() {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      if (!cell[r][c]) return true;
      if (c + 1 < N && cell[r][c] == cell[r][c + 1]) return true;
      if (r + 1 < N && cell[r][c] == cell[r + 1][c]) return true;
    }
  return false;
}

static void addTile() {
  int freeAt[N * N], n = 0;
  for (int i = 0; i < N * N; i++)
    if (!cell[i / N][i % N]) freeAt[n++] = i;
  if (!n) return;
  int i = freeAt[random(0, n)];
  cell[i / N][i % N] = random(0, 10) ? 1 : 2;   // mostly a 2, now and then a 4
  pop[i / N][i % N] = 0;
}

static void newGame() {
  memset(cell, 0, sizeof(cell));
  score = 0;
  over = won = false;
  addTile();
  addTile();
}

static void saveBest() {
  if (score <= best) return;
  best = score;
  EEPROM.put(EE_2048_HI, best);
  flashCommit();
}

static void enter() {
  if (!begun) {
    begun = true;
    EEPROM.get(EE_2048_HI, best);
    if (best > 4000000) best = 0;                // empty flash reads as a huge number
    newGame();
  }
}

// Slide all tiles one way: dx, dy is -1, 0 or 1. Returns true if a tile moved.
static bool slide(int dx, int dy) {
  bool moved = false;
  for (int line = 0; line < N; line++) {
    // the cells of this row or column, from the side the tiles go to
    uint8_t v[N];
    int n = 0;
    bool joined = false;                         // the last tile in v came from a join
    for (int k = 0; k < N; k++) {
      int i = dx > 0 || dy > 0 ? N - 1 - k : k;
      uint8_t t = dx ? cell[line][i] : cell[i][line];
      if (!t) continue;
      if (n && v[n - 1] == t && !joined) {
        v[n - 1]++;
        score += 1u << v[n - 1];
        if (v[n - 1] == 11) won = true;
        joined = true;
      } else {
        v[n++] = t;
        joined = false;
      }
    }
    for (int k = 0; k < N; k++) {
      int i = dx > 0 || dy > 0 ? N - 1 - k : k;
      uint8_t t = k < n ? v[k] : 0;
      uint8_t &dst = dx ? cell[line][i] : cell[i][line];
      if (dst != t) {
        moved = true;
        if (t > dst) (dx ? pop[line][i] : pop[i][line]) = 0;
        dst = t;
      }
    }
  }
  return moved;
}

static uint16_t tileColor(int p) {
  static const uint16_t C[] = {rgb(44, 48, 66), rgb(110, 124, 160), rgb(90, 150, 200), rgb(70, 180, 170), rgb(90, 200, 110),
                               rgb(190, 200, 80), rgb(240, 180, 70), rgb(245, 140, 60), rgb(240, 100, 80), rgb(230, 80, 140),
                               rgb(190, 90, 220), rgb(255, 215, 90)};
  return C[min(p, 11)];
}

static void frame(uint16_t *fb, float dt, uint32_t /*now*/) {
  // a swipe: the stylus goes down on the board, moves (more than a tap does), and lifts.
  // The way it went is from the start to the place just before the lift.
  if (T.released && !T.tap && !over && !inHomeCorner(T.startX, T.startY) && inBox(T.startX, T.startY, BX, BY, BSIZE, BSIZE)) {
    float dx = T.relX - T.startX, dy = T.relY - T.startY;
    if (T.moved >= 16 && (fabsf(dx) >= 6 || fabsf(dy) >= 6)) {
      bool h = fabsf(dx) > fabsf(dy);
      if (slide(h ? (dx > 0 ? 1 : -1) : 0, h ? 0 : (dy > 0 ? 1 : -1))) {
        addTile();
        if (!canMove()) {
          over = true;
          saveBest();
        }
      }
    }
  }

  backdrop(fb);
  appHeader(fb, "2048", ACCENT);

  fillRoundRect(fb, BX, BY, BSIZE, BSIZE, 9, rgb(24, 27, 40));
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      int p = cell[r][c];
      pop[r][c] += dt;
      int grow = p && pop[r][c] < 0.12f ? (int)(3 * sinf(pop[r][c] / 0.12f * 3.1416f)) : 0;
      int x = BX + GAP + c * (TILE + GAP) - grow, y = BY + GAP + r * (TILE + GAP) - grow, s = TILE + 2 * grow;
      fillRoundRect(fb, x, y, s, s, 7, tileColor(p));
      if (!p) continue;
      char t[12];
      snprintf(t, sizeof(t), "%lu", 1ul << p);
      const Font *f = p < 7 ? LARGE : p < 10 ? MEDIUM : SMALL;      // 3 and 4 digits need a smaller font
      textCenter(fb, f, t, x + s / 2, y + s / 2 + (f == LARGE ? 8 : f == MEDIUM ? 6 : 4), p < 3 ? WHITE : rgb(20, 22, 30));
    }

  // the panel: score, best, New
  const int pw = W - PX - 8;
  card(fb, PX, BY, pw, 46);
  tiny(fb, "SCORE", PX + 9, BY + 6, MUTED);
  char s[16];
  snprintf(s, sizeof(s), "%lu", (unsigned long)score);
  text(fb, MEDIUM, s, PX + 9, BY + 37, INK);
  card(fb, PX, BY + 52, pw, 46);
  tiny(fb, "BEST", PX + 9, BY + 58, MUTED);
  snprintf(s, sizeof(s), "%lu", (unsigned long)max(best, score));
  text(fb, MEDIUM, s, PX + 9, BY + 89, ACCENT);
  button(fb, PX, BY + 104, pw, 34, "New", ACCENT);
  if (tapIn(PX, BY + 104, pw, 34)) {
    saveBest();
    newGame();
  }
  tiny(fb, won ? "You made 2048!" : "Swipe on the", PX + 2, BY + 150, won ? ACCENT : MUTED);
  tiny(fb, won ? "Go on for more." : "board to slide", PX + 2, BY + 162, MUTED);
  if (!won) tiny(fb, "the tiles.", PX + 2, BY + 174, MUTED);

  if (over) {
    card(fb, BX + 20, BY + 66, BSIZE - 40, 68, ACCENT);
    textCenter(fb, MEDIUM, "No more moves", BX + BSIZE / 2, BY + 94, WHITE);
    textCenter(fb, SMALL, "Tap New to play again", BX + BSIZE / 2, BY + 118, LIGHTGREY);
  }
}

static void leave() { saveBest(); }

// four small tiles
static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  for (int i = 0; i < 4; i++) {
    int x = cx - 15 + (i % 2) * 16, y = cy - 15 + (i / 2) * 16;
    fillRoundRect(fb, x, y, 14, 14, 4, i == 0 ? WHITE : i == 3 ? col : blend(rgb(40, 44, 60), col, i == 1 ? 110 : 190));
  }
  fillRect(fb, cx - 10, cy - 9, 4, 2, rgb(40, 44, 60));          // a "2" hint on the white tile
  fillRect(fb, cx - 10, cy - 6, 4, 2, rgb(40, 44, 60));
}

}  // namespace g2048
