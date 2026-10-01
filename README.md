# Prats Deck

Fourteen touch apps for the Waveshare Pico-ResTouch-LCD-2.8 on a Raspberry Pi Pico 2 W.
A small desk companion: a pet cat, a clock with weather, PC controls, a PC monitor, tools and games.

## Set up

1. Copy `config.example.h` to `config.h`. Put your Wi-Fi name and password in it. `config.h` is not stored in git, so your password stays on your PC.
2. Make sure the folder is named `Prats-Deck`. Arduino needs the folder and the `.ino` file to have the same name.
3. Open `Prats-Deck.ino` in Arduino IDE 2.
4. Install the board package "Raspberry Pi Pico/RP2040/RP2350" by Earle F. Philhower (Boards Manager).
5. Tools menu: **Board** Raspberry Pi Pico 2W, **CPU Speed** 125 MHz, **Optimize** Small (-Os), the default. Do not use -O3: with board package 6.2.0 it breaks USB, and you then need the BOOTSEL button to upload again.
6. Click Upload. If the upload fails with "No drive to deploy", close `pc_monitor.py` and the Serial Monitor first, and make sure that Tools > Port shows the deck's port. (Windows gives the deck a new COM port number when its USB set-up changes, as it did when the Trackpad app was added.)

No extra libraries are needed.

## Use

- Tap an app to open it.
- **Go home:** hold the top-left corner (the small arrow) for half a second.
- The **Guide** app on the device explains each app. The table below says the same.

## The apps

| App | What it is | How to use it |
|---|---|---|
| Chindi | Your pet cat. She gets hungry, sleepy and bored, and she wants attention. | See the Chindi section. |
| Galaxy | A galaxy of glowing dots that follow the stylus. Only for fun. | Hold: the dots circle the stylus. Drag: they follow. Flick: they fly away. Tap the top-right corner for new colours. |
| Macros | Buttons that control your PC. The deck works as a USB keyboard, so the PC needs no software. | Top buttons: music and volume. Bottom buttons: Windows shortcuts (Copy, Paste, Lock, and more). Hold Vol + or Vol - to repeat. While `pc_monitor.py` runs, a strip under the buttons shows the song that plays on the PC. |
| Monitor | Shows how hard your PC works: processor (CPU), memory (RAM), graphics card (GPU), temperatures and network speed. | Run `python pc_monitor.py` on the PC and keep the USB cable connected. Tap the graph to change between CPU, RAM and GPU. |
| Clock | A clock set from the internet, with the weather for today and 2 more days. | Needs Wi-Fi in `config.h`. Tap the weather panel to refresh it. |
| Wi-Fi | Finds the Wi-Fi networks near you and shows how strong each one is. | List: all networks, the strongest first. Chart: which channels are crowded. The bottom line gives the best channel for your own router. |
| Scope | An oscilloscope: it draws how an electrical signal changes with time. A tool for electronics work. | Connect the signal to pin GP26. 3.3 V is the maximum. Zoom changes the time scale. Hold stops the picture. Test: wire GP0 to GP26 to see a 1 kHz wave. |
| Guide | Tells you what each app is and how to use it. | Tap a name. Prev and Next turn the pages. |
| Paint | Paint with light. Lines that cross become brighter. | Drag to paint. Mirror makes 2-way or 6-way patterns. Fade lets old lines go away. Clear starts again. |
| Bricks | The classic game: hit the ball with the bat and break all the bricks. | Drag to move the bat. Tap to launch the ball. You have 3 lives. The high score is saved. |
| Life | Conway's Game of Life, a famous simulation. Each dot lives or dies by the count of its neighbours. Simple rules make patterns that move and grow. | Draw on the screen to add living dots. Pause, change the speed, or start with a Random field. |
| Snake | The classic game: the snake grows when it eats. A wall or its own body ends the game. | When it goes sideways, tap above or below its head to turn it. When it goes up or down, tap left or right of its head. Gold food is worth 5. The high score is saved. |
| Trackpad | The screen is a mouse pad for your PC. The deck works as a USB mouse, so the PC needs no software. | Drag to move the pointer. Tap to click. Drag up or down in the strip at the right to scroll. Bottom buttons: left click, right click, and Hold, which keeps the left button down so that you can drag a window (tap it again to let go). |
| Settings | Brightness, touch calibration, tear-free screen, Chindi's keyboard walk, device info. | Tap a row to change it. |

## Chindi

Chindi is drawn after a real orange and white cat: orange cap, white blaze, striped saddle, ringed tail.
Her needs (food, energy, fun, clean, love) change only while the Pico is on. They are saved in flash.

**Touch her**

- Stroke her head, chin or back to pet her.
- Tap her nose to boop it. Tap an eye for a slow blink.
- Belly rubs are a gamble. Do not pull her tail.

**The room is the menu.** Tap a thing in her room and she goes there and uses it.

| Room | Things to tap |
|---|---|
| Living room | The blue water fountain (she drinks), the cat tree (she climbs up; tap the tall post and she scratches it), the brush on the wall corner (she rubs her cheek), the window (she watches the birds). At night the fountain glows. |
| Dining room (level 3) | The flowers (she jumps on the table and sniffs them, and can sneeze), the photo frames (your gallery), the window, the cushion. |
| Bedroom (level 5) | The bed, the light switch, the cup on the little table, the door (change rooms), the window. |
| Balcony (level 7) | The plants. Birds land on the railing. |

A tap on the open floor calls her over.

**The dock:** Feed (kibble, fish, treats), Play (laser, feather, yarn, mini-games), Clean (brush her), Sleep, More.

**The game**

- **Wishes:** she has 3 wishes a day, one at a time (the small chip under the stats bar). Tap the chip for a hint. Each wish gives a treat. With no Wi-Fi time, she gets new wishes each time the deck starts.
- **Gifts:** when all 3 wishes are done, and sometimes on her own, she brings you a small thing. Tap it to keep it. There are 12 to collect.
- **Album:** it shows your gifts and the 20 things you can find her doing. Open it from the stats bar or from More.
- **Levels:** bond levels 1 to 10 unlock rooms, toys and things to wear. Daily streaks give treats.
- **Mini-games:** Fish Catch, Mouse Whack, Laser Chase and Zoomies (she runs through the garden; tap to jump over the cucumbers and puddles).

**On her own** she wanders, grooms, kneads, loafs, flops on her side, stares at nothing, gets the zoomies, pushes the cup off the table, sits in boxes, naps in the sunbeam and sleeps at night. The window shows the real weather and time (from the Clock app). When `pc_monitor.py` reports a busy CPU, she lies on the warm laptop. When it reports that a song plays on the PC, she dances to it, with notes in the air, and her fun goes up.

**Keyboard walk:** when it is on, she sometimes walks on your keyboard and types a few letters on your PC (about every 25 to 60 minutes, never Enter). The letters go into the window that is active on the PC. Turn it off in Settings if that is a problem.

## Tear-free screen

On this screen, a slanted line can cross pictures that move. The firmware removes it: it sends each picture in step with the screen's own redraw. For this, the screen must answer a question from the Pico, and the firmware tests that at start-up.

Settings shows the result:

- **on:** the mode works.
- **off:** you switched it off.
- **not available:** your screen gives no answer. The firmware then sends pictures the old way.

## Wi-Fi (Clock app and home page)

Set `WIFI_SSID` and `WIFI_PASS` in `config.h`, then upload again. The Pico 2 W supports 2.4 GHz networks only.
Your location comes from your internet address. To set it yourself, fill in `WEATHER_LAT`, `WEATHER_LON` and `WEATHER_CITY`.

## PC stats (Monitor app)

```
pip install psutil pyserial
python pc_monitor.py
```

The script finds the Pico by itself, on any COM port. On Windows it also sends the song that plays on the PC (title and artist, from the Windows media controls), and the Macros app shows it. Letters that are not in the deck's fonts show as `?`. Close the Arduino Serial Monitor first, because only one program can use the port. GPU data needs an NVIDIA card. On Windows, the CPU temperature shows `--`, because psutil cannot read it there.

## Files

- `Prats-Deck.ino`: home page and app list
- `config.example.h`: template for your settings (`config.h`)
- `src/display.h`: screen, tear-free sending, smooth shapes, text
- `src/touch.h`: touch, calibration, stylus filtering
- `src/glow.h`: shared glow layer and sparks
- `src/core.h`: shared look (cards, header, buttons)
- `src/app_*.h`: one file per app
- `src/chindi_*.h`: Chindi's drawing, rooms, behaviour and games
- `src/fonts/inter.h`: the Inter typeface as smooth fonts (SIL Open Font License, see the licence file there)
- `tools/fonts/make_fonts.py`: makes `inter.h` from the Inter font files
- `tools/sim/`: runs the real code on a PC and saves screenshots
- `pc_monitor.py`: sends PC stats to the Monitor app
