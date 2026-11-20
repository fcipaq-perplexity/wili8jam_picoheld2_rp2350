/*
 * Board definition for the Pico Held 2 handheld (RP2350B edition)
 *
 * Usage in a project's CMakeLists.txt (before pico_sdk_init / project()):
 *
 *   list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
 *   set(PICO_BOARD picoheld_2)
 *
 * Hardware variants (select with -DPICOHELD2_HW_VARIANT=<n>):
 *   0 = Pico Held 2 v1.5, SunmaxIC 720x720 DPI panel (default)
 *   1 = Pico Held 2 v1.4, Buydisplay 720x720 DPI panel
 * Pin assignments are identical for both variants; only the panel init differs.
 *
 * NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
 *       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
 */

// This header may be included by other board headers as "boards/picoheld_2.h"

pico_board_cmake_set(PICO_PLATFORM, rp2350)

#ifndef _BOARDS_PICOHELD_2_H
#define _BOARDS_PICOHELD_2_H

// For board detection
#define PICOHELD_2

#ifndef PICOHELD2_HW_VARIANT
#define PICOHELD2_HW_VARIANT 0
#endif

// --- RP2350 VARIANT ---
// RP2350B (48 GPIOs)
#define PICO_RP2350A 0

// --- BUTTONS ---
// SELECT is hard-wired to BOOTSEL (hold SELECT while powering on -> USB boot)
#define PICOHELD2_BTN_A_PIN        25
#define PICOHELD2_BTN_B_PIN        24
#define PICOHELD2_BTN_X_PIN        26
#define PICOHELD2_BTN_Y_PIN        22
#define PICOHELD2_BTN_L_PIN        42   // analog (ADC2)
#define PICOHELD2_BTN_R_PIN        27
#define PICOHELD2_BTN_START_PIN    29   // active high, pull-down (RP2350-E9!)
#define PICOHELD2_BTN_SELECT_PIN   23
#define PICOHELD2_BTN_STICK_PIN    46   // analog (ADC6)
#define PICOHELD2_DPAD_LR_PIN      43   // analog (ADC3)
#define PICOHELD2_DPAD_UD_PIN      44   // analog (ADC4)
#define PICOHELD2_ANALOG_X_PIN     45   // analog (ADC5)
#define PICOHELD2_ANALOG_Y_PIN     47   // analog (ADC7)

// --- POWER MANAGEMENT ---
#define PICOHELD2_BAT_ADC_PIN      41   // battery voltage / 2 (ADC1)
#define PICOHELD2_PWR_EN_PIN       28   // power latch, drive high to stay on
#define PICOHELD2_CHG_STAT_PIN     30

// --- SOUND (I2S DAC) ---
#define PICOHELD2_SND_EN_PIN       40
#define PICOHELD2_I2S_LRCLK_PIN    37
#define PICOHELD2_I2S_BCLK_PIN     38
#define PICOHELD2_I2S_DATA_PIN     39

// --- LCD (RGB565 DPI, 720x720) ---
#define PICOHELD2_LCD_BL_PWM_PIN    1
#define PICOHELD2_LCD_RST_PIN       2
#define PICOHELD2_LCD_CS_PIN        3   // panel config SPI (bit-banged)
#define PICOHELD2_LCD_PCLK_PIN      4
#define PICOHELD2_LCD_DE_PIN        5
#define PICOHELD2_LCD_DB0_PIN       6   // DB0..DB15 = GPIO 6..21
#define PICOHELD2_LCD_SCK_PIN      34   // shared with SD SCK
#define PICOHELD2_LCD_MOSI_PIN     36

// --- SD CARD (SPI0) ---
#define PICOHELD2_SD_SCK_PIN       34
#define PICOHELD2_SD_MOSI_PIN      35
#define PICOHELD2_SD_MISO_PIN      32
#define PICOHELD2_SD_CS_PIN        33
// Card detect, input only, NO pulls: high = no card, low = card inserted.
// Also controls the SD supply; never drive it unless you want to cut SD power.
#define PICOHELD2_SD_DET_PIN       31

// --- PSRAM (APS6404L-3SQR, 8 MB) ---
#define PICOHELD2_PSRAM_CS_PIN      0   // XIP_CS1
#define PICOHELD2_PSRAM_SIZE_BYTES (8 * 1024 * 1024)

// --- UART ---
// The debug UART shares its pins with the SD card (TX=32/MISO, RX=33/CS),
// so there is deliberately no default UART. Use USB CDC stdio instead.
#define PICOHELD2_DBG_UART_TX_PIN  32
#define PICOHELD2_DBG_UART_RX_PIN  33

// --- SPI ---
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 0
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN PICOHELD2_SD_SCK_PIN
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN PICOHELD2_SD_MOSI_PIN
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN PICOHELD2_SD_MISO_PIN
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN PICOHELD2_SD_CS_PIN
#endif

#ifndef PICO_SD_CARD_DETECT_PIN
#define PICO_SD_CARD_DETECT_PIN PICOHELD2_SD_DET_PIN
#endif

// --- I2S AUDIO (for pico-extras/src/rp2_common/pico_audio_i2s/) ---
// Note: on this board LRCLK is the LOWER of the two clock pins.
#ifndef PICO_AUDIO_I2S_DATA_PIN
#define PICO_AUDIO_I2S_DATA_PIN PICOHELD2_I2S_DATA_PIN
#endif
#ifndef PICO_AUDIO_I2S_CLOCK_PIN_BASE
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE PICOHELD2_I2S_LRCLK_PIN
#endif

// --- FLASH (16 MB) ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (16 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif
