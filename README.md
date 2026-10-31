# f3scope

![CI](https://github.com/<user>/<repo>/actions/workflows/ci.yml/badge.svg)

A small USB oscilloscope built on an **STM32F303RE** (NUCLEO-F303RE). The MCU samples a signal with its 12-bit ADC (timer-triggered, DMA to RAM), finds a trigger point, measures the signal, and streams frames to a PC where they are plotted live. Sample rate, trigger level, edge and mode are set from the PC.

Firmware is written in C++17 on top of the STM32 HAL and builds with CMake and `arm-none-eabi-gcc`. No IDE or CubeMX-generated code. The hardware-independent logic is unit-tested on the host, and CI builds the firmware and runs the tests on every push.

![f3scope live view](docs/scope.png)

## Features

- **Hardware-paced sampling, 1 kS/s to 1 MS/s:** TIM6 TRGO starts every ADC conversion, and DMA moves the result to RAM. The CPU is not involved during capture, and the sample interval is exact. The timer prescaler and period are computed for each rate, and the board reports the rate it actually achieved (72 MHz is not divisible by every rate).
- **Trigger** with hysteresis: rising or falling edge, adjustable level, 10 % pre-trigger, **Auto / Normal / Single** modes.
- **Measurements on the board:** frequency, period, duty cycle, Vpp, Vmin, Vmax, Vavg, Vrms. Frequency is averaged over all periods in the capture, measured at 50 % of the signal (independent of the trigger level).
- **Control from the PC:** text commands over the same serial link (`RATE`, `LEVEL`, `EDGE`, `MODE`). The board validates every value and answers with its current settings.
- **Binary frame protocol** over the ST-LINK virtual COM port at 1.5 Mbaud, with sync bytes and a checksum. Text messages from the board travel between frames on the same link.
- **UART receive via circular DMA:** no interrupt per byte, so commands are not lost at 1.5 Mbaud.
- **Input buffer on the on-chip OPAMP2** (follower), for sources that cannot drive the ADC directly.
- **Built-in test signal:** TIM7 + DMA (circular) drive DAC1 with a sine table: a 1 kHz sine on PA4, so the scope can measure itself.
- **Python live viewer** (`pyqtgraph`): plot with time and voltage axes, controls for every setting, draggable trigger line, measurements panel, automatic reconnect.
- **Layered firmware:** board support, peripheral drivers and hardware-independent application logic are kept in separate layers.
- **82 unit tests** for the application layer, run on the host with AddressSanitizer and UBSan, and in CI.

## Signal chain

```
                  ┌──────── STM32F303RE ───────────────────────────────────────────────┐
 signal ── PA0 ──►│ ADC1 ──► DMA1 Ch1 ──► capture buffer ──► trigger ──► frame (1000)  │──► USART2 TX ──► ST-LINK VCP ──► PC
 (0…3.3 V)        │  ▲                    (2000 samples)  └─► measure ──► "meas:" line │      1.5 Mbaud     (/dev/ttyACM*)   (scope.py)
                  │  └── TIM6 TRGO (sample clock, 1 kS/s … 1 MS/s)                     │
                  │                                                                    │
                  │ commands ◄── parser ◄── ring buffer ◄── DMA1 Ch6 (circular) ◄──────│◄── USART2 RX ◄── PC
                  │                                                                    │
                  │ sine table ──► DMA2 Ch3 (circular) ──► DAC1 ──► PA4 (test out)     │
                  │                          ▲                                         │
                  │                          └── TIM7 TRGO                             │
                  │                                                                    │
 buffered ─ PA7 ─►│ OPAMP2 (follower) ──► PA6 ───────────── jumper to PA0 ──────────── │
 input            └────────────────────────────────────────────────────────────────────┘
```

## Hardware

| Function | Pin | Nucleo header |
|---|---|---|
| Scope input (ADC1_IN1) | PA0 | A0 (CN8) |
| Buffered input (OPAMP2 VINP) | PA7 | D11 (CN5) |
| OPAMP2 output | PA6 | D12 (CN5) |
| Test signal output (DAC1_OUT1) | PA4 | A2 (CN8) |
| UART TX / RX (USART2 → ST-LINK VCP) | PA2 / PA3 | via on-board ST-LINK |
| Status LED (LD2, toggles every frame) | PA5 | on board |

**Two ways to connect a signal:**

- **Direct:** signal → **A0**. To see the built-in test signal, connect **A2 → A0**.
- **Through the buffer:** signal → **D11**, and a jumper **D12 → A0**. The OPAMP is always running, so the same firmware works for both.

> **Input range is 0…3.3 V only.** Never apply negative voltages or more than 3.3 V to A0 or D11. A 1 kΩ series resistor is recommended for external sources, and the source ground must be connected to the Nucleo GND.

### Audio input (signals around 0 V)

A sound card output swings around 0 V, so it is AC-coupled and shifted to mid-scale (1.65 V) first:

```
                                3V3 (CN6)
                                  │
                                 R1 10k
                                  │
 jack TIP ─────── C1 1 µF ────────●──────── R3 1k ────► A0  (or D11 via the buffer)
                                  │
                                 R2 10k
                                  │
 jack SLEEVE ────────────────────GND
```

- C1 blocks DC in both directions; the lower cut-off is 1 / (2π · 5 kΩ · 1 µF) ≈ 32 Hz.
- R1/R2 bias the node to half of the same 3.3 V that feeds the ADC reference, so the bias tracks the scale (ratiometric).
- With an electrolytic C1, the + lead goes to the bias node.
- Use a 3-pole (TRS) cable; on 4-pole headset plugs the ground is not on the sleeve.

## Firmware architecture

```
src/
├── main.cpp            wires everything together: init, command → capture → trigger → measure → send loop
├── bsp/                board support: clocks (72 MHz from HSE bypass + PLL), pins, SysTick, syscalls
├── drivers/            C++ wrappers around the HAL
│   ├── uart            TX blocking, RX via circular DMA
│   ├── adc             ADC1 + DMA, triggered by TIM6
│   ├── dac             DAC1 + DMA, triggered by TIM7
│   ├── tim             BasicTimer (TIM6/TIM7), set_rate() returns the actual rate
│   └── opamp           OPAMP2 follower, self-calibrated at start-up
└── app/                hardware-independent logic, no HAL includes, unit-tested on the host
    ├── trigger         edge detection with hysteresis
    ├── commands        line parser for PC commands, validated settings
    ├── measure         min/max/mean/RMS, frequency, duty cycle
    ├── timer_math      timer prescaler/period for a wanted rate
    ├── protocol        frame header / footer / checksum
    └── wavegen         sine table generation
```

Each layer only depends on the layers below it. `app/` compiles on a PC with the normal compiler. The timer math was moved out of the timer driver for exactly this reason: the driver only writes registers, the calculation is tested.

## Building and flashing

Requirements (Arch Linux):

```sh
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-gdb openocd cmake
```

Clone with submodules (CMSIS and the STM32F3 HAL):

```sh
git clone --recurse-submodules <repo-url> f3scope
cd f3scope
```

Build and flash:

```sh
cmake -B build
cmake --build build
cmake --build build --target flash     # OpenOCD + on-board ST-LINK
cmake --build build --target format    # clang-format for src/
```

Debugging: run `openocd -f board/st_nucleo_f3.cfg` in one terminal, then `arm-none-eabi-gdb build/firmware.elf` with `target extended-remote :3333`.

## Tests

The `app/` layer is tested with GoogleTest on the host. The tests are a separate CMake project, built with the PC compiler (GoogleTest is downloaded on the first configure):

```sh
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

| Module | Tests | What is covered |
|---|---|---|
| `trigger` | 14 | both edges, arming, inclusive thresholds, noise inside the hysteresis, flat and empty input, a real sine, ends of the ADC range |
| `commands` | 39 | every command, limits, 17 invalid values that must leave the settings unchanged, `\r\n`, line length, recovery after errors |
| `timer_math` | 12 | known PSC/ARR values, the 65 536-tick boundary, invalid rates, the whole 1 kS/s…1 MS/s range |
| `measure` | 17 | sine and square waves, frequency averaged over periods, duty cycle, noise, offset independence, overflow of the sums |

Debug builds run with AddressSanitizer, UndefinedBehaviorSanitizer and `_GLIBCXX_ASSERTIONS`, so an out-of-range `string_view[0]` or a buffer overrun fails the test instead of passing by luck.

**CI** (`.github/workflows/ci.yml`) runs on every push: host tests, firmware build (Debug and Release, the `.elf/.hex/.bin` are kept as artifacts), and a clang-format check.

## PC viewer

```sh
python -m venv host/.venv
source host/.venv/bin/activate
pip install pyserial numpy pyqtgraph PyQt6

python host/scope.py --port /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_*
python host/scope.py --text      # no GUI, print frame statistics only
```

Options: `--port`, `--baud` (default 1 500 000, must match the firmware), `--rate` (starting sample rate; afterwards it comes from the board).

The window has controls for sample rate, trigger level (slider or drag the dashed line on the plot), edge and mode, plus the measurements panel. The controls always show what the **board** reports, so a rejected command does not leave them in a wrong state. Commands can also be typed in the terminal.

The viewer reconnects by itself when the port disappears (USB unplugged, board power-cycled). **Reconnect** (Ctrl+R) forces it, **Clear** resets the plot and counters. When the board is reset it prints `f3scope ready`, and the viewer sends the previous settings again.

## Protocol

### Frames (board → PC)

All multi-byte fields are little-endian.

| Field | Size | Value |
|---|---|---|
| sync | 2 bytes | `0xAA 0x55` |
| count | `uint16` | number of samples |
| samples | `count × uint16` | raw 12-bit ADC values (0…4095) |
| checksum | `uint16` | sum of all samples, modulo 65536 |

The receiver scans for the sync bytes, validates `count`, and drops any frame whose checksum does not match.

### Text lines (board → PC)

ASCII lines ending in `\n`, sent only between frames. ASCII never contains `0xAA`, so text can never be mistaken for a frame.

```
f3scope ready
settings: rate=100000 Hz level=2048 edge=rising mode=A
meas: min=348 max=3748 mean=2048 rms=2375 periodic=1 freq_mhz=1000000 duty=500
cmd error: bad value
single: captured
```

`meas:` uses integers only (ADC counts, millihertz, permille): the PC converts to volts, and the firmware needs no floating-point `printf`.

### Commands (PC → board)

One per line, upper case, ending in `\n` (`\r\n` also works).

| Command | Argument | Range |
|---|---|---|
| `RATE` | sample rate, Hz | 1000 … 1 000 000 (the board reports the actual rate) |
| `LEVEL` | trigger level, ADC counts | 0 … 4095 |
| `EDGE` | `R` / `F` | rising / falling |
| `MODE` | `A` / `N` / `S` | Auto / Normal / Single (each `MODE S` captures one more frame) |

Invalid lines are answered with `cmd error: …` and change nothing.

## Measured results

Built-in DAC sine looped back to the ADC (A2 → A0), 100 kS/s:

| Quantity | Expected | Measured |
|---|---|---|
| DAC = 2048 (≈1.65 V), DC level | 2048 | 2050–2052 typical (offset ≈ +3 LSB ≈ 2.4 mV) |
| Sine 2048 ± 1700: min / max | 348 / 3748 | ≈ 341 / ≈ 3743 |
| Frequency | 1.000 kHz | 1.000 kHz |
| Period | 1.000 ms | 1.000 ms |
| Duty cycle | 50 % | 50 % |
| Vpp | 2.74 V | ≈ 2.7 V |
| Vavg | 1.65 V | ≈ 1.6 V |
| Vrms (including DC) | 1.91 V | ≈ 1.9 V |

Vmin and Vmax are single extreme samples, so noise moves them by a few tens of millivolts; frequency, duty, Vavg and Vrms are averaged over the whole capture and are stable.

External signals tested: a 1 kHz PWM from a Raspberry Pi Pico (duty swept 5…95 %; the trigger holds the edge, the duty measurement follows), and a laptop audio output through the AC-coupled input above.

## Engineering notes

Problems found and fixed along the way:

- **HAL weak stubs linked instead of real functions.** `stm32f3xx_hal_dac.c` defines `__weak` placeholder versions of `HAL_DAC_Start` (returns `HAL_ERROR`) and `HAL_DAC_ConfigChannel` (does nothing). When the HAL is built as a *static* library, the linker resolves those symbols from the first object that defines them and never pulls in `stm32f3xx_hal_dac_ex.c` with the real implementations. I found this by stepping through `dac::init()` in gdb. Building the HAL as a CMake **OBJECT** library fixes it.
- **UART TX on the wrong pin.** Found by reading `GPIOA->MODER` / `AFR[0]` in gdb. PA2 was still an input, and the alternate function had been applied to PA5 (the LED pin).
- **Commands lost at 1.5 Mbaud.** With the HAL receive interrupt, a byte arrives every 6.7 µs, and the HAL IRQ handler plus callback did not always finish before the next one: overrun, lost bytes, `unknown command`. Confirmed by dropping to 115200 baud, where every command arrived. Fixed by receiving into a ring buffer with **circular DMA**; the write position comes from the DMA counter (`CNDTR`), so there is no interrupt at all.
- **Falling-edge trigger never fired.** In the falling branch the crossing set `armed = true` again instead of returning the index, so Auto mode always sent the frame from index 0 and the picture drifted. Rising was fine, which is why it went unnoticed until `EDGE F` existed. Now covered by tests that fail with the old code.
- **`EDGE R` after `EDGE F` stayed falling.** The parser cast the character to the enum (`static_cast<Edge>('R')` is 82, not `Rising`). Fixed with an explicit `switch`; a regression test checks the round trip.
- **Vpp of 52 V.** The measurement got `sizeof(frame)` (bytes, 4000) instead of the number of samples (2000) and read past the buffer; 52.8 V is 65535 counts, a value the 12-bit ADC cannot produce. A classic, and the reason the tests run with AddressSanitizer.
- **Timer math in the driver could not be tested.** The PSC/ARR calculation now lives in `app/timer_math` and is checked against hand-calculated values and over the whole rate range, including the off-by-one boundary at 65 536 ticks.

## Roadmap

- [x] Timer → ADC → DMA capture
- [x] DAC test signal (TIM7 + DMA circular)
- [x] Binary streaming + live PC plot
- [x] Software trigger (rising/falling, hysteresis, pre-trigger)
- [x] Commands from PC: sample rate, trigger level/edge, Auto/Normal/Single
- [x] UART RX via circular DMA
- [x] Measurements: frequency, period, duty cycle, Vpp, Vavg, Vrms
- [x] Unit tests for `app/` on the host + GitHub Actions CI
- [x] OPAMP2 input buffer (follower)

## License

MIT, for the code in `src/`, `host/`, `tests/`, `cmake/` and `linker/`. Code in `third_party/` (ARM CMSIS, ST STM32F3 HAL and device headers) keeps its original licenses.
