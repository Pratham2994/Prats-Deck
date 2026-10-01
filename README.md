# Pico Deck

Twelve touch apps for the Waveshare Pico-ResTouch-LCD-2.8 on a Raspberry Pi Pico 2 W.

## Upload

1. Open `picodeck.ino` in Arduino IDE.
2. Tools menu: **Board** Raspberry Pi Pico 2W, **CPU Speed** 125 MHz, **Optimize** Optimize Even More (-O3).
3. Click Upload.

Your touch calibration from `galaxy.ino` carries over. To redo it, open **Settings > Touch calibration**.

## Use

- Tap an app to open it.
- **Go home:** hold the top-left corner (the small arrow) for half a second.

## Apps

| App | What it does |
|---|---|
| Chindi | Virtual pet cat (see below). |
| Galaxy | Particles swirl round the stylus, follow drags, and fly when flicked. Tap the top-right corner for colours. |
| Macros | USB media keys and Windows shortcuts. The Pico acts as a keyboard; no PC software. |
| Monitor | Live CPU, RAM, GPU, temperatures and network from your PC. Run `pc_monitor.py` on the PC. |
| Clock | Internet clock, weather and 3-day forecast, with a background that matches the weather. Needs Wi-Fi in `config.h`. |
| Wi-Fi | Scans nearby networks. List and channel chart, plus the least busy channel for your router. |
| Scope | Oscilloscope on GP26 (0 to 3.3 V). Touch GP26 to see mains hum. Turn on Test and wire GP0 to GP26 for a 1 kHz wave. |
| Focus | Pomodoro timer with a burning-fuse ring. Keeps running in the background. Tap the centre to start or pause, hold it to reset. |
| Paint | Paint with light, with a 2-way or 6-way kaleidoscope mirror. |
| Bricks | Breakout. Drag to move, tap to launch. The high score is saved. |
| Life | Conway's Game of Life with glowing trails. Draw to add cells. |
| Settings | Screen brightness, touch calibration, Chindi's keyboard walk, device info. |

## Chindi

An orange and white cat drawn from smooth shapes. Her needs (food, energy, fun, clean, love) change only while the Pico is on, and are saved in flash.

- **Touch:** stroke her head, chin or back to pet her. Tap her nose to boop it, or her eyes for a slow blink. Belly rubs are a gamble. Do not pull her tail.
- **Dock:** Feed (kibble, fish, treats), Play (laser, feather, yarn, mini-games), Clean (brush her), Sleep, More.
- **More:** Games (Fish Catch, Mouse Whack, Laser Chase), Wardrobe, Rooms, Photo, Gallery, Settings.
- **On her own:** wanders, grooms, kneads, loafs, stares at nothing, gets the zoomies, pushes the cup off the table, sits in boxes, naps in the sunbeam, watches the rain, sleeps at night.
- **Links:** the window shows the real weather and time (from the Clock app). Finished Focus sessions earn treats, and she yawns if you skip a break. When `pc_monitor.py` reports a busy CPU, she lies on the warm laptop. She peeks in on the home screen.
- **Keyboard walk:** when it is on, she sometimes walks on your keyboard and types a few letters on your PC (about every 25 to 60 minutes, never Enter). Turn it off in Settings.
- **Progress:** bond levels 1 to 10 unlock accessories, toys and rooms. Tap the stats bar to see her profile. Daily streaks give treats.

## Wi-Fi (Clock app)

Edit `config.h`: set `WIFI_SSID` and `WIFI_PASS`, then upload again. The Pico 2 W supports 2.4 GHz networks only.
Your location comes from your internet address. To set it yourself, fill in `WEATHER_LAT`, `WEATHER_LON` and `WEATHER_CITY`.

## PC stats (Monitor app)

```
pip install psutil pyserial
python pc_monitor.py
```

Close the Arduino Serial Monitor first, because only one program can use the port. GPU data needs an NVIDIA card. On Windows, the CPU temperature shows `--`, because psutil cannot read it there.

## Files

- `picodeck.ino`: home screen and app list
- `config.h`: your settings
- `src/display.h`: screen, DMA, drawing, text
- `src/touch.h`: touch, calibration, stylus filtering
- `src/glow.h`: shared glow layer and sparks
- `src/app_*.h`: one file per app
- `src/chindi_*.h`: Chindi's drawing, rooms, behaviour and games
- `src/fonts/`: Adafruit GFX fonts (BSD licence, see the licence file there)
