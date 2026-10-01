"""pc_monitor.py - send live PC stats to the Pico Deck "PC Stats" app over USB.

Setup (once):
    pip install psutil pyserial

Run:
    python pc_monitor.py            # finds the Pico by itself
    python pc_monitor.py COM5       # or name the port

Close the Arduino Serial Monitor first: only one program can open the port.
GPU usage and temperature come from nvidia-smi when an NVIDIA GPU is present.
"""

import shutil
import subprocess
import sys
import time

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


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else None
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
            if stalled:
                print("Pico is reading again")
                stalled = False
        except serial.SerialTimeoutException:
            # The port is fine but the Pico is not reading (busy, or old firmware that
            # only reads in the Monitor app). Drop this update and keep the port open.
            if not stalled:
                print("Pico is not reading the data. Is the Pico Deck firmware running? Waiting...")
                stalled = True
            if link:
                link.reset_output_buffer()
        except serial.SerialException as e:
            print("Connection lost:", e)
            if link:
                link.close()
            link = None
            time.sleep(2)
        except KeyboardInterrupt:
            break


if __name__ == "__main__":
    main()
