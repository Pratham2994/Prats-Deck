// net.h
// Wi-Fi connection (non-blocking), internet time, and small HTTP + JSON helpers.
#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "../config.h"

namespace net {

enum Status { OFF, NO_CONFIG, CONNECTING, CONNECTED, FAILED };
static Status status = OFF;
static uint32_t startedAt = 0;
static bool ntpStarted = false;

static bool configured() { return strlen(WIFI_SSID) > 0; }

// Start connecting if not already. Call every frame from apps that need the internet.
static Status poll() {
  if (!configured()) return status = NO_CONFIG;
  if (status == OFF) {
    WiFi.mode(WIFI_STA);
    WiFi.beginNoBlock(WIFI_SSID, WIFI_PASS);
    startedAt = millis();
    status = CONNECTING;
  }
  if (status == CONNECTING || status == CONNECTED) {
    if (WiFi.status() == WL_CONNECTED) {
      status = CONNECTED;
      if (!ntpStarted) {
        NTP.begin("pool.ntp.org", "time.nist.gov");
        ntpStarted = true;
      }
    } else if (status == CONNECTED) {
      status = CONNECTING;                     // link dropped: the driver reconnects
      startedAt = millis();
    } else if (millis() - startedAt > 20000) {
      status = FAILED;
    }
  }
  return status;
}

static void retry() {
  WiFi.disconnect();
  status = OFF;
}

static bool timeValid() { return time(nullptr) > 1700000000; }

// GET a URL. Tries plain HTTP first, then HTTPS (without certificate checks).
static bool get(const String &url, String &body) {
  {
    HTTPClient http;
    http.setTimeout(5000);
    if (http.begin(url)) {
      int code = http.GET();
      if (code == 200) body = http.getString();
      http.end();
      if (code == 200) return true;
    }
  }
  if (url.startsWith("http://")) {
    HTTPClient http;
    http.setInsecure();
    http.setTimeout(5000);
    if (http.begin("https://" + url.substring(7))) {
      int code = http.GET();
      if (code == 200) body = http.getString();
      http.end();
      if (code == 200) return true;
    }
  }
  return false;
}

// number after "key": searching from position from. NAN if missing.
static float jsonNum(const String &s, const char *key, int from = 0) {
  String k = String("\"") + key + "\":";
  int i = s.indexOf(k, from);
  if (i < 0) return NAN;
  const char *p = s.c_str() + i + k.length();
  char *end;
  float f = strtof(p, &end);
  return end == p ? NAN : f;
}

// first n numbers of array "key":[a,b,c]. Returns how many were read.
static int jsonArr(const String &s, const char *key, float *out, int n, int from = 0) {
  String k = String("\"") + key + "\":[";
  int i = s.indexOf(k, from);
  if (i < 0) return 0;
  const char *p = s.c_str() + i + k.length();
  int got = 0;
  while (got < n) {
    char *end;
    float f = strtof(p, &end);
    if (end == p) break;
    out[got++] = f;
    p = end;
    while (*p == ',' || *p == ' ') p++;
  }
  return got;
}

// string value of "key":"value"
static String jsonStr(const String &s, const char *key) {
  String k = String("\"") + key + "\":\"";
  int i = s.indexOf(k);
  if (i < 0) return "";
  int a = i + k.length(), b = s.indexOf('"', a);
  return b < 0 ? "" : s.substring(a, b);
}

}  // namespace net
