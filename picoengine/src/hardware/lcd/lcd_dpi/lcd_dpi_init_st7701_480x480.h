/*
 * Pico Engine - a library for the Pico Held 2 handheld
 *
 * Copyright (C) 2023 - 2025 Daniel Kammer (daniel.kammer@web.de)
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
 
#include <hwcfg.h>

#pragma once

#include <typedefs.h>

#error "please setup the following lines:"

#define LCD_FREQ  (37000000) // (37000000)

#define LCD_VBP		      (20)
#define LCD_VFP	 	      (15)
#define LCD_VPW		       (5)
#define LCD_HBP	          (80)
#define LCD_HFP		      (15)  // min: 4
#define LCD_HPW	           (5)  // min: 1

#define LCD_PHYS_RES_X   (720)  // max: 1024
#define LCD_PHYS_RES_Y   (720)  // max: 1024

typedef enum
{
  END_LIST,
  DELAY,
  WRITE,
} spi_operation_type_t;

typedef enum
{
  SPI_COMMAND,
  SPI_DATA
} spi_dc_t;

void lcd_spi_send(const uint8_t* init_seq);

/*************************** wake/sleep sequences ***********************/
static const uint8_t panel_sleep_operations[] = {
  // enter sleep
  WRITE, 1, 0x28,
  DELAY, 10,                        
  WRITE, 1, 0x10, 
  DELAY, 20,                                    

  END_LIST
};

static const uint8_t panel_wake_operations[] = {
  // enter sleep
  WRITE, 1, 0x11,
  DELAY, 120,
  WRITE, 1, 0x29, 

  END_LIST
};

/*************************** init sequences ***********************/
// For st7701 (480x480) LCD
static const uint8_t panel_init_operations[] = {
    WRITE, 6, 0xFF,
      0x77, 0x01, 0x00, 0x00, 0x10,

    WRITE, 3, 0xC0,
       0x3B, 0x00,
    WRITE, 3, 0xC1,
      0x0D, 0x02,
    WRITE, 3, 0xC2,
      0x31, 0x05,
    WRITE, 2, 0xCD, 0x08,

    WRITE, 17, 0xB0, // Positive Voltage Gamma Control
      0x00, 0x11, 0x18, 0x0E,
      0x11, 0x06, 0x07, 0x08,
      0x07, 0x22, 0x04, 0x12,
      0x0F, 0xAA, 0x31, 0x18,

    WRITE, 17, 0xB1, // Negative Voltage Gamma Control
      0x00, 0x11, 0x19, 0x0E,
      0x12, 0x07, 0x08, 0x08,
      0x08, 0x22, 0x04, 0x11,
      0x11, 0xA9, 0x32, 0x18,

    // PAGE1
    WRITE, 6, 0xFF,
      0x77, 0x01, 0x00, 0x00, 0x11,

    WRITE, 2, 0xB0, 0x60, // Vop=4.7375v
    WRITE, 2, 0xB1, 0x32, // VCOM=32
    WRITE, 2, 0xB2, 0x07, // VGH=15v
    WRITE, 2, 0xB3, 0x80,
    WRITE, 2, 0xB5, 0x49, // VGL=-10.17v
    WRITE, 2, 0xB7, 0x85,
    WRITE, 2, 0xB8, 0x21, // AVDD=6.6 & AVCL=-4.6
    WRITE, 2, 0xC1, 0x78,
    WRITE, 2, 0xC2, 0x78,

    WRITE, 4, 0xE0,
     0x00, 0x1B, 0x02,

    WRITE, 12, 0xE1,
      0x08, 0xA0, 0x00, 0x00,
      0x07, 0xA0, 0x00, 0x00,
      0x00, 0x44, 0x44,

    WRITE, 13, 0xE2,
      0x11, 0x11, 0x44, 0x44,
      0xED, 0xA0, 0x00, 0x00,
      0xEC, 0xA0, 0x00, 0x00,

    WRITE, 5, 0xE3,
      0x00, 0x00, 0x11, 0x11,

    WRITE, 3, 0xE4,
      0x44, 0x44,

    WRITE, 17, 0xE5,
      0x0A, 0xE9, 0xD8, 0xA0,
      0x0C, 0xEB, 0xD8, 0xA0,
      0x0E, 0xED, 0xD8, 0xA0,
      0x10, 0xEF, 0xD8, 0xA0,

    WRITE, 5, 0xE6,
      0x00, 0x00, 0x11, 0x11,

    WRITE, 3, 0xE7,
      0x44, 0x44,

    WRITE, 17, 0xE8,
      0x09, 0xE8, 0xD8, 0xA0,
      0x0B, 0xEA, 0xD8, 0xA0,
      0x0D, 0xEC, 0xD8, 0xA0,
      0x0F, 0xEE, 0xD8, 0xA0,

    WRITE, 8, 0xEB,
      0x02, 0x00, 0xE4, 0xE4,
      0x88, 0x00, 0x40,

    WRITE, 3, 0xEC,
      0x3C, 0x00,

    WRITE, 17, 0xED,
      0xAB, 0x89, 0x76, 0x54,
      0x02, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0x20,
      0x45, 0x67, 0x98, 0xBA,

    //-----------VAP & VAN---------------
    WRITE, 6, 0xFF,
      0x77, 0x01, 0x00, 0x00, 0x13,

    WRITE, 2, 0xE5, 0xE4,

    WRITE, 6, 0xFF,
      0x77, 0x01, 0x00, 0x00, 0x00,

    WRITE, 1, 0x21,   // 0x20 normal, 0x21 IPS
    WRITE, 2, 0x3A, 0x60, // 0x70 RGB888, 0x60 RGB666, 0x50 RGB565

    WRITE, 1, 0x11, // Sleep Out

    DELAY, 120,

    WRITE, 1, 0x29, // Display On

    END_LIST
  };
