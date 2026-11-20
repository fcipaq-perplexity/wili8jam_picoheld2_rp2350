/*
 * pplib - a library for the Pico Held handheld
 *
 * Copyright (C) 2023 Daniel Kammer (daniel.kammer@web.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#ifndef HWCFG_H
#define HWCFG_H

// 0 = P2 v1.5 w/ Sunmaxic display
// 1 = P2 v1.4 w/ Buydisplay display

#ifndef PE_CONFIG_NO
#define PE_CONFIG_NO 0
#endif

#if PE_CONFIG_NO==0
// ================================== CONFIG 0 (P2 1.5 SunmaxIC) ============================================

/*----------------- buttons --------------- */
#define ADC_PIN_OFFSET  (40) // RP235xB

#define PIN_ANALOG_X    (45)
#define PIN_ANALOG_Y    (47)

#define PIN_BTN_A       (25)  // A
#define PIN_BTN_B       (24)  // B
#define PIN_BTN_X       (26)  // X
#define PIN_BTN_Y       (22)  // Y
#define PIN_BTN_L       (42)  // L
#define PIN_BTN_R       (27)  // R
#define PIN_BTN_ST      (29)  // St
#define PIN_BTN_SEL     (23)  // Sel
#define PIN_BTN_SW      (46)  // Ana SW

#define PIN_DPAD_LR     (43)  // LR
#define PIN_DPAD_UD     (44)  // UD

/* ------------------------ Power management ------------------------*/
#define PIN_BAT_ADC     (41)
#define PIN_PWR_EN      (28)
#define PIN_CHG_STAT    (30)

// Physical battery and power supply parameters (in mV)
#define PWR_REF_VOL     (3300)  // 3.3V rail
#define PWR_BAT_MAX_VOL (4050)
#define PWR_BAT_MIN_VOL (3400)
#define PWR_CHG_MIN_VOL (4170)

/* ------------------------ Sound ------------------------*/
#define PIN_SND_EN      (40)
#define PIN_I2S_LR      (37)  // LR and BCK need to be adjacent pins with LR being the lower GPIO #
#define PIN_I2S_BCK     (38)
#define PIN_I2S_DAT     (39)
/* supported frequencies (kHz): 44.1, 32, 16, 8 */

/* ---------------------- LCD ----------------------*/
#define LCD_IF_DPI

// Sunmaxic display
#define LCD_DRIVER "lcd_dpi_init_nv3052c_sunmix.h"

// display pins
#define PIN_LCD_BL_PWM   (1)

// Note: the data pins are PIO hardware configured and driven and need to be in consecutive order
#define PIN_LCD_DB0      (6)    // 1st of the 16 data pins, that's RRRRRGGGGGGBBBBB (from left to right: from LSB to MSB)

#define PIN_LCD_DE       (5)    // data enable
#define PIN_LCD_PCLK     (4)    // pixel clock
#define PIN_LCD_RST      (2)    // reset pin

// LCD SPI
#define PIN_LCD_SCK     (34)
#define PIN_LCD_MOSI    (36)
#define PIN_LCD_CS       (3)

/* ---------------------- SD card ----------------------*/
#define PIN_SD_SCK      (PIN_LCD_SCK)  // shared pin
#define PIN_SD_MOSI     (35)
#define PIN_SD_MISO     (32)
#define PIN_SD_CS       (33)
#define PIN_SD_DET      (31)

/* ---------------------- PSRAM ----------------------*/
#define PIN_PSRAM_CS    (0)
 
/* ---------------------- UART ----------------------*/
#define PIN_DBG_UART_RX (PIN_SD_CS)    // shared pin
#define PIN_DBG_UART_TX (PIN_SD_MISO)  // shared pin

/* ---------------------- button map ----------------------*/
// pin ID,      gpio #,      signal type, pull mode,    thrs 1,  thrs 2,  pressed state
#define BUTTON_MAP \
 [BUTTON_A]     = { BUTTON_A,     PIN_BTN_A,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_B]     = { BUTTON_B,     PIN_BTN_B,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_X]     = { BUTTON_X,     PIN_BTN_X,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_Y]     = { BUTTON_Y,     PIN_BTN_Y,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_ST]    = { BUTTON_ST,    PIN_BTN_ST,  BTN_DIGITAL, BTN_PULLDOWN, BTN_N_A, BTN_N_A, BTN_PRESSED_ON_HI }, \
 [BUTTON_SEL]   = { BUTTON_SEL,   PIN_BTN_SEL, BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_L]     = { BUTTON_L,     PIN_BTN_L,   BTN_ANALOG,  BTN_PULLUP,         0,     500, BTN_N_A           }, \
 [BUTTON_R]     = { BUTTON_R,     PIN_BTN_R,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_SW]    = { BUTTON_SW,    PIN_BTN_SW,  BTN_ANALOG,  BTN_PULLUP,         0,     500, BTN_N_A           }, \
 [BUTTON_LEFT]  = { BUTTON_LEFT,  PIN_DPAD_LR, BTN_ANALOG,  BTN_N_A,            0,    1450, BTN_N_A           }, \
 [BUTTON_RIGHT] = { BUTTON_RIGHT, PIN_DPAD_LR, BTN_ANALOG,  BTN_N_A,         2250,    4095, BTN_N_A           }, \
 [BUTTON_UP]    = { BUTTON_UP,    PIN_DPAD_UD, BTN_ANALOG,  BTN_N_A,            0,    1450, BTN_N_A           }, \
 [BUTTON_DOWN]  = { BUTTON_DOWN,  PIN_DPAD_UD, BTN_ANALOG,  BTN_N_A,         2250,    4096, BTN_N_A           }, \

#elif PE_CONFIG_NO==1
// ================================== CONFIG 1 (P2 1.4 Buydisplay) ============================================

/*----------------- buttons --------------- */
#define ADC_PIN_OFFSET  (40) // RP235xB

#define PIN_ANALOG_X    (45)
#define PIN_ANALOG_Y    (47)

#define PIN_BTN_A       (25)  // A
#define PIN_BTN_B       (24)  // B
#define PIN_BTN_X       (26)  // X
#define PIN_BTN_Y       (22)  // Y
#define PIN_BTN_L       (42)  // L
#define PIN_BTN_R       (27)  // R
#define PIN_BTN_ST      (29)  // St
#define PIN_BTN_SEL     (23)  // Sel
#define PIN_BTN_SW      (46)  // Ana SW

#define PIN_DPAD_LR     (43)  // LR
#define PIN_DPAD_UD     (44)  // UD

/* ------------------------ Power management ------------------------*/
#define PIN_BAT_ADC     (41)
#define PIN_PWR_EN      (28)
#define PIN_CHG_STAT    (30)

// Physical battery and power supply parameters (in mV)
#define PWR_REF_VOL     (3300)  // 3.3V rail
#define PWR_BAT_MAX_VOL (4050)
#define PWR_BAT_MIN_VOL (3400)
#define PWR_CHG_MIN_VOL (4170)

/* ------------------------ Sound ------------------------*/
#define PIN_SND_EN      (40)
#define PIN_I2S_LR      (37)  // LR and BCK need to be adjacent pins with LR being the lower GPIO #
#define PIN_I2S_BCK     (38)
#define PIN_I2S_DAT     (39)
/* supported frequencies (kHz): 44.1, 32, 16, 8 */

/* ---------------------- LCD ----------------------*/
#define LCD_IF_DPI

// Buydisplay display
#define LCD_DRIVER "lcd_dpi_init_nv3052c_buydisplay.h"

// display pins
#define PIN_LCD_BL_PWM   (1)

// Note: the data pins are PIO hardware configured and driven and need to be in consecutive order
#define PIN_LCD_DB0      (6)    // 1st of the 16 data pins, that's RRRRRGGGGGGBBBBB (from left to right: from LSB to MSB)

#define PIN_LCD_DE       (5)    // data enable
#define PIN_LCD_PCLK     (4)    // pixel clock
#define PIN_LCD_RST      (2)    // reset pin

// LCD SPI
#define PIN_LCD_SCK     (34)
#define PIN_LCD_MOSI    (36)
#define PIN_LCD_CS       (3)

/* ---------------------- SD card ----------------------*/
#define PIN_SD_SCK      (PIN_LCD_SCK)  // shared pin
#define PIN_SD_MOSI     (35)
#define PIN_SD_MISO     (32)
#define PIN_SD_CS       (33)
#define PIN_SD_DET      (31)

/* ---------------------- PSRAM ----------------------*/
#define PIN_PSRAM_CS    (0)
 
/* ---------------------- UART ----------------------*/
#define PIN_DBG_UART_RX (PIN_SD_CS)    // shared pin
#define PIN_DBG_UART_TX (PIN_SD_MISO)  // shared pin

/* ---------------------- button map ----------------------*/
// pin ID,      gpio #,      signal type, pull mode,    thrs 1,  thrs 2,  pressed state
#define BUTTON_MAP \
 [BUTTON_A]     = { BUTTON_A,     PIN_BTN_A,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_B]     = { BUTTON_B,     PIN_BTN_B,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_X]     = { BUTTON_X,     PIN_BTN_X,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_Y]     = { BUTTON_Y,     PIN_BTN_Y,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_ST]    = { BUTTON_ST,    PIN_BTN_ST,  BTN_DIGITAL, BTN_PULLDOWN, BTN_N_A, BTN_N_A, BTN_PRESSED_ON_HI }, \
 [BUTTON_SEL]   = { BUTTON_SEL,   PIN_BTN_SEL, BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_L]     = { BUTTON_L,     PIN_BTN_L,   BTN_ANALOG,  BTN_PULLUP,         0,     500, BTN_N_A           }, \
 [BUTTON_R]     = { BUTTON_R,     PIN_BTN_R,   BTN_DIGITAL, BTN_PULLUP,   BTN_N_A, BTN_N_A, BTN_PRESSED_ON_LO }, \
 [BUTTON_SW]    = { BUTTON_SW,    PIN_BTN_SW,  BTN_ANALOG,  BTN_PULLUP,         0,     500, BTN_N_A           }, \
 [BUTTON_LEFT]  = { BUTTON_LEFT,  PIN_DPAD_LR, BTN_ANALOG,  BTN_N_A,            0,    1450, BTN_N_A           }, \
 [BUTTON_RIGHT] = { BUTTON_RIGHT, PIN_DPAD_LR, BTN_ANALOG,  BTN_N_A,         2250,    4095, BTN_N_A           }, \
 [BUTTON_UP]    = { BUTTON_UP,    PIN_DPAD_UD, BTN_ANALOG,  BTN_N_A,            0,    1450, BTN_N_A           }, \
 [BUTTON_DOWN]  = { BUTTON_DOWN,  PIN_DPAD_UD, BTN_ANALOG,  BTN_N_A,         2250,    4096, BTN_N_A           }, \

#endif  // CONFIG SELECTION

#endif //HWCFG_H
