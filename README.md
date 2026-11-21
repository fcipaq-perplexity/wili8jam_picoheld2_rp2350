# wili8jam

A PICO-8-compatible fantasy console for the **Pico Held 2** handheld (RP2350B edition).
Port of wili8jam (originally for the Adafruit Fruit Jam).

Runs PICO-8 `.p8` and `.p8.png` cartridges from an SD card on the Pico Held 2's 720x720 LCD with I2S audio and the built-in buttons. A Lua REPL is available over USB serial.

## Features

- **PICO-8 cartridge runner** -- loads `.p8` and `.p8.png` files with full game loop (`_init`/`_update`/`_draw`)
- **100+ PICO-8 API functions** -- graphics, audio, input, math, strings, tables, memory, coroutines, cartdata
- **8-channel audio** -- 4-channel SFX engine (8 waveforms, 7 effects) + music pattern sequencer + 4-channel basic synth
- **PICO-8 syntax preprocessor** -- `!=`, `+=`, short-form `if`/`while`, `?print`, `//` comments, P8SCII glyphs, `0b` literals
- **128x128 screen on the 720x720 LCD** -- two scaling modes: *crop* (2x, outer 4 pixels on each side cut off) and *fit* (1.875x, whole screen)
- **Cart picker** -- browse folders on the SD card and launch carts with the buttons
- **Lua 5.4 REPL** -- over USB CDC serial (also used for debug output)
- **Memory layout tuned for speed** -- PICO-8 memory (sprites, map, screen) in internal SRAM; small Lua objects in a 128 KB SRAM pool, the rest of the Lua heap in 8 MB PSRAM; hot code in RAM; screen scaling on core 1

## Hardware

| | |
|-|-|
| **Board** | Pico Held 2, RP2350B edition (board definition: `boards/picoheld_2.h`) |
| **CPU** | Dual ARM Cortex-M33 @ 300 MHz (1.20 V) |
| **RAM** | 520 KB SRAM + 8 MB PSRAM (APS6404L on GPIO 0, 84 MHz) |
| **Flash** | 16 MB |
| **Storage** | microSD (FAT32), SPI0 |
| **Display** | 720x720 RGB DPI panel via PIO/DMA (Pico Engine driver), 240x240 8-bit CLUT framebuffer, 3x scanout |
| **Audio** | I2S DAC, 22.05 kHz |
| **Input** | D-pad, analog stick, A/B/X/Y, L/R, START/SELECT |
| **Serial** | USB-C (CDC terminal) |

The hardware drivers in `picoengine/` are the Pico Engine library (LCD, buttons, power, backlight). They are compiled directly from that tree by the top-level `CMakeLists.txt`; `src/held_hw.cpp` is the C glue between them and the emulator.

## Controls

| Button | PICO-8 / function |
|-|-|
| D-pad / analog stick | ⬅️ ➡️ ⬆️ ⬇️ (don't touch the stick while switching on: its centre is calibrated at boot) |
| A | 🅾️ (O) |
| B | ❎ (X) |
| START | pause/menu button (`btn(6)`) |
| SELECT + START | exit the cart, back to the cart picker (also leaves the REPL) |
| SELECT + L | toggle screen scaling: crop (2x) / fit (1.875x) |
| SELECT + UP / DOWN | volume |
| SELECT + LEFT / RIGHT | backlight |
| SELECT + X | sampling profiler on/off (output on USB serial in the game loop) |
| hold R (2 s) | power off |
| SELECT while powering on | USB bootloader (hard-wired to BOOTSEL) |

While SELECT is held the cart sees no buttons.

## Building

### Prerequisites

- [Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk)
- ARM GCC toolchain (14.2.1 recommended)
- CMake 3.13+

### Build

```bash
mkdir -p build && cd build
cmake .. -G "Unix Makefiles"
make -j8
```

Options:

- `-DPICOHELD2_HW_VARIANT=0` -- Pico Held 2 v1.5, SunmaxIC panel (default)
- `-DPICOHELD2_HW_VARIANT=1` -- Pico Held 2 v1.4, Buydisplay panel
- `-DHELD_HOT_CODE_IN_RAM=ON` -- link the Lua VM core, the allocator and the PICO-8 drawing code into RAM instead of running them from flash through the XIP cache (default ON, ~100 KB RAM)
- `-DHELD_SYS_CLOCK_KHZ=300000` -- system clock (default 300 MHz at 1.20 V, `HELD_VREG_VOLTAGE` in `src/held_config.h`). The LCD scanout is DMA bound, so the panel refresh scales with it (about 44 Hz at 252 MHz). In the game loop frames are dropped rather than slowing a cart down.

Outputs:

- `build/wili8jam.uf2` -- the console
- `build/picoheld2_hwtest.uf2` -- hardware bring-up test: colour bars, crop/fit frame markers, orientation marker, live button display with raw ADC values (D-pad axes, L, stick), A/B play test sounds

Tuning knobs live in `src/held_config.h` (all overridable with `target_compile_definitions`): D-pad ADC thresholds (`HELD_DPAD_LEFT_MAX`, `HELD_DPAD_RIGHT_MIN`, `HELD_DPAD_UP_MAX`, `HELD_DPAD_DOWN_MIN`), the SELECT glitch filter (`HELD_SEL_CONSECUTIVE`), analog stick (`HELD_USE_ANALOG_STICK`, `HELD_STICK_THRESHOLD_PCT`, dead zone, inversion; centre calibrated at boot, min/max tracked at runtime), `HELD_LCD_NUM_FB` (1 = single buffer written right after vsync in scan order, default; 2 = double buffering), `HELD_BACKLIGHT_DEFAULT`. Also `PSRAM_MAX_FREQ_HZ` in `src/psram.h` (default 84 MHz; 63000000 if PSRAM is flaky).

The D-pad thresholds can be tried out live from the REPL: `input.dpad()` prints the current values and the raw ADC readings, `input.dpad(1150, 2250, 1600, 2750)` sets left max, right min, up max, down min.

### Flash

Hold SELECT while switching the Pico Held 2 on, then copy the `.uf2` to the mounted drive.

### Tests

The PICO-8 preprocessor has a host-side test suite that runs natively:

```bash
gcc -o test_preprocess src/test_p8_preprocess.c src/p8_preprocess.c tlsf/tlsf.c -Isrc -I. -lm -DTEST_HOST
./test_preprocess
```

Check a `.p8.png` cart on the host (same PNG/PXA/preprocessor code as the firmware):

```bash
gcc -o test_p8png test_p8png.c src/p8_png.c src/p8_preprocess.c tlsf/tlsf.c \
    $(ls lua-5.4.7/src/*.c | grep -v '/lua.c\|/luac.c') -Isrc -I. -Ilua-5.4.7/src -DLUA_32BITS=1 -lm
./test_p8png mycart.p8.png
```

## Usage

### Running Cartridges

Put `.p8` or `.p8.png` files anywhere on the SD card (folders are supported). After boot the cart picker shows the SD card: UP/DOWN select, LEFT/RIGHT page, A runs a cart or opens a folder, B goes up one folder.

- **Auto-run:** a file named `main.p8` (or `main.p8.png`) in the root runs once at boot
- **Exit a cart:** SELECT + START. Runtime errors are shown on screen.

### Serial REPL

Connect to the USB-C serial port (115200 baud) and press Enter while the cart picker is shown. You get a Lua 5.4 REPL with PICO-8 API functions available as globals; the console is mirrored on the LCD. SELECT + START on the device returns to the cart picker.

```
> print("hello")
hello
> circfill(64,64,20,8)
> flip()
```

Built-in commands: `ls`, `cd`, `load`, `run`, `resume`, `cls`, `help`, `info`, `reboot`, `input.debug()`, `input.dpad()`, `input.stick()` (`input.stick("cal")` re-centres)

## Source Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, init sequence, picker/REPL main loop, Lua library registration |
| `src/held_hw.cpp/h` | Pico Held 2 glue: power latch, buttons, battery, LCD scaling (128x128 4bpp -> 240x240 CLUT) |
| `src/gfx.c/h` | 128x128 4-bit indexed framebuffer, drawing primitives, font rendering |
| `src/audio.c/h` | I2S PIO + DMA output, software volume, 4-channel basic synth |
| `src/audio_i2s.pio` | PIO I2S transmitter program (LRCLK on the lower pin) |
| `src/input.c/h` | Button mapping, btnp auto-repeat, system button combos |
| `src/sdcard.c/h` | SD card SPI driver |
| `src/psram.h` | PSRAM initialization via QMI hardware registers |
| `src/p8_api.c/h` | Full PICO-8 API: 100+ Lua globals |
| `src/p8_cart.c/h` | .p8 file parser, game loop runner, cart picker UI |
| `src/p8_png.c/h` | .p8.png loader: PNG decoder, DEFLATE decompressor, steganography, PXA |
| `src/p8_preprocess.c/h` | PICO-8 syntax preprocessor |
| `src/p8_sfx.c/h` | SFX engine + music pattern sequencer |
| `src/p8_console.c/h` | On-screen text console |
| `src/hwtest.c` | Hardware bring-up test firmware |
| `src/test_p8_preprocess.c` | Host-side preprocessor tests |
| `boards/picoheld_2.h` | Reusable Pico SDK board definition |

### Vendored Libraries

| Directory | Source |
|-----------|--------|
| `lua-5.4.7/src/` | [Lua 5.4.7](https://www.lua.org/) (LUA_32BITS=1, patched luaconf.h) |
| `tlsf/` | [TLSF v3.1](http://www.gii.upv.es/tlsf/) allocator for PSRAM |
| `fatfs/` | [FatFS R0.15](http://elm-chan.org/fsw/ff/) + custom diskio with PSRAM bounce buffer |
| `picoengine/` | Pico Engine -- Pico Held 2 hardware library (LCD, controls, power) |

## Acknowledgments

- [Lua](https://www.lua.org/) by PUC-Rio
- [PICO-8](https://www.lexaloffle.com/pico-8.php) by Lexaloffle Games
- [yocto-8](https://github.com/yocto-8/yocto-8) -- P8SCII font data (MIT license)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)

PICO-8 is a trademark of Lexaloffle Games. This project is not affiliated with or endorsed by Lexaloffle.
