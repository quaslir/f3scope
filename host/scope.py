#!/usr/bin/env python3
"""f3scope host viewer.

Reads frames from the STM32 over a serial port, plots them live and sends
settings to the board.

Packet format (all multi-byte fields little-endian):
    0xAA 0x55 | count (u16) | samples[count] (u16 each) | checksum (u16)
    checksum = sum(samples) & 0xFFFF

Text lines the board prints between frames (printf, ASCII, ending in '\\n')
are shown in the terminal as "[board] ...". ASCII never contains 0xAA, so text
can never be mistaken for a frame. A line
    settings: rate=<hz> Hz level=<0-4095> edge=<rising|falling> mode=<A|N|S>
updates the time axis and the controls, so the window always shows what the
board actually uses.

After every frame the board may send
    meas: min=<n> max=<n> mean=<n> rms=<n> periodic=<0|1> freq_mhz=<n> duty=<permille>
(ADC counts, millihertz, permille); it is shown in the Measurements panel.

Commands (one per line, sent by the controls or typed in the terminal):
    RATE <hz>   LEVEL <0-4095>   EDGE R|F   MODE A|N|S

The serial port is reopened automatically if it disappears (USB unplugged,
board power-cycled), and the Reconnect button forces it. When the board
restarts it prints "f3scope ready"; the viewer then sends the previous
settings again so a reset does not lose them.

Usage:
    python scope.py --port /dev/ttyACM0 --baud 1500000
    python scope.py --text               # no GUI, just print frame stats
"""

import argparse
import os
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

# must match PRETRIGGER in main.cpp: samples shown before the trigger point
PRETRIGGER_SAMPLES = 100

RATE_PRESETS = [1_000, 2_000, 5_000, 10_000, 20_000, 50_000,
                100_000, 200_000, 500_000, 1_000_000]

TEXT_LINE_RE = re.compile(rb"[A-Za-z][ -~]{3,}")
SETTINGS_RE = re.compile(
    r"settings: rate=(\d+) Hz level=(\d+) edge=(rising|falling) mode=([ANS])"
)
# sent by the board after every frame (ADC counts, mHz, permille)
MEAS_RE = re.compile(
    r"meas: min=(\d+) max=(\d+) mean=(\d+) rms=(\d+) "
    r"periodic=([01]) freq_mhz=(\d+) duty=(\d+)"
)


def print_board_line(line):
    print(f"[board] {line}", flush=True)


def counts_to_volts(counts):
    return counts * VREF / ADC_MAX


def volts_to_counts(volts):
    return int(round(min(max(volts, 0.0), VREF) * ADC_MAX / VREF))


def format_freq(hz):
    if hz >= 1_000_000:
        return f"{hz / 1_000_000:.4f} MHz"
    if hz >= 1_000:
        return f"{hz / 1_000:.3f} kHz"
    return f"{hz:.3f} Hz"


def format_time(seconds):
    if seconds >= 1:
        return f"{seconds:.4f} s"
    if seconds >= 1e-3:
        return f"{seconds * 1e3:.4f} ms"
    return f"{seconds * 1e6:.3f} \u00b5s"


def format_rate(hz):
    if hz >= 1_000_000:
        return f"{hz / 1_000_000:g} MS/s"
    if hz >= 1_000:
        return f"{hz / 1_000:g} kS/s"
    return f"{hz} S/s"


class SerialLink:
    """Serial port that survives unplugging.

    read() never raises: while the port is missing it waits and retries, and
    returns b"" like a timeout. Only the reader thread calls read(); write()
    may be called from any thread.
    """

    RETRY_S = 1.0

    def __init__(self, port, baud):
        self.port = port
        self.baud = baud
        self.short_name = os.path.basename(port)   # by-id paths are long
        self.status = "connecting"
        self.connected = False
        self.connections = 0          # bumped on every successful open
        self._ser = None
        self._lock = threading.Lock()
        self._next_try = 0.0
        self._reconnect = threading.Event()

    def reconnect(self):
        """Ask the reader thread to close and reopen the port."""
        self._reconnect.set()

    def _close(self, status):
        with self._lock:
            if self._ser is not None:
                try:
                    self._ser.close()
                except (serial.SerialException, OSError):
                    pass
            self._ser = None
            self.connected = False
            self.status = status

    def _open(self):
        now = time.monotonic()
        if now < self._next_try:
            return
        self._next_try = now + self.RETRY_S
        try:
            ser = serial.Serial(self.port, self.baud, timeout=0.2)
            ser.reset_input_buffer()
        except (serial.SerialException, OSError, ValueError):
            self.status = f"waiting for {self.short_name}"
            return
        with self._lock:
            self._ser = ser
            self.connected = True
            self.connections += 1
            self.status = f"connected: {self.short_name}"
        print(f"[pc] connected to {self.port}", flush=True)

    def read(self, n):
        if self._reconnect.is_set():
            self._reconnect.clear()
            self._close("reconnecting")
            self._next_try = 0.0
        if self._ser is None:
            self._open()
            if self._ser is None:
                time.sleep(0.2)
                return b""
        try:
            return self._ser.read(n)
        except (serial.SerialException, OSError, TypeError, AttributeError):
            # TypeError/AttributeError: pyserial internals after a hot unplug
            print(f"[pc] lost {self.port}, waiting for it to come back", flush=True)
            self._close(f"lost {self.short_name}")
            return b""

    def write(self, data):
        with self._lock:
            if self._ser is None:
                raise serial.SerialException("not connected")
            try:
                self._ser.write(data)
            except (serial.SerialException, OSError) as e:
                raise serial.SerialException(str(e)) from e

    def close(self):
        self._close("closed")


class FrameReader:
    """Reads packets from a byte stream, resynchronising on errors.

    Bytes skipped while looking for the sync word are collected into text
    lines and passed to on_text. 'settings:' lines also update self.settings.
    """

    def __init__(self, ser, rate_hz=100_000, on_text=print_board_line):
        self.ser = ser
        self.frames_ok = 0
        self.frames_bad = 0
        self.rate_hz = rate_hz
        self.on_text = on_text
        self.settings = None          # dict from the last 'settings:' line
        self.settings_version = 0     # bumped on every 'settings:' line
        self.measurements = None      # dict from the last 'meas:' line
        self.last_message = ""
        # set when the board prints "f3scope ready" (it was reset); holds the
        # settings it had before, so the viewer can send them again
        self.restart_pending = False
        self.settings_before_restart = None
        self._text = bytearray()

    def reset_stats(self):
        self.frames_ok = 0
        self.frames_bad = 0
        self.measurements = None
        self._text.clear()

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
                self._handle_line(line.decode("ascii"))
        elif b == 0x0D:  # '\r'
            pass
        elif 0x20 <= b <= 0x7E and len(self._text) < MAX_TEXT_LINE:
            self._text.append(b)
        else:
            # binary garbage (e.g. the rest of a broken frame): drop the line
            self._text.clear()

    def _handle_line(self, line):
        m = MEAS_RE.fullmatch(line)
        if m:
            # comes with every frame: keep it, but don't flood the terminal
            self.measurements = {
                "min": int(m.group(1)),
                "max": int(m.group(2)),
                "mean": int(m.group(3)),
                "rms": int(m.group(4)),
                "periodic": m.group(5) == "1",
                "freq_hz": int(m.group(6)) / 1000.0,
                "duty": int(m.group(7)) / 10.0,
            }
            return

        if line.startswith("f3scope ready"):
            if self.settings is not None:
                self.settings_before_restart = dict(self.settings)
                self.restart_pending = True

        m = SETTINGS_RE.search(line)
        if m:
            self.rate_hz = int(m.group(1))
            self.settings = {
                "rate": int(m.group(1)),
                "level": int(m.group(2)),
                "edge": m.group(3),
                "mode": m.group(4),
            }
            self.settings_version += 1
        self.last_message = line
        if self.on_text is not None:
            self.on_text(line)

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


class Commander:
    """Sends command lines to the board. Safe to call from several threads."""

    def __init__(self, ser):
        self.ser = ser
        self._lock = threading.Lock()

    def send(self, cmd):
        cmd = cmd.strip().upper()
        if not cmd:
            return
        try:
            with self._lock:
                self.ser.write((cmd + "\n").encode("ascii"))
            print(f"[pc] {cmd}", flush=True)
        except (UnicodeEncodeError, serial.SerialException) as e:
            print(f"[pc] cannot send '{cmd}': {e}", flush=True)


def start_command_input(commander):
    """Send lines typed in this terminal to the board (e.g. 'RATE 10000')."""

    def loop():
        for line in sys.stdin:
            commander.send(line)

    threading.Thread(target=loop, daemon=True).start()


def run_text(reader, link):
    while True:
        samples = reader.read_frame()
        if samples is None:
            if link.connected:
                print(f"(no valid frame)  ok={reader.frames_ok} bad={reader.frames_bad}")
            continue
        meas = reader.measurements
        freq = ""
        if meas and meas["periodic"]:
            freq = f"  f={format_freq(meas['freq_hz'])} duty={meas['duty']:.1f}%"
        print(
            f"frame ok: count={len(samples)} min={samples.min()} max={samples.max()}"
            f"  rate={reader.rate_hz} Hz{freq}  ok={reader.frames_ok} bad={reader.frames_bad}"
        )


def run_gui(reader, commander, link):
    import pyqtgraph as pg
    from pyqtgraph.Qt import QtCore, QtWidgets

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

    threading.Thread(target=worker, daemon=True).start()

    app = pg.mkQApp("f3scope")
    root = QtWidgets.QWidget()
    root.setWindowTitle("f3scope")
    layout = QtWidgets.QHBoxLayout(root)

    # ---- plot ----------------------------------------------------------------
    plot = pg.PlotWidget()
    plot.setLabel("bottom", "time", units="s")
    plot.setLabel("left", "voltage", units="V")
    plot.setYRange(0, VREF)
    plot.showGrid(x=True, y=True, alpha=0.3)
    curve = plot.plot(pen=pg.mkPen("y", width=2))

    level_line = pg.InfiniteLine(
        pos=counts_to_volts(2048), angle=0, movable=True,
        bounds=(0, VREF), pen=pg.mkPen("c", width=1, style=QtCore.Qt.PenStyle.DashLine),
        hoverPen=pg.mkPen("c", width=3),
    )
    plot.addItem(level_line)
    # the label text is set by show_level(), so it always shows the same
    # value as the slider, whatever moved the line
    level_tag = pg.InfLineLabel(level_line, text="", position=0.93, color="c",
                                anchors=[(0.5, 1), (0.5, 1)])

    trig_marker = pg.InfiniteLine(
        pos=PRETRIGGER_SAMPLES / reader.rate_hz, angle=90, movable=False,
        pen=pg.mkPen((150, 150, 150), width=1, style=QtCore.Qt.PenStyle.DotLine),
    )
    plot.addItem(trig_marker)
    layout.addWidget(plot, stretch=1)

    # ---- controls ------------------------------------------------------------
    panel = QtWidgets.QWidget()
    panel.setFixedWidth(250)
    controls = QtWidgets.QVBoxLayout(panel)

    # sample rate
    rate_box = QtWidgets.QGroupBox("Sample rate")
    rate_layout = QtWidgets.QVBoxLayout(rate_box)
    rate_combo = QtWidgets.QComboBox()
    for hz in RATE_PRESETS:
        rate_combo.addItem(format_rate(hz), hz)
    rate_combo.setCurrentIndex(RATE_PRESETS.index(100_000))
    rate_actual = QtWidgets.QLabel("board: ?")
    rate_layout.addWidget(rate_combo)
    rate_layout.addWidget(rate_actual)
    controls.addWidget(rate_box)

    def on_rate(index):
        commander.send(f"RATE {rate_combo.itemData(index)}")

    rate_combo.activated.connect(on_rate)

    # trigger level
    level_box = QtWidgets.QGroupBox("Trigger level")
    level_layout = QtWidgets.QVBoxLayout(level_box)
    level_slider = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
    level_slider.setRange(0, ADC_MAX)
    level_slider.setValue(2048)
    level_label = QtWidgets.QLabel()
    level_layout.addWidget(level_slider)
    level_layout.addWidget(level_label)
    level_hint = QtWidgets.QLabel("or drag the dashed line on the plot")
    level_hint.setStyleSheet("color: gray")
    level_layout.addWidget(level_hint)
    controls.addWidget(level_box)

    def show_level(counts):
        volts = counts_to_volts(counts)
        level_label.setText(f"{counts}  ({volts:.3f} V)")
        level_tag.format = f"trigger {counts}  {volts:.2f} V"   # no {value}: fixed text
        level_tag.setText(level_tag.format)

    show_level(2048)

    def on_slider_moved(counts):
        show_level(counts)
        # no blockSignals here: the line's own label listens to the same
        # signal; on_line_moved only updates the slider and never sends
        level_line.setValue(counts_to_volts(counts))

    def on_slider_value(counts):
        on_slider_moved(counts)
        if not level_slider.isSliderDown():   # keyboard / click on the track
            commander.send(f"LEVEL {counts}")

    level_slider.valueChanged.connect(on_slider_value)
    level_slider.sliderReleased.connect(
        lambda: commander.send(f"LEVEL {level_slider.value()}"))

    def on_line_moved(line):
        counts = volts_to_counts(line.value())
        show_level(counts)
        level_slider.blockSignals(True)
        level_slider.setValue(counts)
        level_slider.blockSignals(False)

    level_line.sigPositionChanged.connect(on_line_moved)
    level_line.sigPositionChangeFinished.connect(
        lambda line: commander.send(f"LEVEL {volts_to_counts(line.value())}"))

    # edge
    edge_box = QtWidgets.QGroupBox("Edge")
    edge_layout = QtWidgets.QHBoxLayout(edge_box)
    edge_rising = QtWidgets.QRadioButton("Rising")
    edge_falling = QtWidgets.QRadioButton("Falling")
    edge_rising.setChecked(True)
    edge_group = QtWidgets.QButtonGroup(edge_box)
    edge_group.addButton(edge_rising)
    edge_group.addButton(edge_falling)
    edge_layout.addWidget(edge_rising)
    edge_layout.addWidget(edge_falling)
    controls.addWidget(edge_box)

    edge_rising.clicked.connect(lambda: commander.send("EDGE R"))
    edge_falling.clicked.connect(lambda: commander.send("EDGE F"))

    # mode
    mode_box = QtWidgets.QGroupBox("Mode")
    mode_layout = QtWidgets.QHBoxLayout(mode_box)
    mode_buttons = {}
    mode_group = QtWidgets.QButtonGroup(mode_box)
    for code, name in (("A", "Auto"), ("N", "Normal"), ("S", "Single")):
        btn = QtWidgets.QPushButton(name)
        btn.setCheckable(True)
        mode_group.addButton(btn)
        mode_layout.addWidget(btn)
        mode_buttons[code] = btn
        # Single is sent on every click: each click captures one more frame
        btn.clicked.connect(lambda _checked, c=code: commander.send(f"MODE {c}"))
    mode_buttons["A"].setChecked(True)
    single_hint = QtWidgets.QLabel("click Single again for the next frame")
    single_hint.setStyleSheet("color: gray")
    single_hint.setWordWrap(True)
    controls.addWidget(mode_box)
    controls.addWidget(single_hint)

    # measurements (computed on the board over the whole capture)
    meas_box = QtWidgets.QGroupBox("Measurements")
    meas_grid = QtWidgets.QGridLayout(meas_box)
    meas_labels = {}
    for row, (key, name) in enumerate((
            ("freq", "Freq"), ("period", "Period"), ("duty", "Duty"),
            ("vpp", "Vpp"), ("vmin", "Vmin"), ("vmax", "Vmax"),
            ("vavg", "Vavg"), ("vrms", "Vrms"))):
        meas_grid.addWidget(QtWidgets.QLabel(name), row, 0)
        value = QtWidgets.QLabel("\u2014")
        value.setAlignment(QtCore.Qt.AlignmentFlag.AlignRight)
        value.setStyleSheet("font-family: monospace")
        meas_grid.addWidget(value, row, 1)
        meas_labels[key] = value
    controls.addWidget(meas_box)

    def show_measurements():
        m = reader.measurements
        if m is None:
            return
        v = counts_to_volts
        meas_labels["vmin"].setText(f"{v(m['min']):.3f} V")
        meas_labels["vmax"].setText(f"{v(m['max']):.3f} V")
        meas_labels["vpp"].setText(f"{v(m['max'] - m['min']):.3f} V")
        meas_labels["vavg"].setText(f"{v(m['mean']):.3f} V")
        meas_labels["vrms"].setText(f"{v(m['rms']):.3f} V")
        if m["periodic"] and m["freq_hz"] > 0:
            meas_labels["freq"].setText(format_freq(m["freq_hz"]))
            meas_labels["period"].setText(format_time(1.0 / m["freq_hz"]))
            meas_labels["duty"].setText(f"{m['duty']:.1f} %")
        else:
            for key in ("freq", "period", "duty"):
                meas_labels[key].setText("\u2014")

    # connection
    conn_box = QtWidgets.QGroupBox("Connection")
    conn_layout = QtWidgets.QVBoxLayout(conn_box)
    conn_label = QtWidgets.QLabel()
    conn_label.setWordWrap(True)
    conn_buttons = QtWidgets.QHBoxLayout()
    reconnect_btn = QtWidgets.QPushButton("Reconnect")
    reconnect_btn.setToolTip("Close and reopen the serial port, clear counters (Ctrl+R)")
    clear_btn = QtWidgets.QPushButton("Clear")
    clear_btn.setToolTip("Clear the plot, measurements and frame counters")
    conn_buttons.addWidget(reconnect_btn)
    conn_buttons.addWidget(clear_btn)
    restore_box = QtWidgets.QCheckBox("Restore settings after board reset")
    restore_box.setChecked(True)
    conn_layout.addWidget(conn_label)
    conn_layout.addLayout(conn_buttons)
    conn_layout.addWidget(restore_box)
    controls.addWidget(conn_box)

    # status
    controls.addStretch(1)
    status = QtWidgets.QLabel()
    status.setWordWrap(True)
    message = QtWidgets.QLabel()
    message.setWordWrap(True)
    message.setStyleSheet("color: gray")
    controls.addWidget(status)
    controls.addWidget(message)

    layout.addWidget(panel)
    root.resize(1200, 550)
    root.show()

    # ---- apply what the board reports ---------------------------------------
    seen = {"version": 0}

    def sync_from_board():
        if reader.settings_version == seen["version"] or reader.settings is None:
            return
        seen["version"] = reader.settings_version
        s = dict(reader.settings)

        rate_actual.setText(f"board: {s['rate']:,} Hz".replace(",", " "))
        # a rate typed in the terminal may not be a preset: show it as blank
        rate_combo.setCurrentIndex(
            RATE_PRESETS.index(s["rate"]) if s["rate"] in RATE_PRESETS else -1)

        # don't fight the user while a drag is in progress
        if not level_slider.isSliderDown() and not level_line.moving:
            level_slider.blockSignals(True)
            level_slider.setValue(s["level"])
            level_slider.blockSignals(False)
            level_line.setValue(counts_to_volts(s["level"]))
            show_level(s["level"])

        (edge_rising if s["edge"] == "rising" else edge_falling).setChecked(True)
        mode_buttons[s["mode"]].setChecked(True)

    # ---- connection ------------------------------------------------------------
    def clear_view():
        reader.reset_stats()
        with lock:
            latest["samples"] = None
        curve.setData([], [])
        plot.setTitle("")
        for value in meas_labels.values():
            value.setText("\u2014")

    def on_reconnect():
        print("[pc] reconnecting...", flush=True)
        clear_view()
        link.reconnect()

    reconnect_btn.clicked.connect(on_reconnect)
    clear_btn.clicked.connect(clear_view)
    shortcut = pg.QtGui.QShortcut(pg.QtGui.QKeySequence("Ctrl+R"), root)
    shortcut.activated.connect(on_reconnect)

    def show_connection():
        ok = link.connected
        conn_label.setText(link.status)
        conn_label.setStyleSheet("color: #2a2" if ok else "color: #d33; font-weight: bold")

    def restore_after_restart():
        if not reader.restart_pending:
            return
        reader.restart_pending = False
        old = reader.settings_before_restart
        if not restore_box.isChecked() or old is None:
            return
        print("[pc] board was reset, sending previous settings", flush=True)
        commander.send(f"RATE {old['rate']}")
        commander.send(f"LEVEL {old['level']}")
        commander.send("EDGE " + ("R" if old["edge"] == "rising" else "F"))
        if old["mode"] != "S":          # don't capture a Single frame by itself
            commander.send(f"MODE {old['mode']}")

    # ---- redraw --------------------------------------------------------------
    fps = {"t": time.monotonic(), "n": 0, "value": 0.0}

    def update():
        show_connection()
        restore_after_restart()
        sync_from_board()
        show_measurements()
        message.setText(reader.last_message)

        with lock:
            samples = latest["samples"]
            rate_hz = latest["rate"]
            latest["samples"] = None

        now = time.monotonic()
        if now - fps["t"] >= 1.0:
            fps["value"] = fps["n"] / (now - fps["t"])
            fps["t"], fps["n"] = now, 0
        status.setText(
            f"{fps['value']:.1f} fps    ok {reader.frames_ok}    bad {reader.frames_bad}"
        )

        if samples is None:
            return
        fps["n"] += 1
        t = np.arange(len(samples)) / rate_hz
        v = counts_to_volts(samples.astype(np.float32))
        curve.setData(t, v)
        trig_marker.setValue(PRETRIGGER_SAMPLES / rate_hz)
        plot.setTitle(
            f"{format_rate(rate_hz)}    min {v.min():.3f} V    max {v.max():.3f} V    "
            f"Vpp {v.max() - v.min():.3f} V"
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
    p.add_argument("--baud", type=int, default=1_500_000)
    p.add_argument("--rate", type=int, default=100_000,
                   help="starting sample rate in Hz (updated by 'settings:' lines from the board)")
    p.add_argument("--text", action="store_true", help="print stats instead of plotting")
    args = p.parse_args()

    # no exit if the port is missing: the link waits for the board to appear
    link = SerialLink(args.port, args.baud)
    reader = FrameReader(link, rate_hz=args.rate)
    commander = Commander(link)
    start_command_input(commander)
    print("commands: RATE <hz> | LEVEL <0-4095> | EDGE R|F | MODE A|N|S", flush=True)

    try:
        if args.text:
            run_text(reader, link)
        else:
            run_gui(reader, commander, link)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()


if __name__ == "__main__":
    main()
