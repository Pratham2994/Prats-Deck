# Prats Deck: handoff notes

Paste this file into a new Claude Code session to continue the project.

## The hardware

- Raspberry Pi Pico 2 W (RP2350) with a Waveshare Pico-ResTouch-LCD-2.8 (ST7789 320x240 screen, XPT2046 resistive touch, used with a stylus).
- Pins: DC 8, CS 9, SCK 10, MOSI 11, MISO 12, backlight 13, reset 15, touch CS 16, touch IRQ 17. The screen and touch share SPI1.
- Scope input on GP26. A test square wave comes out on GP0.

## Build settings (Arduino IDE 2)

- Board package: "Raspberry Pi Pico/RP2040/RP2350" by Earle F. Philhower. Version 6.2.0 was used.
  Boards Manager URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
- Board: Raspberry Pi Pico 2W. **CPU Speed: 125 MHz** (SPI = clk_peri / 2 = 62.5 MHz, the ST7789 rating). **Optimize: Small (-Os)**, the default. Flash size: leave at the default.
- **Do not use -O3 in the Tools menu.** With board package 6.2.0 the board then sends a bad USB descriptor (Windows: "Invalid Configuration Descriptor"). There is no COM port after that, so you need BOOTSEL to upload again. The fault is in the package's USB code, not in the sketch.
- The sketch's own code is built with `#pragma GCC optimize ("O2")` (top of `Prats-Deck.ino`). On the board this gave about 3 times the frame rate of -Os. -O3 in the pragma gave no more speed.
- No extra libraries. The fonts are in `src/fonts/inter.h` (Inter, SIL Open Font License).
- The sketch folder must be named `Prats-Deck`, the same as `Prats-Deck.ino`.
- Command-line build (arduino-cli ships inside the IDE, under `resources/app/lib/backend/resources`):
  `arduino-cli compile --fqbn "rp2040:rp2040:rpipico2w:freq=125,opt=Small" --warnings all Prats-Deck`
- Do not start two builds of the sketch at the same time (Verify and Upload together). They share one build folder, and the link then fails with "access beyond end of merged section".
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
  - `chindi_games.h`: the 4 mini-games. It is included inside `namespace chindi`.
- EEPROM (1024 bytes): calibration at 0, Bricks high score at 64, brightness at 72, tear-free switch at 73, Snake high score at 80, Chindi's save data at 256 (magic "CHN2").
- The deck is a USB keyboard and a USB mouse (`Keyboard.begin()`, `Mouse.begin()`). To try a change that could break USB or crash, build with `-DDECK_TEST`: if the firmware hangs (watchdog, 5 s), or if no PC sees it on USB 12 s after start-up, the deck goes to boot mode by itself, so it does not need the BOOTSEL button. Do not keep such a build on the deck: on a charger it would go to boot mode. The mouse was added on 2026-10-02 and tested this way: Windows shows the keyboard, the media keys, the mouse and the serial port. The USB product ID changed with it (F00E to F00C), so Windows gave the deck a new COM port number (COM4 became COM5 on the test PC): select the new port in the Arduino IDE. The Trackpad app itself was tested in the simulator only, not with a stylus on the screen.
- The home page has 5 x 3 tiles (`HOME_COLS`), so 15 apps fit. The Guide list has 3 x 5 names. A new app needs: its header, a line in `APPS`, a page in `app_guide.h`, a row in the README.
- `pc_monitor.py`: sends PC stats to the Monitor app, one line per second. On Windows it also sends the song that plays (`NP state title<TAB>artist`), read by a small PowerShell helper from the Windows media controls. `pcstats::song()` gives it to the Macros app. The deck also answers a line with only `?` on the serial port with `DECK fps=.. heap=.. up=..`: a quick check that the firmware runs, and how fast. Not tested with a real song yet: only with no song, and with made-up lines in the simulator. Finds the Pico by USB vendor ID. Needs `pip install psutil pyserial`.

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
- On the board (2026-10-02): the firmware starts and USB works. Chindi (living room, by day) ran at 5 frames a second with -Os. With the O2 pragma, quicker drawing code (colour fades, rings, lines, cat shapes) and both processor cores, it runs at 40 to 44. One frame is about 24 ms: the cat 9, the room 6.5, the stats bar 2.5, the props 2, the dock 2. With one core it was 29 frames a second (34 ms).
- All the other apps and the home page run at 40 to 46 frames a second. 46 is the limit: one picture takes 20 ms to send.
- Two cores (`src/twocore.h`): core 1 has no work of its own, so `onBothCores` gives it a part of a drawing job. The shape renderer (`cg::render`) and the room background (`room::draw`) use it: each core draws its own rows, and the split row moves by itself until both parts take the same time. Every drawing function in `display.h` keeps to the row window of its core (`rowLo`, `rowHi`, `rowOk`). A function that is drawn by both cores must only draw: no changes to data, no input handling. In the simulator both parts run one after the other, so its pictures also prove that no function draws outside its rows.
- Frame timing: add `--build-property "compiler.cpp.extra_flags=-DDECK_PROF=0"` to the arduino-cli build. The deck then opens that app (0 = Chindi, -1 = home, -2 = each app in turn) and prints the time of each stage on USB serial once a second. See `PROF_MARK` in `display.h`. `core0` and `core1` in that line are the times of the two cores' parts of the shared jobs.
- The Chindi room is never dimmed by the clock. Only her light switch and Sleep dim it, to 75%.
- In that test the tear-free mode was off (`lcdSync` was 0). The cause was not examined: the screen gave no answer, or the switch in Settings is off.
- **Not tested on the real board yet:** the tear-free mode (it needs the screen to answer on SPI; the picture direction in that mode comes from the ST7789 data sheet, not from a test), real Wi-Fi weather on the home page, the scope ADC, and Windows media keys.
- If the picture is turned or mirrored with the tear-free mode on: switch it off in Settings (or set `TEAR_FREE = false` in `display.h`) and correct `lcdTurn`.

## Working rules (from the user's CLAUDE.md)

- Use simple, short sentences (ASD-STE100 style) when talking to the user.
- Do the smallest change that solves the problem. Read the code first. Run the real build (arduino-cli) before saying something is done.
- Git: short commit messages, no co-author or AI lines. Never commit `config.h`.
