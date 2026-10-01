"""pc_monitor.py - send live PC stats to the Prats Deck "Monitor" app over USB.

Setup (once):
    pip install psutil pyserial

Run:
    python pc_monitor.py            # finds the Pico by itself
    python pc_monitor.py COM5       # or name the port

Close the Arduino Serial Monitor first: only one program can open the port.
GPU usage and temperature come from nvidia-smi when an NVIDIA GPU is present.
On Windows, the song that plays on the PC is sent too. The Macros app shows it.
"""

import os
import shutil
import subprocess
import sys
import threading
import time
import unicodedata

try:
    import psutil
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("Missing packages. Run:  pip install psutil pyserial")

PICO_VID = 0x2E8A  # Raspberry Pi


def find_port():
    for p in list_ports.comports():
        if p.vid == PICO_VID:
            return p.device
    return None


def gpu_stats():
    """(usage %, temp C) from nvidia-smi, or (-1, -1)."""
    if not shutil.which("nvidia-smi"):
        return -1, -1
    try:
        out = subprocess.run(
            ["nvidia-smi", "--query-gpu=utilization.gpu,temperature.gpu", "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=2,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        ).stdout.strip().splitlines()[0]
        use, temp = (float(x) for x in out.split(","))
        return use, temp
    except Exception:
        return -1, -1


def cpu_temp():
    """CPU temperature in C, or -1. psutil only supports this on Linux."""
    try:
        temps = getattr(psutil, "sensors_temperatures", dict)()
        for name in ("coretemp", "k10temp", "cpu_thermal", "acpitz"):
            if temps.get(name):
                return temps[name][0].current
    except Exception:
        pass
    return -1


# Windows PowerShell script: once a second, print "state<TAB>title<TAB>artist" of the song that
# Windows shows on its media controls. state: 0 = nothing, 1 = playing, 2 = paused.
# It needs no extra Python package. It stops by itself when this program is gone: it looks
# for the process number PARENT, which is put in before the script starts.
SONG_SCRIPT = r"""
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
    $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
function Await($op, $type) {
    $t = $asTask.MakeGenericMethod($type).Invoke($null, @($op))
    $t.Wait(-1) | Out-Null
    $t.Result
}
$mgrType = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager, Windows.Media.Control, ContentType = WindowsRuntime]
$propType = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties, Windows.Media.Control, ContentType = WindowsRuntime]
$mgr = Await ($mgrType::RequestAsync()) $mgrType
while (Get-Process -Id PARENT -ErrorAction SilentlyContinue) {
    $line = "0`t`t"
    try {
        $s = $mgr.GetCurrentSession()
        if ($s) {
            $p = Await ($s.TryGetMediaPropertiesAsync()) $propType
            $state = if ([int]$s.GetPlaybackInfo().PlaybackStatus -eq 4) { 1 } else { 2 }
            $line = "$state`t$($p.Title)`t$($p.Artist)"
        }
    } catch { }
    [Console]::Out.WriteLine($line)
    [Console]::Out.Flush()
    Start-Sleep -Milliseconds 1000
}
"""


def plain(text, limit):
    """Text the deck can show: its fonts have the ASCII letters only."""
    text = unicodedata.normalize("NFKD", text)
    out = "".join(c if " " <= c <= "~" else ("" if unicodedata.combining(c) else "?") for c in text)
    out = " ".join(out.split())
    if out and not any(c.isalnum() for c in out):
        return "(title in another script)"
    return out[:limit]


def song_line(raw):
    """'state<TAB>title<TAB>artist' from the helper -> the line for the deck."""
    parts = (raw.split("\t") + ["", ""])[:3]
    state = parts[0] if parts[0] in ("1", "2") else "0"
    title = plain(parts[1], 55)
    if not title:
        state = "0"
    return "NP %s %s\t%s\n" % (state, title, plain(parts[2], 35))


class Song:
    """Follows the song on the PC with a small PowerShell helper (Windows only)."""

    def __init__(self):
        self.raw = "0\t\t"
        self.proc = None
        if sys.platform != "win32" or not shutil.which("powershell"):
            return
        try:
            self.proc = subprocess.Popen(
                ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                 SONG_SCRIPT.replace("PARENT", str(os.getpid()))],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            threading.Thread(target=self._read, daemon=True).start()
        except OSError:
            self.proc = None

    def _read(self):
        for line in self.proc.stdout:
            self.raw = line.decode("utf-8", "replace").rstrip("\r\n")
        self.raw = "0\t\t"                    # the helper ended: no song to show

    def line(self):
        return song_line(self.raw)

    def close(self):
        if self.proc:
            self.proc.terminate()


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else None
    song = Song()
    song_sent, song_at = None, 0.0
    psutil.cpu_percent()  # first call primes the counter
    last_net = psutil.net_io_counters()
    last_t = time.time()
    link = None
    stalled = False
    while True:
        try:
            if link is None:
                name = port or find_port()
                if not name:
                    print("Pico not found. Is it plugged in?")
                    time.sleep(2)
                    continue
                link = serial.Serial(name, 115200, timeout=1, write_timeout=1)
                print("Sending stats to", name, "- press Ctrl+C to stop")

            time.sleep(1)
            now = time.time()
            net = psutil.net_io_counters()
            span = max(now - last_t, 0.001)
            down = (net.bytes_recv - last_net.bytes_recv) / 1024 / span
            up = (net.bytes_sent - last_net.bytes_sent) / 1024 / span
            last_net, last_t = net, now

            mem = psutil.virtual_memory()
            gpu, gpu_t = gpu_stats()
            line = "PC %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.2f %.2f\n" % (
                psutil.cpu_percent(), mem.percent, gpu, cpu_temp(), gpu_t,
                down, up, mem.used / 2**30, mem.total / 2**30)
            link.write(line.encode())
            np = song.line()
            if np != song_sent or now - song_at >= 5:      # on a change, and again every 5 s
                link.write(np.encode())
                song_sent, song_at = np, now
            if stalled:
                print("Pico is reading again")
                stalled = False
        except serial.SerialTimeoutException:
            # The port is fine but the Pico is not reading (busy, or old firmware that
            # only reads in the Monitor app). Drop this update and keep the port open.
            if not stalled:
                print("Pico is not reading the data. Is the Prats Deck firmware running? Waiting...")
                stalled = True
            if link:
                link.reset_output_buffer()
        except serial.SerialException as e:
            print("Connection lost:", e)
            if link:
                link.close()
            link = None
            song_sent = None
            time.sleep(2)
        except KeyboardInterrupt:
            break
    song.close()


if __name__ == "__main__":
    main()
