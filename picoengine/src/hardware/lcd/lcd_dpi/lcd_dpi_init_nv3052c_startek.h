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

/*************************** sleep/wake sequences ***********************/

// codes for Star Tek display
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

/*************************** init sequence ***********************/

static const uint8_t panel_init_operations[] = {
//***************************************************************//LCD SETING
  WRITE, 2, 0xFF, 0X30,  //PAGE01
  WRITE, 2, 0xFF, 0X52, 
  WRITE, 2, 0xFF, 0X01, 

  WRITE, 2, 0xE3, 0X00, 
  WRITE, 2, 0xF6, 0XC0, 
  WRITE, 2, 0xF0, 0X00, 
  WRITE, 2, 0x0A, 0X11, 
  WRITE, 2, 0x23, 0X20, //RGB interface control, DE MODE PCLK-N 

  WRITE, 2, 0x25, 0X14, 
  WRITE, 2, 0x26, 0X2E, 
  WRITE, 2, 0x27, 0X2E, 
  WRITE, 2, 0x29, 0X02, 
  WRITE, 2, 0x2A, 0XCF, 
  WRITE, 2, 0x32, 0X34, 


  WRITE, 2, 0x38, 0x9C, //VCOM_ADJ1
  WRITE, 2, 0x39, 0xA7, //VCOM_ADJ2 
  WRITE, 2, 0x3A, 0x7D, //VCOM_ADJ3
  
  WRITE, 2, 0x3B, 0X94, 
  WRITE, 2, 0x40, 0X07, 
  WRITE, 2, 0x42, 0X6D, 
  WRITE, 2, 0x43, 0X83, 
  WRITE, 2, 0x81, 0X00, 
  WRITE, 2, 0x91, 0X57, 
  WRITE, 2, 0x92, 0X57, 
  WRITE, 2, 0xA0, 0X52, 
  WRITE, 2, 0xA1, 0X50, 
  WRITE, 2, 0xA4, 0X9C, 
  WRITE, 2, 0xA7, 0X02, 
  WRITE, 2, 0xA8, 0X02, 
  WRITE, 2, 0xA9, 0X02, 
  WRITE, 2, 0xAA, 0XA8, 
  WRITE, 2, 0xAB, 0X28, 
  WRITE, 2, 0xAE, 0XD2, 
  WRITE, 2, 0xAF, 0X02, 
  WRITE, 2, 0xB0, 0XD2, 
  WRITE, 2, 0xB2, 0X26, 
  WRITE, 2, 0xB3, 0X26, 

  WRITE, 2, 0xFF, 0X30,  //PAGE02
  WRITE, 2, 0xFF, 0X52, 
  WRITE, 2, 0xFF, 0X02, 
//**************GAMMA SET **************//
  WRITE, 2, 0xB0, 0X02, 
  WRITE, 2, 0xB1, 0X0E, 
  WRITE, 2, 0xB2, 0X08, 
  WRITE, 2, 0xB3, 0X29, 
  WRITE, 2, 0xB4, 0X28, 
  WRITE, 2, 0xB5, 0X37, 
  WRITE, 2, 0xB6, 0X12, 
  WRITE, 2, 0xB7, 0X32, 
  WRITE, 2, 0xB8, 0X0B, 
  WRITE, 2, 0xB9, 0X03, 
  WRITE, 2, 0xBA, 0X0E, 
  WRITE, 2, 0xBB, 0X0D, 
  WRITE, 2, 0xBC, 0X10, 
  WRITE, 2, 0xBD, 0X13, 
  WRITE, 2, 0xBE, 0X18, 
  WRITE, 2, 0xBF, 0X0F, 
  WRITE, 2, 0xC0, 0X16, 
  WRITE, 2, 0xC1, 0X08, 

  WRITE, 2, 0xD0, 0X05, 
  WRITE, 2, 0xD1, 0X0B, 
  WRITE, 2, 0xD2, 0X03, 
  WRITE, 2, 0xD3, 0X33, 
  WRITE, 2, 0xD4, 0X32, 
  WRITE, 2, 0xD5, 0X32, 
  WRITE, 2, 0xD6, 0X0F, 
  WRITE, 2, 0xD7, 0X39, 
  WRITE, 2, 0xD8, 0X0B, 
  WRITE, 2, 0xD9, 0X02, 
  WRITE, 2, 0xDA, 0X10, 
  WRITE, 2, 0xDB, 0X0F, 
  WRITE, 2, 0xDC, 0X11, 
  WRITE, 2, 0xDD, 0X14, 
  WRITE, 2, 0xDE, 0X1A, 
  WRITE, 2, 0xDF, 0X11, 
  WRITE, 2, 0xE0, 0X18, 
  WRITE, 2, 0xE1, 0X04, 
//***************************************//
  WRITE, 2, 0xFF, 0x30, 
  WRITE, 2, 0xFF, 0x52, 
  WRITE, 2, 0xFF, 0x03, //PAGE03 

  WRITE, 2, 0x00, 0X00, 
  WRITE, 2, 0x01, 0X00, 
  WRITE, 2, 0x02, 0X00, 
  WRITE, 2, 0x03, 0X00, 
  WRITE, 2, 0x08, 0X0A, 
  WRITE, 2, 0x09, 0X0B, 
  WRITE, 2, 0x0A, 0X0C, 
  WRITE, 2, 0x0B, 0X0D, 
  WRITE, 2, 0x20, 0X00, 
  WRITE, 2, 0x21, 0X00, 
  WRITE, 2, 0x22, 0X00, 
  WRITE, 2, 0x23, 0X00, 
  WRITE, 2, 0x28, 0X22, 
  WRITE, 2, 0x2A, 0Xe9, 
  WRITE, 2, 0x2B, 0Xe9, 
  WRITE, 2, 0x30, 0X00, 
  WRITE, 2, 0x31, 0X00, 
  WRITE, 2, 0x32, 0X00, 
  WRITE, 2, 0x33, 0X00, 
  WRITE, 2, 0x34, 0XA1, 
  WRITE, 2, 0x35, 0X00, 
  WRITE, 2, 0x36, 0X26,   
  WRITE, 2, 0x37, 0X03, 
  WRITE, 2, 0x40, 0X0D,   
  WRITE, 2, 0x41, 0X0E,   
  WRITE, 2, 0x42, 0X0F,   
  WRITE, 2, 0x43, 0X10,   
  WRITE, 2, 0x44, 0X22, 
  WRITE, 2, 0x45, 0Xe4,   
  WRITE, 2, 0x46, 0Xe5,   
  WRITE, 2, 0x47, 0X22, 
  WRITE, 2, 0x48, 0Xe6,   
  WRITE, 2, 0x49, 0Xe7,   
  WRITE, 2, 0x50, 0X11,   
  WRITE, 2, 0x51, 0X12,   
  WRITE, 2, 0x52, 0X13,   
  WRITE, 2, 0x53, 0X14,   
  WRITE, 2, 0x54, 0X22, 
  WRITE, 2, 0x55, 0Xe8,   
  WRITE, 2, 0x56, 0Xe9,   
  WRITE, 2, 0x57, 0X22, 
  WRITE, 2, 0x58, 0Xea,   
  WRITE, 2, 0x59, 0Xeb,   
  WRITE, 2, 0x60, 0X05,   
  WRITE, 2, 0x61, 0X05,   
  WRITE, 2, 0x65, 0X0A,   
  WRITE, 2, 0x66, 0X0A,   
  WRITE, 2, 0x80, 0X0f,     
  WRITE, 2, 0x81, 0X0d,     
  WRITE, 2, 0x82, 0X0b,     
  WRITE, 2, 0x83, 0X09,     
  WRITE, 2, 0x8A, 0X01,     
  WRITE, 2, 0x8B, 0X1f,     
  WRITE, 2, 0x8D, 0X00,     
  WRITE, 2, 0x8E, 0X03,     
  WRITE, 2, 0x90, 0X1f,     
  WRITE, 2, 0x91, 0X1f,     
  WRITE, 2, 0x92, 0X11,     
  WRITE, 2, 0x94, 0X00,     
  WRITE, 2, 0x95, 0X00,     
  WRITE, 2, 0x96, 0X10,     
  WRITE, 2, 0x97, 0X0e,     
  WRITE, 2, 0x98, 0X0c,     
  WRITE, 2, 0x99, 0X0a,     
  WRITE, 2, 0xA0, 0X02,     
  WRITE, 2, 0xA1, 0X1f,     
  WRITE, 2, 0xA3, 0X00,     
  WRITE, 2, 0xA4, 0X04,     
  WRITE, 2, 0xA6, 0X1f,     
  WRITE, 2, 0xA7, 0X1f,     
  WRITE, 2, 0xA8, 0X12,     
  WRITE, 2, 0xAA, 0X00,     
  WRITE, 2, 0xAB, 0X00,  
  WRITE, 2, 0xB0, 0X0A,  
  WRITE, 2, 0xB1, 0X0C,  
  WRITE, 2, 0xB2, 0X0E,  
  WRITE, 2, 0xB3, 0X10,  
  WRITE, 2, 0xBa, 0X04,  
  WRITE, 2, 0xBb, 0X00,  
  WRITE, 2, 0xBd, 0X1F,  
  WRITE, 2, 0xBe, 0X02,  
  WRITE, 2, 0xc0, 0X1f,     
  WRITE, 2, 0xc1, 0X1f,     
  WRITE, 2, 0xc2, 0X11,     
  WRITE, 2, 0xc4, 0X00,     
  WRITE, 2, 0xc5, 0X00,  
  WRITE, 2, 0xC6, 0X09,  
  WRITE, 2, 0xC7, 0X0B,  
  WRITE, 2, 0xC8, 0X0D,  
  WRITE, 2, 0xC9, 0X0F, 
  WRITE, 2, 0xD0, 0X03,  
  WRITE, 2, 0xD1, 0X00,  
  WRITE, 2, 0xD3, 0X1F, 
  WRITE, 2, 0xD4, 0X01, 
  WRITE, 2, 0xd6, 0X1f,     
  WRITE, 2, 0xd7, 0X1f,     
  WRITE, 2, 0xd8, 0X12,     
  WRITE, 2, 0xdA, 0X00,     
  WRITE, 2, 0xdB, 0X00,  
  
  WRITE, 2, 0xFF, 0X30, //PAGE0
  WRITE, 2, 0xFF, 0X52, 
  WRITE, 2, 0xFF, 0X00, 

  WRITE, 2, 0x3A, 0X50, //24BIT

  WRITE, 2, 0x36, 0x09, //default: 0X0a, // 0x09 flipped vertically and horizontally, 0a: default
  WRITE, 1, 0x11, 
  DELAY, 120,                        
  WRITE, 1, 0x29, 
  DELAY, 20,                                    

  END_LIST
};  
