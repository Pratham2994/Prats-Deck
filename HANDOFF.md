# Prats Deck: handoff notes

Paste this file into a new Claude Code session to continue the project.

## The hardware

- Raspberry Pi Pico 2 W (RP2350) with a Waveshare Pico-ResTouch-LCD-2.8 (ST7789 320x240 screen, XPT2046 resistive touch, used with a stylus).
- Pins: DC 8, CS 9, SCK 10, MOSI 11, MISO 12, backlight 13, reset 15, touch CS 16, touch IRQ 17. The screen and touch share SPI1.
- Scope input on GP26. A test square wave comes out on GP0.

## Build settings (Arduino IDE 2)

- Board package: "Raspberry Pi Pico/RP2040/RP2350" by Earle F. Philhower. Version 6.2.0 was used.
  Boards Manager URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
- Board: Raspberry Pi Pico 2W. **CPU Speed: 125 MHz** (SPI = clk_peri / 2 = 62.5 MHz, the ST7789 rating). **Optimize: -O3**. Flash size: leave at the default.
- No extra libraries. The fonts are in `src/fonts/inter.h` (Inter, SIL Open Font License).
- The sketch folder must be named `Prats-Deck`, the same as `Prats-Deck.ino`.
- Command-line build (arduino-cli ships inside the IDE, under `resources/app/lib/backend/resources`):
  `arduino-cli compile --fqbn "rp2040:rp2040:rpipico2w:freq=125,opt=Optimize3" --warnings all Prats-Deck`
- Uploading fails with "No drive to deploy" when another program (`pc_monitor.py`, the Serial Monitor) holds the COM port. Close it first, or hold BOOTSEL while plugging in.
- `config.h` holds the Wi-Fi password. It is in `.gitignore`. `config.example.h` is the template.

## How it works

- `Prats-Deck.ino`: the app table, the home page (clock header, 4x3 grid, Chindi mood chip), the hold-top-left home gesture, setup and loop.
- `src/display.h`:
  - SPI and DMA. Two 153 KB buffers.
  - **Tear-free mode.** The screen redraws along its long side; a landscape picture is sent along the short side, so a slanted tear shows. The fix: `lcdTurn` copies the landscape buffer into the screen's own order (MADCTL 0x00), the redraw is slowed to about 45 Hz (FRCTR2 0x19), and `lcdSyncWait` reads the scan line (command 0x45, bit-banged, on MISO or MOSI) and starts a send only when it cannot cross the redraw. `lcdSyncProbe` tests at start-up if the screen answers. No answer: the old double-buffer path is used. Settings has a switch (EEPROM byte 73).
  - Smooth (anti-aliased) shapes: circles, rounded boxes, triangles, lines, arcs. `gradRect` is a dithered colour fade.
  - Text: Inter in 4-bit coverage, sizes TINY, SMALL, MEDIUM, LARGE, plus digits-only GIANT and CLOCKFACE. Made by `tools/fonts/make_fonts.py`.
- `src/touch.h`: the touch driver, the calibration (EEPROM at 0), and the filtered tracker `T` (down, pressed, released, tap, smoothed position and velocity, flick velocity). A lift counts only after 100 ms with no contact.
- `src/glow.h`: a shared 160x120 "heat" grid drawn through palettes, plus spark particles.
- `src/core.h`: the App struct, `appMem` (10 KB shared by apps, via `APP_STATE`), the EEPROM layout, and the shared look: `backdrop`, `card`, `pill`, `appHeader`, `button`, `toolbar`.
- The apps (one header each, in namespaces): chindi, galaxy, macro (USB HID), pcstats (USB serial from `pc_monitor.py`), clockapp (Wi-Fi, NTP, open-meteo, ip-api; `service()` also runs from the home page), wifiscan, scope (ADC + DMA), guide, paint, bricks, life, settings.
- Chindi (the pet cat):
  - `chindi_gfx.h`: an anti-aliased span renderer for ellipses, capsules and convex polygons, with outlines, groups, clips and gradients.
  - `chindi_cat.h`: the cat model and poses (sit, loaf, groom, knead, walk, run, crouch, pounce, bat, eat, curl, flop).
  - `chindi_room.h`: the rooms (living, dining, bedroom, balcony), their tappable spots (`DEFS`, `spotAt`), the sky and weather, the props, the gifts.
  - `app_chindi.h`: the save data, needs, particles, icons, discoveries, wishes, keyboard walk, home mood and peek.
  - `chindi_play.h`: behaviour (`behave`), touch, room taps (`useSpot`), play modes, UI sheets, gallery, album and the frame function. `jumpOnto` / `jumpOff` move her onto the perch, the table, the bed and the laptop.
  - `chindi_games.h`: the 3 mini-games. It is included inside `namespace chindi`.
- EEPROM (1024 bytes): calibration at 0, Bricks high score at 64, brightness at 72, tear-free switch at 73, Chindi's save data at 256 (magic "CHN2").
- `pc_monitor.py`: sends PC stats to the Monitor app, one line per second. Finds the Pico by USB vendor ID. Needs `pip install psutil pyserial`.

## The PC simulator (tools/sim)

The simulator runs the real sketch on a PC with fake hardware, drives touch input, saves screenshots, and checks results. It needs g++ (MinGW, for example Strawberry Perl or MSYS2) and Python.

```
cd Prats-Deck/tools/sim
mkdir out
g++ -std=gnu++17 -O1 -Wno-format -Iinclude sim_chindi.cpp -o simchindi.exe && ./simchindi.exe   # Chindi tour and checks
g++ -std=gnu++17 -O1 -Wno-format -Iinclude sim_main.cpp -o sim.exe && ./sim.exe                  # the other apps
g++ -std=gnu++17 -O1 -Wno-format -Iinclude -DSIM_FORCE_SYNC sim_main.cpp -o simsync.exe          # the same, through the tear-free path
TOUCHTEST=1 ./sim.exe                                                                            # stylus contact-loss test
python ppm2png.py                                                                                # out/*.ppm -> png
```

## Status

- Everything compiles with the real compiler. The simulator covers all features and its checks pass.
- **Not tested on the real board yet:** all of the 2026-10 rework. Most important: the tear-free mode (it needs the screen to answer on SPI; the picture direction in that mode comes from the ST7789 data sheet, not from a test), the speed of Chindi with the new drawing, real Wi-Fi weather on the home page, the scope ADC, and Windows media keys.
- If the picture is turned or mirrored with the tear-free mode on: switch it off in Settings (or set `TEAR_FREE = false` in `display.h`) and correct `lcdTurn`.

## Working rules (from the user's CLAUDE.md)

- Use simple, short sentences (ASD-STE100 style) when talking to the user.
- Do the smallest change that solves the problem. Read the code first. Run the real build (arduino-cli) before saying something is done.
- Git: short commit messages, no co-author or AI lines. Never commit `config.h`.
