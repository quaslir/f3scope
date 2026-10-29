#!/usr/bin/env python3
"""f3scope host viewer.

Reads frames from the STM32 over a serial port and plots them live.

Packet format (all multi-byte fields little-endian):
    0xAA 0x55 | count (u16) | samples[count] (u16 each) | checksum (u16)
    checksum = sum(samples) & 0xFFFF

Usage:
    python scope.py                      # /dev/ttyACM0, 115200 baud, 100 kHz
    python scope.py --port /dev/ttyACM1 --baud 921600 --rate 100000
    python scope.py --text               # no GUI, just print frame stats
"""

import argparse
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


class FrameReader:
    """Reads packets from a byte stream, resynchronising on errors."""

    def __init__(self, ser):
        self.ser = ser
        self.frames_ok = 0
        self.frames_bad = 0

    def _read_exact(self, n):
        data = bytearray()
        while len(data) < n:
            chunk = self.ser.read(n - len(data))
            if not chunk:
                return None  # timeout
            data += chunk
        return bytes(data)

    def _find_sync(self):
        prev = None
        while True:
            b = self.ser.read(1)
            if not b:
                return False  # timeout
            b = b[0]
            if prev == SYNC0 and b == SYNC1:
                return True
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


def run_text(reader):
    while True:
        samples = reader.read_frame()
        if samples is None:
            print(f"(no valid frame)  ok={reader.frames_ok} bad={reader.frames_bad}")
            continue
        print(
            f"frame ok: count={len(samples)} min={samples.min()} max={samples.max()}"
            f"  ok={reader.frames_ok} bad={reader.frames_bad}"
        )


def run_gui(reader, rate_hz):
    import pyqtgraph as pg
    from pyqtgraph.Qt import QtCore

    latest = {"samples": None}
    lock = threading.Lock()
    stop = threading.Event()

    def worker():
        while not stop.is_set():
            samples = reader.read_frame()
            if samples is not None:
                with lock:
                    latest["samples"] = samples

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
            f"f3scope  |  {fps['value']:.1f} fps  |  "
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
    p.add_argument("--rate", type=float, default=100_000, help="sample rate in Hz")
    p.add_argument("--text", action="store_true", help="print stats instead of plotting")
    args = p.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        sys.exit(f"cannot open {args.port}: {e}")

    ser.reset_input_buffer()
    reader = FrameReader(ser)

    try:
        if args.text:
            run_text(reader)
        else:
            run_gui(reader, args.rate)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
