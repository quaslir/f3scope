#!/usr/bin/env python3
"""f3scope host viewer.

Reads frames from the STM32 over a serial port and plots them live.

Packet format (all multi-byte fields little-endian):
    0xAA 0x55 | count (u16) | samples[count] (u16 each) | checksum (u16)
    checksum = sum(samples) & 0xFFFF

Text lines the board prints between frames (printf, ASCII, ending in '\\n')
are shown in the terminal as "[board] ...". ASCII never contains 0xAA, so text
can never be mistaken for a frame. A line "settings: rate=<N> Hz ..." updates
the time axis automatically, so --rate is only the starting value.

Usage:
    python scope.py                      # /dev/ttyACM0, 115200 baud, 100 kHz
    python scope.py --port /dev/ttyACM0 --baud 1500000 --rate 100000
    python scope.py --text               # no GUI, just print frame stats
"""

import argparse
import re
import sys
import threading
import time

import numpy as np
import serial

SYNC0 = 0xAA
SYNC1 = 0x55
MAX_COUNT = 10_000
ADC_MAX = 4095
VREF = 3.3
MAX_TEXT_LINE = 200

RATE_RE = re.compile(rb"rate=(\d+)")
TEXT_LINE_RE = re.compile(rb"[A-Za-z][ -~]{3,}")


def print_board_line(line):
    print(f"[board] {line}", flush=True)


class FrameReader:
    """Reads packets from a byte stream, resynchronising on errors.

    Bytes skipped while looking for the sync word are collected into text
    lines and passed to on_text.
    """

    def __init__(self, ser, rate_hz=100_000, on_text=print_board_line):
        self.ser = ser
        self.frames_ok = 0
        self.frames_bad = 0
        self.rate_hz = rate_hz
        self.on_text = on_text
        self._text = bytearray()

    def _read_exact(self, n):
        data = bytearray()
        while len(data) < n:
            chunk = self.ser.read(n - len(data))
            if not chunk:
                return None  # timeout
            data += chunk
        return bytes(data)

    def _text_byte(self, b):
        """Collect printable ASCII between frames into lines."""
        if b == 0x0A:  # '\n'
            line = bytes(self._text)
            self._text.clear()
            # real messages start with a letter and are a few chars long;
            # short printable runs inside binary data are ignored
            if TEXT_LINE_RE.fullmatch(line):
                self._handle_line(line)
        elif b == 0x0D:  # '\r'
            pass
        elif 0x20 <= b <= 0x7E and len(self._text) < MAX_TEXT_LINE:
            self._text.append(b)
        else:
            # binary garbage (e.g. the rest of a broken frame): drop the line
            self._text.clear()

    def _handle_line(self, line):
        m = RATE_RE.search(line)
        if m:
            self.rate_hz = int(m.group(1))
        if self.on_text is not None:
            self.on_text(line.decode("ascii"))

    def _find_sync(self):
        prev = None
        while True:
            b = self.ser.read(1)
            if not b:
                return False  # timeout
            b = b[0]
            if prev == SYNC0 and b == SYNC1:
                self._text.clear()
                return True
            if b != SYNC0:
                self._text_byte(b)
            prev = b

    def read_frame(self):
        """Return a numpy array of samples, or None on timeout/bad packet."""
        if not self._find_sync():
            return None

        raw = self._read_exact(2)
        if raw is None:
            return None
        count = int.from_bytes(raw, "little")
        if not 0 < count <= MAX_COUNT:
            self.frames_bad += 1
            return None

        payload = self._read_exact(count * 2)
        footer = self._read_exact(2)
        if payload is None or footer is None:
            return None

        samples = np.frombuffer(payload, dtype="<u2")
        expected = int(samples.sum(dtype=np.uint64)) & 0xFFFF
        received = int.from_bytes(footer, "little")
        if expected != received:
            self.frames_bad += 1
            return None

        self.frames_ok += 1
        return samples


def start_command_input(ser):
    """Send lines typed in this terminal to the board (e.g. 'RATE 10000')."""

    def loop():
        for line in sys.stdin:
            cmd = line.strip().upper()
            if not cmd:
                continue
            try:
                ser.write((cmd + "\n").encode("ascii"))
                print(f"[pc] {cmd}", flush=True)
            except (UnicodeEncodeError, serial.SerialException) as e:
                print(f"[pc] cannot send '{cmd}': {e}", flush=True)

    threading.Thread(target=loop, daemon=True).start()


def run_text(reader):
    while True:
        samples = reader.read_frame()
        if samples is None:
            print(f"(no valid frame)  ok={reader.frames_ok} bad={reader.frames_bad}")
            continue
        print(
            f"frame ok: count={len(samples)} min={samples.min()} max={samples.max()}"
            f"  rate={reader.rate_hz} Hz  ok={reader.frames_ok} bad={reader.frames_bad}"
        )


def run_gui(reader):
    import pyqtgraph as pg
    from pyqtgraph.Qt import QtCore

    latest = {"samples": None, "rate": reader.rate_hz}
    lock = threading.Lock()
    stop = threading.Event()

    def worker():
        while not stop.is_set():
            samples = reader.read_frame()
            if samples is not None:
                with lock:
                    latest["samples"] = samples
                    latest["rate"] = reader.rate_hz

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()

    app = pg.mkQApp("f3scope")
    win = pg.PlotWidget(title="f3scope")
    win.setLabel("bottom", "time", units="s")
    win.setLabel("left", "voltage", units="V")
    win.setYRange(0, VREF)
    win.showGrid(x=True, y=True, alpha=0.3)
    curve = win.plot(pen=pg.mkPen("y", width=2))
    win.resize(1000, 500)
    win.show()

    fps = {"t": time.monotonic(), "n": 0, "value": 0.0}

    def update():
        with lock:
            samples = latest["samples"]
            rate_hz = latest["rate"]
            latest["samples"] = None
        if samples is None:
            return
        t = np.arange(len(samples)) / rate_hz
        v = samples.astype(np.float32) * (VREF / ADC_MAX)
        curve.setData(t, v)

        fps["n"] += 1
        now = time.monotonic()
        if now - fps["t"] >= 1.0:
            fps["value"] = fps["n"] / (now - fps["t"])
            fps["t"], fps["n"] = now, 0
        win.setTitle(
            f"f3scope  |  {rate_hz / 1000:g} kS/s  |  {fps['value']:.1f} fps  |  "
            f"min {v.min():.3f} V  max {v.max():.3f} V  Vpp {v.max() - v.min():.3f} V  |  "
            f"ok {reader.frames_ok}  bad {reader.frames_bad}"
        )

    timer = QtCore.QTimer()
    timer.timeout.connect(update)
    timer.start(20)

    try:
        app.exec()
    finally:
        stop.set()


def main():
    p = argparse.ArgumentParser(description="f3scope host viewer")
    p.add_argument("--port", default="/dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--rate", type=int, default=100_000,
                   help="starting sample rate in Hz (updated by 'settings:' lines from the board)")
    p.add_argument("--text", action="store_true", help="print stats instead of plotting")
    args = p.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        sys.exit(f"cannot open {args.port}: {e}")

    ser.reset_input_buffer()
    reader = FrameReader(ser, rate_hz=args.rate)
    start_command_input(ser)
    print("type commands here: RATE <hz> | LEVEL <0-4095> | EDGE R|F | MODE A|N|S", flush=True)

    try:
        if args.text:
            run_text(reader)
        else:
            run_gui(reader)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
