# Pico Deck: handoff notes

Paste this file into a new Claude Code session to continue the project.

## The hardware

- Raspberry Pi Pico 2 W (RP2350) with a Waveshare Pico-ResTouch-LCD-2.8 (ST7789 320x240 screen, XPT2046 resistive touch, used with a stylus).
- Pins: DC 8, CS 9, SCK 10, MOSI 11, MISO 12, backlight 13, reset 15, touch CS 16, touch IRQ 17. The screen and touch share SPI1.
- Scope input on GP26. A test square wave comes out on GP0.

## Build settings (Arduino IDE 2)

- Board package: "Raspberry Pi Pico/RP2040/RP2350" by Earle F. Philhower. Version 6.2.0 was used.
  Boards Manager URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
- Board: Raspberry Pi Pico 2W. **CPU Speed: 125 MHz** (SPI = clk_peri / 2 = 62.5 MHz, the ST7789 rating). **Optimize: -O3**. Flash size: leave at the default.
- No extra libraries. The fonts are copied into `src/fonts` (Adafruit GFX, BSD licence).
- Command-line build (arduino-cli ships inside the IDE):
  `arduino-cli compile --fqbn "rp2040:rp2040:rpipico2w:freq=125,opt=Optimize3" --warnings all picodeck`
- Uploading fails with "No drive to deploy" when another program (`pc_monitor.py`, the Serial Monitor) holds the COM port. Close it first, or hold BOOTSEL while plugging in.

## How it works

- `picodeck.ino`: the app table, the home screen (4x3 grid), the hold-top-left home gesture, setup and loop.
- Frame loop: two 153 KB frame buffers. DMA sends one to the screen while the CPU draws the next. Measured: 45 to 46 FPS on Galaxy.
- `src/display.h`: SPI and DMA, drawing primitives, fonts, seven-segment digits.
- `src/touch.h`: the touch driver, the calibration (stored in EEPROM at 0), and the filtered tracker `T`. The tracker gives down, pressed, released, tap, smoothed position and velocity, and flick velocity. A lift counts only after 100 ms with no contact, to hide stylus contact losses.
- `src/glow.h`: a shared 160x120 "heat" grid drawn through palettes, plus spark particles.
- `src/core.h`: the App struct, `appMem` (10 KB shared by apps, via `APP_STATE`), the EEPROM layout, and the UI helpers: `card`, `pill`, `appHeader`, `button`, `toolbar`.
- The apps (one header each, in namespaces): chindi, galaxy, macro (USB HID), pcstats (USB serial from `pc_monitor.py`), clockapp (Wi-Fi, NTP, open-meteo, ip-api), wifiscan, scope (ADC + DMA), focus, paint, bricks, life, settings.
- Chindi (the pet cat):
  - `chindi_gfx.h`: an anti-aliased span renderer for ellipses, capsules and convex polygons, with outlines, groups, clips and gradients.
  - `chindi_cat.h`: the cat model and poses.
  - `chindi_room.h`: the rooms, the sky and weather, and the props.
  - `app_chindi.h`: the save data, needs, particles, keyboard walk, the Focus link, and the home badge and peek.
  - `chindi_play.h`: behaviour, touch, play modes, UI sheets, gallery and the frame function.
  - `chindi_games.h`: the 3 mini-games. It is included inside `namespace chindi`.
- EEPROM (1024 bytes): calibration at 0, Bricks high score at 64, brightness at 72, Chindi's save data at 256.
- `config.h`: Wi-Fi SSID and password, weather location, °C/°F, 12/24 h. **It contains the Wi-Fi password: do not share or commit it.**
- `pc_monitor.py`: sends PC stats to the Monitor app, one line per second. Needs `pip install psutil pyserial`.

## The PC simulator (tools/sim)

The simulator runs the real sketch on a PC with fake hardware, drives touch input, saves screenshots, and checks results. It needs g++ (MinGW, for example Strawberry Perl or MSYS2) and Python.

```
cd picodeck/tools/sim
g++ -std=gnu++17 -O1 -Wno-format -Iinclude sim_chindi.cpp -o simchindi.exe && ./simchindi.exe   # 28 Chindi checks
g++ -std=gnu++17 -O1 -Wno-format -Iinclude sim_main.cpp -o sim.exe && ./sim.exe                  # the other apps
TOUCHTEST=1 ./sim.exe                                                                            # stylus contact-loss test
python ppm2png.py                                                                                # out/*.ppm -> png
```

## Status

- Everything compiles with no warnings from the project code. About 61 KB of RAM is free.
- Tested on the real board: Galaxy (46 FPS), Monitor, and uploading.
- Not tested on the real board yet: Chindi's speed, real Wi-Fi weather, the scope ADC, and Windows media keys.
- The simulator covers all features. All its checks pass.

## Working rules (from the user's CLAUDE.md)

- Use simple, short sentences (ASD-STE100 style) when talking to the user.
- Do the smallest change that solves the problem. Read the code first. Run the real build (arduino-cli) before saying something is done.
- Git: no co-author or AI lines in commits. Never commit `config.h` secrets.
