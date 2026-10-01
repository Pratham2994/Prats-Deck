// config.example.h  ->  copy this file to config.h, then edit it
// Your settings. Edit these, then upload again.

// Wi-Fi, for the Clock & Weather app. The Wi-Fi Scanner works without it.
// The Pico 2 W only supports 2.4 GHz networks.
#define WIFI_SSID ""
#define WIFI_PASS ""

// Weather location. Leave at 0, 0 to find it from your internet address.
#define WEATHER_LAT 0.0
#define WEATHER_LON 0.0
#define WEATHER_CITY ""        // name to show when you set the location above

#define USE_FAHRENHEIT 0       // 1 = °F, 0 = °C
#define CLOCK_24H 1            // 1 = 24-hour clock, 0 = 12-hour clock
