// app_clock.h
// Clock & Weather over Wi-Fi: big glowing clock, current weather, 3-day forecast,
// and a live background that matches the weather (sun, stars, clouds, rain, snow, storm).
// Time from the internet (NTP). Weather from open-meteo.com (free, no key).
// Location from your internet address (ip-api.com) unless set in config.h.
// Tap the weather panel to refresh it.
#pragma once

#include "core.h"
#include "net.h"

namespace clockapp {

enum Kind { K_CLEAR, K_PARTLY, K_CLOUDY, K_FOG, K_DRIZZLE, K_RAIN, K_SNOW, K_STORM };
static const char *const KIND_NAMES[] = {"Clear", "Partly cloudy", "Cloudy", "Fog", "Drizzle", "Rain", "Snow", "Thunderstorm"};

static Kind kindOf(int c) {
  if (c == 0) return K_CLEAR;
  if (c <= 2) return K_PARTLY;
  if (c == 3) return K_CLOUDY;
  if (c == 45 || c == 48) return K_FOG;
  if (c >= 51 && c <= 57) return K_DRIZZLE;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return K_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return K_SNOW;
  if (c >= 95) return K_STORM;
  return K_CLOUDY;
}

// kept between visits to the app
static bool haveLoc = false, haveWeather = false;
static float lat, lon;
static char city[32] = "";
static long tzOffset = 0;                    // seconds from UTC
static bool tzKnown = false;
static float temp, humidity, wind;
static int code = 0;
static bool isDay = true;
static float dmax[3], dmin[3], dcode[3];
static uint32_t nextFetch = 0;
static bool fetchPending = false;            // draw "Updating" first, fetch next frame
static char err[40] = "";

static float spawnAcc = 0;
static float cloudX[5], cloudY[5];
static uint32_t flashAt = 0;

static bool fetchLocation() {
  if (WEATHER_LAT != 0.0 || WEATHER_LON != 0.0) {
    lat = WEATHER_LAT;
    lon = WEATHER_LON;
    strlcpy(city, WEATHER_CITY, sizeof(city));
    return haveLoc = true;
  }
  String body;
  if (!net::get("http://ip-api.com/json/?fields=status,city,lat,lon,offset", body)) return false;
  float a = net::jsonNum(body, "lat"), b = net::jsonNum(body, "lon");
  if (isnan(a) || isnan(b)) return false;
  lat = a;
  lon = b;
  float off = net::jsonNum(body, "offset");
  if (!isnan(off)) {
    tzOffset = (long)off;
    tzKnown = true;
  }
  strlcpy(city, net::jsonStr(body, "city").c_str(), sizeof(city));
  return haveLoc = true;
}

static bool fetchWeather() {
  char url[320];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=3%s",
           lat, lon, USE_FAHRENHEIT ? "&temperature_unit=fahrenheit&wind_speed_unit=mph" : "");
  String body;
  if (!net::get(url, body)) return false;
  int cur = body.indexOf("\"current\":");
  int day = body.indexOf("\"daily\":");
  if (cur < 0 || day < 0) return false;
  float t = net::jsonNum(body, "temperature_2m", cur);
  if (isnan(t)) return false;
  temp = t;
  humidity = net::jsonNum(body, "relative_humidity_2m", cur);
  wind = net::jsonNum(body, "wind_speed_10m", cur);
  code = (int)net::jsonNum(body, "weather_code", cur);
  isDay = net::jsonNum(body, "is_day", cur) != 0;
  float off = net::jsonNum(body, "utc_offset_seconds");
  if (!isnan(off)) {
    tzOffset = (long)off;
    tzKnown = true;
  }
  net::jsonArr(body, "temperature_2m_max", dmax, 3, day);
  net::jsonArr(body, "temperature_2m_min", dmin, 3, day);
  net::jsonArr(body, "weather_code", dcode, 3, day);
  return haveWeather = true;
}

// ---------- Weather icons ----------
static void cloud(uint16_t *fb, int cx, int cy, int s, uint16_t col) {
  fillCircle(fb, cx - s / 2, cy, s / 2, col);
  fillCircle(fb, cx + s * 2 / 5, cy, s * 9 / 20, col);
  fillCircle(fb, cx, cy - s / 3, s * 11 / 20, col);
  fillRect(fb, cx - s / 2, cy, s * 9 / 10, s / 2, col);
}

static void sun(uint16_t *fb, int cx, int cy, int s) {
  uint16_t y = rgb(255, 200, 40);
  for (int k = 0; k < 8; k++) {
    float a = k * 0.785f;
    line(fb, cx + (int)(cosf(a) * s * 0.7f), cy + (int)(sinf(a) * s * 0.7f),
         cx + (int)(cosf(a) * s), cy + (int)(sinf(a) * s), y);
  }
  fillCircle(fb, cx, cy, s / 2, y);
}

static void weatherIcon(uint16_t *fb, int c, bool day, int cx, int cy, int s) {
  Kind k = kindOf(c);
  uint16_t cl = rgb(200, 205, 220), dark = rgb(120, 125, 140);
  switch (k) {
    case K_CLEAR:
      if (day) sun(fb, cx, cy, s);
      else {
        fillCircle(fb, cx, cy, s * 2 / 3, rgb(230, 230, 200));
        fillCircle(fb, cx + s / 3, cy - s / 4, s * 2 / 3, rgb(10, 10, 16));
      }
      break;
    case K_PARTLY:
      if (day) sun(fb, cx + s / 3, cy - s / 3, s * 2 / 3);
      cloud(fb, cx - s / 6, cy + s / 5, s, cl);
      break;
    case K_FOG:
      for (int i = 0; i < 4; i++) fillRect(fb, cx - s + (i % 2) * 4, cy - s / 2 + i * s / 3, s * 2 - 4, 3, cl);
      break;
    case K_DRIZZLE:
    case K_RAIN:
      cloud(fb, cx, cy - s / 4, s, dark);
      for (int i = -1; i <= 1; i++)
        thickLine(fb, cx + i * s / 2, cy + s / 3, cx + i * s / 2 - 4, cy + s / 3 + (k == K_RAIN ? 12 : 6), rgb(90, 160, 255));
      break;
    case K_SNOW:
      cloud(fb, cx, cy - s / 4, s, cl);
      for (int i = -1; i <= 1; i++) fillCircle(fb, cx + i * s / 2, cy + s / 2 + (i & 1) * 4, 2, WHITE);
      break;
    case K_STORM:
      cloud(fb, cx, cy - s / 4, s, dark);
      fillTriangle(fb, cx + 2, cy + s / 4, cx - 6, cy + s * 3 / 4, cx, cy + s * 3 / 4, rgb(255, 220, 40));
      fillTriangle(fb, cx, cy + s * 3 / 4 - 2, cx + 6, cy + s * 3 / 4 - 2, cx - 4, cy + s + 4, rgb(255, 220, 40));
      break;
    default:
      cloud(fb, cx, cy, s, cl);
  }
}

// ---------- Live background ----------
static int background(float dt, uint32_t now) {
  if (!haveWeather) {
    if (random(0, 100) < 20) glowSplat(random(0, W), random(0, H), 3, 120 * 256);
    return PAL_ICE;
  }
  Kind k = kindOf(code);
  float rate = 0;                              // sparks per second
  if (k == K_RAIN || k == K_STORM) rate = 160;
  else if (k == K_DRIZZLE) rate = 60;
  else if (k == K_SNOW) rate = 45;
  else if (k == K_CLEAR && isDay) rate = 12;
  spawnAcc += rate * dt;
  while (spawnAcc >= 1) {
    spawnAcc -= 1;
    float x = random(-40, W + 40);
    if (k == K_SNOW) spark(x, -4, random(-15, 15), random(25, 50), 9, 40 * 256);
    else if (k == K_CLEAR) spark(random(0, W), H + 4, random(-10, 10), -random(15, 45), 6, 30 * 256);
    else spark(x, -4, -60, random(280, 380), 1.2f, (k == K_DRIZZLE ? 30 : 45) * 256);
  }
  if (k == K_CLEAR && !isDay && random(0, 100) < 25)       // twinkling stars
    glowSplat(random(0, W), random(0, 140), 2, random(60, 160) * 256);
  // Steady glows: light added per second. With the default fade (0.84 x level + 13 steps lost
  // per second) a rate of R settles at level (R - 13) / 0.84.
  if (k == K_CLEAR && isDay) glowSplat(306, 8, 44, (int)(110 * 256 * dt));   // sun, settles near 115
  if (k == K_PARTLY || k == K_CLOUDY || k == K_FOG || k == K_STORM || k == K_RAIN || k == K_DRIZZLE) {
    int n = k == K_PARTLY ? 3 : 5;
    for (int i = 0; i < n; i++) {
      cloudX[i] += (8 + i * 3) * dt;
      if (cloudX[i] > W + 60) {
        cloudX[i] = -60;
        cloudY[i] = random(10, k == K_FOG ? 230 : 120);
      }
      glowSplat(cloudX[i], cloudY[i], k == K_FOG ? 60 : 36, (int)(55 * 256 * dt));   // settles near 50
    }
  }
  if (k == K_STORM && (int32_t)(now - flashAt) > 0) {      // lightning
    for (int i = 0; i < 6; i++) glowSplat(random(0, W), random(0, 120), 70, 200 * 256);
    flashAt = now + random(2500, 7000);
  }
  sparksUpdate(dt);
  if (k == K_CLEAR) return isDay ? PAL_GOLD : PAL_ICE;
  if (k == K_SNOW || k == K_CLOUDY || k == K_FOG || k == K_PARTLY) return PAL_SNOW;
  return PAL_RAIN;
}

static void degree(uint16_t *fb, int x, int y, int r, uint16_t col) {
  circle(fb, x, y, r, col);
  circle(fb, x, y, r - 1, col);
}

static void enter() {
  for (int i = 0; i < 5; i++) {
    cloudX[i] = random(-60, W);
    cloudY[i] = random(10, 120);
  }
}

static void frame(uint16_t *fb, float dt, uint32_t now) {
  net::Status st = net::poll();

  if (fetchPending) {                          // the "Updating" frame is on screen now
    fetchPending = false;
    bool ok = (haveLoc || fetchLocation()) && fetchWeather();
    strlcpy(err, ok ? "" : "Weather update failed", sizeof(err));
    nextFetch = now + (ok ? 15 * 60000UL : 60000UL);
  } else if (st == net::CONNECTED && (int32_t)(now - nextFetch) >= 0) {
    fetchPending = true;
  }

  int p = background(dt, now);
  glowRender(fb, pal[p], dt);

  // ---- time ----
  char hm[8] = "--:--";
  char date[48] = "";
  int sec = 0;
  bool pm = false;
  if (net::timeValid()) {
    time_t t = time(nullptr) + tzOffset;
    struct tm tm;
    gmtime_r(&t, &tm);
    int h = tm.tm_hour;
    pm = h >= 12;
    if (!CLOCK_24H) h = h % 12 == 0 ? 12 : h % 12;
    snprintf(hm, sizeof(hm), "%02d:%02d", h, tm.tm_min);
    sec = tm.tm_sec;
    static const char *const DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *const MONTHS[] = {"January", "February", "March", "April", "May", "June", "July",
                                         "August", "September", "October", "November", "December"};
    snprintf(date, sizeof(date), "%s, %d %s", DAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon]);
  }
  const int dw = 46, dh = 84, dtk = 10;
  int tw = seg7Width(hm, dw, dtk);
  int tx = (W - tw) / 2;
  uint16_t tc = rgb(200, 240, 255);
  seg7Text(fb, hm, tx, 22, dw, dh, dtk, tc);
  if (!CLOCK_24H && net::timeValid()) tiny(fb, pm ? "PM" : "AM", tx + tw + 2, 24, tc);
  fillRect(fb, tx, 112, tw, 2, rgb(25, 30, 40));
  fillRect(fb, tx, 112, tw * sec / 59, 2, rgb(80, 180, 255));
  if (date[0]) textCenter(fb, SMALL, date, W / 2, 134, WHITE);
  if (city[0]) tiny(fb, city, W - 6 - tinyWidth(city), 6, LIGHTGREY);

  // ---- weather panel ----
  const int py = 144;
  card(fb, 4, py, W - 8, H - py - 3);
  const char *status = nullptr;
  if (st == net::NO_CONFIG) status = "Add your Wi-Fi in config.h";
  else if (st == net::FAILED) status = "Wi-Fi failed. Tap to retry";
  else if (st == net::CONNECTING && !haveWeather) status = "Connecting to Wi-Fi...";
  else if (!haveWeather) status = fetchPending ? "Getting weather..." : (err[0] ? err : "Getting weather...");

  if (status) {
    textCenter(fb, SMALL, status, W / 2, py + 52, WHITE);
    if (st == net::NO_CONFIG) tinyCenter(fb, "(the clock needs internet time too)", W / 2, py + 64, LIGHTGREY);
  } else {
    weatherIcon(fb, code, isDay, 36, py + 34, 22);
    char s[40];
    snprintf(s, sizeof(s), "%.0f", temp);
    int x = text(fb, LARGE, s, 70, py + 46, WHITE);
    degree(fb, x + 5, py + 24, 4, WHITE);
    text(fb, MEDIUM, USE_FAHRENHEIT ? "F" : "C", x + 12, py + 46, WHITE);
    text(fb, SMALL, KIND_NAMES[kindOf(code)], 12, py + 74, rgb(180, 210, 255));
    snprintf(s, sizeof(s), "Humidity %.0f%%  Wind %.0f %s", humidity, wind, USE_FAHRENHEIT ? "mph" : "km/h");
    tiny(fb, s, 12, py + 82, LIGHTGREY);

    // 3-day forecast
    static const char *const SHORT[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int wday = 0;
    if (net::timeValid()) {
      time_t t = time(nullptr) + tzOffset;
      struct tm tm;
      gmtime_r(&t, &tm);
      wday = tm.tm_wday;
    }
    vline(fb, 168, py + 8, H - py - 16, rgb(40, 50, 70));
    for (int d = 0; d < 3; d++) {
      int cx = 196 + d * 50;
      tinyCenter(fb, d == 0 ? "Today" : SHORT[(wday + d) % 7], cx, py + 8, WHITE);
      weatherIcon(fb, (int)dcode[d], true, cx, py + 38, 13);
      snprintf(s, sizeof(s), "%.0f", dmax[d]);
      textCenter(fb, SMALL, s, cx, py + 72, WHITE);
      snprintf(s, sizeof(s), "%.0f", dmin[d]);
      tinyCenter(fb, s, cx, py + 78, LIGHTGREY);
    }
    if (fetchPending) tiny(fb, "Updating...", 6, py + 4, LIGHTGREY);
    else if (err[0]) tiny(fb, err, 6, py + 4, rgb(255, 120, 120));
  }

  if (tapIn(0, py, W, H - py)) {
    if (st == net::FAILED) net::retry();
    else if (st == net::CONNECTED && !fetchPending) fetchPending = true;
  }
}

static void leave() {}

static void icon(uint16_t *fb, int cx, int cy, uint16_t col) {
  sun(fb, cx + 7, cy - 7, 13);
  cloud(fb, cx - 3, cy + 5, 20, WHITE);
  (void)col;
}

}  // namespace clockapp
