// Host simulator stand-ins for the Arduino / pico-sdk APIs the sketch uses.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>
#include <vector>
#include <initializer_list>

using std::min;
using std::max;
using std::isnan;
typedef unsigned int uint;
#define PROGMEM
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define OUTPUT 1
#define LOW 0
#define HIGH 1

#ifndef gmtime_r
static inline struct tm *gmtime_r(const time_t *t, struct tm *out) { gmtime_s(out, t); return out; }
#endif
static inline size_t strlcpy(char *d, const char *s, size_t n) {
  size_t l = strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; }
  return l;
}
static inline size_t strlcat(char *d, const char *s, size_t n) {
  size_t l = strlen(d);
  if (l + 1 < n) strlcpy(d + l, s, n - l);
  return l + strlen(s);
}

// ---- time ----
extern uint64_t simUs;
static inline unsigned long millis() { return (unsigned long)(simUs / 1000); }
static inline unsigned long micros() { return (unsigned long)simUs; }
static inline void delay(unsigned long ms) { simUs += ms * 1000; }
static inline void delayMicroseconds(unsigned long us) { simUs += us; }
static inline long random(long a, long b) { return b <= a ? a : a + rand() % (b - a); }
static inline long random(long b) { return random(0, b); }
static inline void randomSeed(unsigned long s) { srand((unsigned)s); }
static inline void pinMode(int, int) {}
static inline void digitalWrite(int, int) {}
static inline void analogWrite(int, int) {}
static inline float analogReadTemp(float = 3.3f) { return 27.4f; }
struct RP2040 { uint32_t hwrand32() { return 1234; } int getFreeHeap() { return 75000; } };
extern RP2040 rp2040;

// ---- String ----
class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c) {}
  String(const std::string &c) : s(c) {}
  int indexOf(const String &k, int from = 0) const { auto p = s.find(k.s, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(char c, int from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  unsigned length() const { return s.size(); }
  const char *c_str() const { return s.c_str(); }
  String substring(int a, int b = -1) const { return b < 0 ? String(s.substr(a)) : String(s.substr(a, b - a)); }
  bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
  friend String operator+(const String &a, const String &b) { return String(a.s + b.s); }
  friend String operator+(const String &a, const char *b) { return String(a.s + b); }
  friend String operator+(const char *a, const String &b) { return String(a + b.s); }
};

// ---- Serial ----
struct SerialSim {
  std::string in;
  void begin(int) {}
  int available() { return (int)in.size(); }
  int read() { if (in.empty()) return -1; int c = (uint8_t)in[0]; in.erase(0, 1); return c; }
  template <class... A> void printf(const char *f, A... a) { ::printf(f, a...); }
};
extern SerialSim Serial;

// ---- EEPROM ----
struct EEPROMSim {
  uint8_t mem[1024];
  EEPROMSim() { memset(mem, 0xFF, sizeof(mem)); }
  void begin(int) {}
  template <class T> void get(int a, T &v) { memcpy(&v, mem + a, sizeof(T)); }
  template <class T> void put(int a, const T &v) { memcpy(mem + a, &v, sizeof(T)); }
  uint8_t read(int a) { return mem[a]; }
  void write(int a, uint8_t v) { mem[a] = v; }
  bool commit() { return true; }
};
extern EEPROMSim EEPROM;

// ---- Keyboard ----
#define KEY_LEFT_CTRL 0x80
#define KEY_LEFT_SHIFT 0x81
#define KEY_LEFT_GUI 0x83
#define KEY_ESC 0xB1
struct KeyboardSim {
  int sent = 0;
  void begin() {}
  void press(uint8_t) {}
  void releaseAll() { sent++; }
  std::string typed;
  size_t write(uint8_t c) { typed += (char)c; return 1; }
  void consumerPress(uint16_t) {}
  void consumerRelease() { sent++; }
};
extern KeyboardSim Keyboard;

// ---- Wi-Fi ----
enum { WIFI_STA };
enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3 };
enum { ENC_TYPE_NONE = 7 };
struct WiFiSim {
  uint64_t connectAt = 0;
  bool scanning = false;
  uint64_t scanDone = 0;
  void mode(int) {}
  int beginNoBlock(const char *, const char *) { connectAt = simUs + 1500000; return 0; }
  int status() { return connectAt && simUs >= connectAt ? WL_CONNECTED : WL_IDLE_STATUS; }
  void disconnect() { connectAt = 0; }
  int scanNetworks(bool) { scanning = true; scanDone = simUs + 2000000; return -1; }
  int scanComplete() { return simUs >= scanDone ? 9 : -1; }
  const char *SSID(int i) { static const char *n[] = {"HomeNet", "Neighbour_5G", "", "CafeFree", "TP-Link_2F1A", "AndroidAP", "Office-Guest", "VeryLongNetworkNameThatGoesOnAndOn", "Jio-Fiber"}; return n[i]; }
  int RSSI(int i) { static int r[] = {-42, -61, -70, -80, -55, -66, -88, -74, -50}; return r[i]; }
  int channel(int i) { static int c[] = {6, 1, 11, 6, 3, 9, 1, 11, 6}; return c[i]; }
  int encryptionType(int i) { return i == 3 ? ENC_TYPE_NONE : 4; }
};
extern WiFiSim WiFi;
struct NTPSim { void begin(const char *, const char *) {} };
extern NTPSim NTP;

struct HTTPClient {
  std::string url;
  void setTimeout(int) {}
  void setInsecure() {}
  bool begin(const String &u) { url = u.s; return true; }
  int GET() { return 200; }
  String getString() {
    if (url.find("ip-api") != std::string::npos)
      return String("{\"status\":\"success\",\"city\":\"Pune\",\"lat\":18.52,\"lon\":73.86,\"offset\":19800}");
    return String(
      "{\"latitude\":18.5,\"longitude\":73.875,\"generationtime_ms\":0.05,\"utc_offset_seconds\":19800,"
      "\"timezone\":\"Asia/Kolkata\",\"current_units\":{\"time\":\"iso8601\",\"interval\":\"seconds\","
      "\"temperature_2m\":\"°C\",\"relative_humidity_2m\":\"%\",\"weather_code\":\"wmo code\",\"wind_speed_10m\":\"km/h\",\"is_day\":\"\"},"
      "\"current\":{\"time\":\"2026-10-01T16:15\",\"interval\":900,\"temperature_2m\":27.8,\"relative_humidity_2m\":74,"
      "\"weather_code\":" + std::string(getenv("WCODE") ? getenv("WCODE") : "63") + ",\"wind_speed_10m\":11.2,\"is_day\":1},"
      "\"daily_units\":{\"time\":\"iso8601\",\"weather_code\":\"wmo code\",\"temperature_2m_max\":\"°C\",\"temperature_2m_min\":\"°C\"},"
      "\"daily\":{\"time\":[\"2026-10-01\",\"2026-10-02\",\"2026-10-03\"],\"weather_code\":[63,2,95],"
      "\"temperature_2m_max\":[29.1,30.4,28.0],\"temperature_2m_min\":[22.3,21.9,22.8]}}");
  }
  void end() {}
};

// ---- pico-sdk hardware ----
typedef struct { volatile uint32_t dr, icr; } spi_hw_t;
typedef struct spi_inst spi_inst_t;
extern spi_hw_t spiHw;
#define spi1 ((spi_inst_t *)&spiHw)
static inline spi_hw_t *spi_get_hw(spi_inst_t *) { return &spiHw; }
#define SPI_SSPICR_RORIC_BITS 1
enum { SPI_CPOL_0 }; enum { SPI_CPHA_0 }; enum { SPI_MSB_FIRST };
static inline uint spi_init(spi_inst_t *, uint b) { return b; }
static inline void spi_set_format(spi_inst_t *, int, int, int, int) {}
static inline uint spi_set_baudrate(spi_inst_t *, uint b) { return b; }
static inline uint spi_get_baudrate(spi_inst_t *) { return 62500000; }
void spi_write_blocking(spi_inst_t *, const uint8_t *d, size_t n);
void spi_read_blocking(spi_inst_t *, uint8_t rep, uint8_t *d, size_t n);
static inline bool spi_is_busy(spi_inst_t *) { return false; }
static inline bool spi_is_readable(spi_inst_t *) { return false; }
static inline uint spi_get_dreq(spi_inst_t *, bool) { return 0; }

enum { GPIO_OUT = 1, GPIO_IN = 0 };
enum { GPIO_FUNC_SPI = 1, GPIO_FUNC_PWM = 4, GPIO_FUNC_SIO = 5 };
enum { GPIO_SLEW_RATE_FAST = 1 };
enum { GPIO_DRIVE_STRENGTH_8MA = 2 };
void gpio_put(uint p, bool v);
bool gpio_get(uint p);
static inline void gpio_init(uint) {}
static inline void gpio_deinit(uint) {}
static inline void gpio_set_dir(uint, int) {}
static inline void gpio_pull_up(uint) {}
static inline void gpio_set_function(uint, int) {}
static inline void gpio_set_slew_rate(uint, int) {}
static inline void gpio_set_drive_strength(uint, int) {}

enum { clk_sys, clk_peri };
#define CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS 0
static inline uint32_t clock_get_hz(int) { return 125000000; }
static inline void clock_configure(int, int, int, uint32_t, uint32_t) {}

enum { DMA_SIZE_16 = 1 };
#define DREQ_ADC 99
typedef struct { int dreq; } dma_channel_config;
static inline int dma_claim_unused_channel(bool) { static int n = 0; return n++; }
static inline dma_channel_config dma_channel_get_default_config(int) { return {0}; }
static inline void channel_config_set_transfer_data_size(dma_channel_config *, int) {}
static inline void channel_config_set_read_increment(dma_channel_config *, bool) {}
static inline void channel_config_set_write_increment(dma_channel_config *, bool) {}
static inline void channel_config_set_dreq(dma_channel_config *c, uint d) { c->dreq = d; }
void dma_channel_configure(int ch, const dma_channel_config *c, volatile void *w, const volatile void *r, uint n, bool trig);
void dma_channel_set_read_addr(int ch, const volatile void *r, bool trig);
void dma_channel_set_trans_count(int ch, uint n, bool trig);
static inline void dma_channel_wait_for_finish_blocking(int) {}
static inline bool dma_channel_is_busy(int) { return false; }
static inline void dma_channel_abort(int) {}
static inline void dma_channel_unclaim(int) {}

typedef struct { volatile uint32_t fifo; } adc_hw_t;
extern adc_hw_t adcHw;
#define adc_hw (&adcHw)
extern float adcDiv;
static inline void adc_init() {}
static inline void adc_gpio_init(uint) {}
static inline void adc_select_input(uint) {}
static inline void adc_fifo_setup(bool, bool, int, bool, bool) {}
static inline void adc_set_clkdiv(float d) { adcDiv = d; }
static inline void adc_run(bool) {}
static inline void adc_fifo_drain() {}

static inline uint pwm_gpio_to_slice_num(uint) { return 0; }
static inline void pwm_set_clkdiv(uint, float) {}
static inline void pwm_set_wrap(uint, int) {}
static inline void pwm_set_gpio_level(uint, int) {}
static inline void pwm_set_enabled(uint, bool) {}
