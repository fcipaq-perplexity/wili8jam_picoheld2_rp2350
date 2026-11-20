# Board definitions

`picoheld_2.h` — Pico SDK (2.x) board header for the Pico Held 2 (RP2350B, 16 MB flash, 8 MB PSRAM).

To use it in another project, copy this folder (or reference it) and add to your top-level
`CMakeLists.txt` **before** `include(pico_sdk_import.cmake)`:

```cmake
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
set(PICO_BOARD picoheld_2 CACHE STRING "Board type")
```

Hardware variant (both use the same pins, only the LCD panel init differs):

| `PICOHELD2_HW_VARIANT` | Board |
|---|---|
| 0 (default) | Pico Held 2 v1.5, SunmaxIC panel |
| 1 | Pico Held 2 v1.4, Buydisplay panel |

Pass it as a compile definition (e.g. `-DPICOHELD2_HW_VARIANT=1`). The Pico Engine's
`hwcfg.h` uses the matching `PE_CONFIG_NO`.

Notes:
- SELECT is hard-wired to BOOTSEL: hold SELECT while powering on to enter the USB bootloader.
- `PICOHELD2_SD_DET_PIN` (GPIO 31) must stay a floating input (high = no card, low = card in).
- The debug UART pins overlap the SD card; there is no default UART. Use USB CDC stdio.
- GPIO 28 (`PICOHELD2_PWR_EN_PIN`) must be driven high early in boot, or the device powers off
  when the power button is released.
