"""hwtest.py - test the real Prats Deck from the PC, with nobody at the desk.

The deck must run a test build (-DDECK_TEST, see HANDOFF.md). A test build takes made-up
touches over USB serial, sends the picture on its screen, and does not write to flash, so
the test does not change the saved game, scores or settings.

    pip install pyserial
    python tools/hwtest.py            # the full tour: every app, with checks and pictures
    python tools/hwtest.py status     # only ask the deck how it is (works with a normal build too)

Pictures go to tools/hwtest_out/. Close pc_monitor.py and the Serial Monitor first.
The tour does not press the Macros keys and does not tap in Settings: those would act on the
PC or change settings. On the Trackpad it moves the PC's pointer a little and back, with no click.
"""

import ctypes
import os
import random
import struct
import sys
import time
import zlib

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("Missing package. Run:  pip install pyserial")

PICO_VID = 0x2E8A
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hwtest_out")


class Deck:
    def __init__(self):
        port = next((p.device for p in list_ports.comports() if p.vid == PICO_VID), None)
        if not port:
            sys.exit("Deck not found on USB.")
        self.ser = serial.Serial(port, 115200, timeout=1.5)
        time.sleep(0.3)
        self.lowest = 999
        self.fails = 0

    def send(self, line):
        self.ser.write((line + "\n").encode())

    def status(self, note=""):
        """Ask "?". Returns the answer as a dict, and prints it with the note."""
        self.ser.reset_input_buffer()
        self.send("?")
        for _ in range(4):
            line = self.ser.readline().decode("ascii", "replace").strip()
            if line.startswith("DECK"):
                kv = dict(p.split("=") for p in line.split()[1:])
                self.lowest = min(self.lowest, int(kv["fps"]))
                print("  %-30s app=%s fps=%s heap=%s" % (note, kv.get("app", "?"), kv["fps"], kv["heap"]))
                return kv
        sys.exit("The deck gives no answer (%s). If it hung, a test build is now in boot mode." % note)

    def tap(self, x, y, ms=90, wait=0.45):
        self.send("touch %d %d %d" % (x, y, ms))
        time.sleep(ms / 1000 + wait)

    def drag(self, x0, y0, x1, y1, ms=250, wait=0.4):
        self.send("drag %d %d %d %d %d" % (x0, y0, x1, y1, ms))
        time.sleep(ms / 1000 + wait)

    def home(self):
        self.tap(10, 10, 900, 0.6)             # hold the top-left corner

    def open(self, i):
        """Tap tile i of the home page (5 x 3 tiles)."""
        self.tap((i % 5) * 64 + 32, 54 + (i // 5) * 62 + 25, 90, 1.2)

    def check(self, ok, what):
        print("  [%s] %s" % ("PASS" if ok else "FAIL", what))
        self.fails += not ok

    def shot(self, name):
        """Save the picture on the deck's screen as tools/hwtest_out/<name>.png (2x)."""
        self.ser.reset_input_buffer()
        self.send("shot")
        head = b""
        t0 = time.time()
        while not head.endswith(b"\n") and time.time() - t0 < 3:
            head += self.ser.read(1)
        if not head.startswith(b"SHOT"):
            sys.exit("No picture from the deck. Is it a -DDECK_TEST build?")
        w, h = (int(v) for v in head.split()[1:3])
        self.ser.timeout = 4
        data = b""
        while len(data) < w * h * 2:
            part = self.ser.read(w * h * 2 - len(data))
            if not part:
                sys.exit("The picture is not complete.")
            data += part
        self.ser.timeout = 1.5
        rows = []
        for y in range(h):
            row = bytearray()
            for x in range(w):
                c = data[(y * w + x) * 2] | (data[(y * w + x) * 2 + 1] << 8)
                row += bytes(((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3)) * 2
            rows += [b"\x00" + bytes(row)] * 2

        def chunk(kind, body):
            return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, name + ".png"), "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w * 2, h * 2, 8, 2, 0, 0, 0))
                    + chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) + chunk(b"IEND", b""))


def pointer():
    """Where the PC's mouse pointer is (Windows only), or None."""
    if sys.platform != "win32":
        return None

    class Point(ctypes.Structure):
        _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

    p = Point()
    ctypes.windll.user32.GetCursorPos(ctypes.byref(p))
    return p.x, p.y


def tour(d):
    random.seed(7)
    d.home()
    st = d.status("home")
    d.check(st["app"] == "-1", "the deck is on the home page")
    d.shot("home")

    print("Chindi")
    d.open(0)
    st = d.status("Chindi opened")
    d.check(st["app"] == "0", "a tap on the first tile opens Chindi")
    d.shot("chindi")
    for x, y, what in [(160, 120, "her"), (250, 160, "the fountain"), (64, 120, "the cat tree"), (134, 70, "the window"), (200, 190, "the floor")]:
        d.tap(x, y, 120, 2.0)
        d.status("after a tap on " + what)
    d.drag(140, 110, 180, 112, 600, 1.0)
    d.status("after a stroke")
    d.tap(110, 216, 100, 1.0)                  # dock: Play
    d.tap(240, 214, 100, 1.0)                  # Games
    d.tap(240, 214, 100, 2.0)                  # Zoomies
    d.tap(200, 150, 60, 0.25)
    d.shot("zoomies")
    t0 = time.time()
    while time.time() - t0 < 20:               # jump now and then
        d.tap(200, 150, 60, random.uniform(0.5, 1.3))
    d.status("Zoomies after 20 s")
    d.tap(303, 12, 100, 1.0)                   # the X
    d.tap(210, 150, 100, 0.8)                  # (Back, if the game was over)
    d.home()
    st = d.status("home again")
    d.check(st["app"] == "-1", "a hold on the corner goes home from Chindi")

    print("Macros (no key is pressed)")
    d.open(2)
    for _ in range(3):                         # what pc_monitor.py sends while a song plays
        d.send("NP 1 Test Song (sent by hwtest)\tThe Test Band")
        time.sleep(1.0)
    st = d.status("Macros with a song")
    d.check(st["app"] == "2", "Macros opens")
    d.shot("macros_song")
    d.home()

    print("Snake")
    d.open(11)
    st = d.status("Snake opened")
    d.check(st["app"] == "11", "Snake opens")
    for k in range(16):
        if k % 4 < 2:
            d.tap(160, 60 if k % 2 == 0 else 200, 70, 0.35)
        else:
            d.tap(60 if k % 8 < 4 else 260, 130, 70, 0.35)
    d.status("Snake after 16 turns")
    d.shot("snake")
    d.home()

    print("2048")
    d.open(12)
    st = d.status("2048 opened")
    d.check(st["app"] == "12", "2048 opens")
    for k in range(16):
        dx, dy = [(-1, 0), (0, 1), (1, 0), (0, -1)][k % 4]
        d.drag(110, 130, 110 + dx * 80, 130 + dy * 80, 220, 0.35)
    d.status("2048 after 16 swipes")
    d.shot("2048")
    d.home()

    print("Trackpad (the PC's pointer moves a little, and back)")
    d.open(13)
    st = d.status("Trackpad opened")
    d.check(st["app"] == "13", "Trackpad opens")
    p0 = pointer()
    if p0:
        d.drag(160, 120, 100, 120, 400, 0.5)   # left
        p1 = pointer()
        d.drag(100, 120, 100, 80, 400, 0.5)    # up
        p2 = pointer()
        d.drag(100, 80, 100, 120, 400, 0.5)    # down
        d.drag(100, 120, 160, 120, 400, 0.5)   # right
        p3 = pointer()
        print("  PC pointer:", p0, "->", p1, "->", p2, "->", p3)
        d.check(p0[0] - p1[0] > 40 or p0[0] < 60, "a drag to the left moves the PC's pointer left")
        d.check(p1[1] - p2[1] > 25 or p1[1] < 40, "a drag up moves it up")
        d.check(abs(p3[0] - p0[0]) < 40 and abs(p3[1] - p0[1]) < 40, "the drags back bring it near the start")
    d.shot("trackpad")
    d.home()

    print("the other apps")
    for i, name in [(1, "Galaxy"), (3, "Monitor"), (4, "Clock"), (5, "Wi-Fi"), (6, "Scope"), (7, "Guide"), (8, "Paint"), (9, "Bricks"), (10, "Life"), (14, "Settings")]:
        d.open(i)
        time.sleep(1.5)
        if name in ("Galaxy", "Paint", "Life"):
            for _ in range(6):
                d.drag(random.randint(40, 280), random.randint(50, 190), random.randint(40, 280), random.randint(50, 190), 300, 0.2)
        if name == "Bricks":
            d.tap(160, 150, 80, 0.5)
            for _ in range(8):
                d.drag(random.randint(40, 280), 200, random.randint(40, 280), 200, 400, 0.2)
        if name == "Guide":
            d.tap(160, 130, 90, 0.8)
            d.tap(266, 222, 90, 0.8)
        st = d.status(name)
        d.check(st["app"] == str(i), name + " opens and runs")
        d.shot(name.lower().replace("-", ""))
        d.home()

    d.status("home at the end")
    print("lowest frame rate seen: %d" % d.lowest)
    print("pictures: %s" % OUT)
    print("%d checks failed" % d.fails)
    return d.fails


if __name__ == "__main__":
    deck = Deck()
    if len(sys.argv) > 1 and sys.argv[1] == "status":
        deck.status("deck")
        sys.exit(0)
    sys.exit(tour(deck))
