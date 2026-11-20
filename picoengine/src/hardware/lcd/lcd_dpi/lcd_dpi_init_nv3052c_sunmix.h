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

#define LCD_FREQ  (37000000) // (37000000)

#define LCD_VBP		      (20)
#define LCD_VFP	 	      (15)
#define LCD_VPW		       (5)
#define LCD_HBP	        (80)
#define LCD_HFP		      (15)  // min: 4
#define LCD_HPW	         (5)  // min: 1

#define LCD_PHYS_RES_X (720)  // max: 1024
#define LCD_PHYS_RES_Y (720)  // max: 1024

#include <typedefs.h>

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

/*************************** init sequences ***********************/

// *************** Init code for 720x720 square display ZJY400-8532ANT from Sunmaxic ***************
static const uint8_t panel_init_operations[] = {
  // EXT page 1 (vendor specifig commands)
  WRITE, 2, 0xFF, 0x30,              
  WRITE, 2, 0xFF, 0x52,
  WRITE, 2, 0xFF, 0x01,
	
  WRITE, 2, 0xE3, 0x00,   // magic
  WRITE, 2, 0x0A, 0x00,   // magic

  //WRITE, 2, 0x23, 0x20,  // 0x20=DE, 0x00=DE+SYNC
  WRITE, 2, 0xFF, 0x30,
  WRITE, 2, 0xFF, 0x52,
  WRITE, 2, 0xFF, 0x00,
  // END (exit EXTC)

  WRITE, 2, 0x36, 0x09, //default: 0X0a, // 0x09 flipped vertically and horizontally, 0a: default
												
  WRITE, 1, 0x11,  // Sleep out
	
  DELAY, 120,
	
  WRITE, 1, 0x29,  // Display on

  END_LIST
	
};

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
  WRITE, 1, 0x11, // Sleep out
  DELAY, 120,
  WRITE, 1, 0x29, // Display on

  END_LIST
};
