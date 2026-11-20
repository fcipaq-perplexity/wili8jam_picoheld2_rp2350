/*
 * Blitter example for the Pico Held 2 Pico Engine library
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
 
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include <picoengine.h>
#include "hwcfg.h"
#include <graphics/blitter.h>

#include <fonts/fontfiles/f13x16.h>

#include <stdlib.h>  // itoa()
#include <cstdio>

#include <time.h>
#include <math.h>
#include "pico/mutex.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "sparkfun_pico/sfe_pico_alloc.h"

/* ====================== assets ====================== */
#if LCD_COLORDEPTH!=8
#error "Sorry, all assets are 8 bit, color depth needs to be 8 bits. Please set in setup.h"
#endif

// Assets by freepik.com
#include "assets/smiley_8.h"
#include "assets/smiley2_8.h"

#include "assets/mario_circuit1_map.h"
#if LCD_COLORDEPTH == 16
#include "assets/mario_circuit1_tiles.h"
#include "assets/car_rear_data.h"
#elif LCD_COLORDEPTH == 8
#include "assets/mario_circuit1_tiles_8bit.h"
#include "assets/car_rear_data8.h"
#endif

#define SOUND_EN

#ifdef SOUND_EN
#include "assets/music.h"
#include "assets/snd_low_grip.h"
#include "assets/snd_motor_accel.h"
#include "assets/snd_motor_idle.h"
uint8_t music_on = 1;
#endif

#define TRANSPARENT_COLOR (0xcd)

pe_config_t pe_cfg;


LCD_DPI* lcd;
/* ==================== smileys ==================== */

typedef struct 
{
  int x;   // position x
  int y;   // position y
  int z;   // zoom
  int r;   // rotation
  int dx;  // delta x
  int dy;  // delta y
  int dz;  // delta zoom
  int dr;  // delta rotation

  int zx;  // stretch x
  int zy;  // stretch y
  int dzx; // delta stretch x
  int dzy; // delta stretch y
} smiley_t;

// set no. of sprites
#define MAX_SMILEYS (200)
  
smiley_t smiley[2][MAX_SMILEYS / 2];

/* ====================== racing "game" ====================== */
uint8_t tile_size = tiles_data.width / 8;  // normalize to 8 by 8 tile size

bool double_buffering = false;

int v_theta_start = 180;
int v_theta_delta = 0;
int start_x = 1430, start_y = 1112;

gbuffer_t fb_back;
int lcd_scale = 3;

/* ==================== implementation ==================== */

#ifndef ARDUINO
uint32_t millis() {
  return to_ms_since_boot(get_absolute_time());
}

uint32_t random(uint32_t rand_max) {
  return rand() % rand_max;
}
#endif

void blink_error(unsigned int n) {
  for (int i = 1; i < 10; i++) {
    printf("%d\n", n);
    sleep_ms(1000);
  }
  pwr_shutdown();
}

void setup() {
  int ret = 0;

  #if 1
    uint32_t sys_clock_khz = 300000;   // 276 kHz
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(10);
    set_sys_clock_khz(sys_clock_khz, true);
  #endif

  stdio_init_all();

#define UART_ENABLE
#ifdef UART_ENABLE
  // Initialize chosen UART peripheral (UART0 here)
  uart_init(uart0, 115200);

  // Set the GPIO pins for UART0 TX and RX
  gpio_set_function(PIN_DBG_UART_RX, GPIO_FUNC_UART);
  gpio_set_function(PIN_DBG_UART_TX, GPIO_FUNC_UART);
#endif

  if (pe_init() != PE_SUCCESS)
    blink_error(1);

  sleep_ms(5);  // 750 us required. TODO: investigate why

  sleep_ms(10000);
  printf("laice1");

#if 0
  if (!sfe_pico_alloc_init(PIN_PSRAM_CS, 80000000));  //70  // Pimoroni can do 90 - why?
    blink_error(2);
#endif

#ifdef SOUND_EN
  ret = snd_init();
  if (ret != SND_SUCCESS)
    blink_error(ret);

  snd_set_vol(2);
  snd_unmute();
#endif

  lcd = new LCD_DPI();
  printf("laice2");

  pe_cfg = {
    .lcd_num_fb = 2,
    .lcd_refresh_mode = LCD_FRR_REFRESH,
    .lcd_scale = lcd_scale,
  };

  ret = lcd->init(pe_cfg.lcd_num_fb, pe_cfg.lcd_refresh_mode, pe_cfg.lcd_scale);
  if (ret != LCD_SUCCESS)
    blink_error(ret);

  lcd_backlight_init();
  lcd_backlight_set(100);
  printf("laice3");

#if 0
  // jumpy
  uint32_t tim = millis();
  while (1) {
    gbuffer_t fb_back;
    lcd->get_backbuffer(&fb_back);

    if (pe_cfg.lcd_refresh_mode == LCD_VRR_REFRESH)
      lcd->refresh();  // waits for vsync

    for (int i = 0; i < 10000; i++)
      fb_back.data[i] = i % 256;

    if (millis() - tim > 2000) {
      printf("alive\n");
      tim = millis();
    }


  }
#endif
  printf("laice4");

}

void racing_game() {
//  for (int h = 0; h < 256; h++)
 //   custom_palette[h] = smiley_palette_data[h];

  #if defined LCD_DOUBLE_PIXEL_LINEAR || defined LCD_DOUBLE_PIXEL_NEAREST
    float defaultzoom = 1.0f;
  #else
    float defaultzoom = 2.0f;
  #endif

  float a = 0.05f;  // acceleration
  float b = -0.2f;  // deceleration
  float v = 0.0f;   // velocity
  float v_max = 9;  // max. velocity

  float zoom = defaultzoom;
  float pos_x = start_x;  // position on map
  float pos_y = start_y;
  int v_theta = v_theta_start;  // orientation of car

  // fps counting
  uint32_t fps_timer = millis();
  uint16_t fps_cnt = 0;
  uint16_t fps = 0;

  uint8_t mode7 = 0;  // render mode
  uint8_t b1_deb = 0;  // button debounce

  while (1) {
    draw_rect_fill(0, 0, lcd->get_screen_width() - 1, lcd->get_screen_height() - 1, 0x00, fb_back);

    // query button states
    ctrl_button_state_update();

    if (ctrl_is_pressed(BUTTON_R)) {
      lcd_backlight_set(0);
      pwr_shutdown();  
    }

    if (ctrl_is_pressed(BUTTON_L))
      return;

    if (ctrl_is_pressed(BUTTON_SEL)) {
      #ifdef SOUND_EN
        if (!b1_deb) {
          b1_deb = 1;
          if (music_on)
            music_on = 0;
          else
            music_on = 1;
        } else {
          b1_deb = 0;
        }
      #endif
    } else {

      if (ctrl_is_pressed(BUTTON_A)) {
        if (!b1_deb) {
          b1_deb = 1;
          mode7++;
          if (mode7 == 4)
            mode7 = 0;
        }
      } else {
        b1_deb = 0;
      }

      if (ctrl_is_pressed(BUTTON_B)) {
        v += a;
        if (v > v_max)
          v = v_max;
        #ifdef SOUND_EN
        if (music_on) {
          if (snd_num_bufs_waiting(SND_CHAN_2) == 0) {
            if (v < v_max / 2)
              snd_enqueue_buf(snd_motor_accel, SND_CHAN_2, SND_NONBLOCKING);
            else
              snd_enqueue_buf(snd_motor_idle, SND_CHAN_2, SND_NONBLOCKING);
          }
        }
        #endif
      }

      if (ctrl_is_pressed(BUTTON_X)) {
        v += b;
        if (v < 0)
          v = 0;
      }
    }

    if (!ctrl_is_pressed(BUTTON_ANY)) {
      v += b / 10;
      if (v < 0)
        v = 0;
    }

    uint8_t steering = 0;

    if ((v >= 0.3f) && (ctrl_is_pressed(BUTTON_LEFT))) {
      v_theta += 2;
      steering = 1;
    }

    if ((v >= 0.3f) && (ctrl_is_pressed(BUTTON_RIGHT))) {
      v_theta -= 2;
      steering = 1;
    }

    if (steering)
      v_theta_delta += 1;
    else
      v_theta_delta--;

    if (v_theta_delta > 50)
      v_theta_delta = 50;
    if (v_theta_delta < 0)
      v_theta_delta = 0;

    #ifdef SOUND_EN
      if (music_on) {
        if ((v_theta_delta == 50) && (snd_num_bufs_waiting(SND_CHAN_1) == 0))
          snd_enqueue_buf(snd_low_grip, SND_CHAN_1, SND_NONBLOCKING);
      }
    #endif

    if (v_theta > 360)
      v_theta -= 360;
    if (v_theta < 0)
      v_theta += 360;

    // calculate new position
    float theta = v_theta * M_PI / 180;
    pos_x += v * sinf(theta);
    pos_y += v * cosf(theta);

    if ((pos_x > 192 * 8) || (pos_x < 64 * 8) || (pos_y > 192 * 8) || (pos_y < 64 * 8)) {
      pos_x = start_x;
      pos_y = start_y;
      v = 0;
      v_theta = v_theta_start;
    }

    zoom = (defaultzoom / (v / 10 + 1));

    if (mode7 == 0)
      zoom /= 2;

    uint16_t tm_x;
    uint16_t tm_y;
    uint16_t tm_w;
    uint16_t tm_h;

    if ((mode7 == 0) || (mode7 == 1)) {
      // view from top (car rotating)
      if (mode7 == 1) {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd->get_screen_width();
        tm_h = lcd->get_screen_height();
      } else {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd->get_screen_width() / 2;
        tm_h = lcd->get_screen_height() / 2;
      }

      tile_blit(tm_x,
                tm_y,
                tm_w,
                tm_h,
                pos_x - tm_w / 2 / zoom,  // pivot in centre
                pos_y - tm_h / 2 / zoom,  // pivot in centre
                zoom / tile_size,
                zoom / tile_size,
                map_data,
                tiles_data,
                BLIT_NO_ALPHA,
                fb_back);
      #if LCD_COLORDEPTH == 16
        blit_buf(tm_x + tm_w / 2, tm_y + tm_h / 2, zoom / 2, v_theta * M_PI / 180, 0xf81f, car_data, fb_back);
      #elif LCD_COLORDEPTH == 8
        blit_buf(tm_x + tm_w / 2, tm_y + tm_h / 2, zoom / 2, v_theta * M_PI / 180, 0xe3, car_data, fb_back);
      #endif
    }

    if ((mode7 == 0) || (mode7 == 2)) {
      // view from top (map rotating)
      if (mode7 == 2) {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd->get_screen_width();
        tm_h = lcd->get_screen_height();
      } else {
        tm_x = 0;
        tm_y = lcd->get_screen_height() / 2;
        tm_w = lcd->get_screen_width() / 2;
        tm_h = lcd->get_screen_height() / 2;
      }

      tile_blit_rot(tm_x,
                    tm_y,
                    tm_w,
                    tm_h,
                    pos_x,
                    pos_y,
                    tm_w / 2,
                    tm_h / 2,
                    (180 - v_theta) * M_PI / 180.0f,
                    zoom / (tiles_data.width / 8),
                    zoom / (tiles_data.height / 8),
                    map_data,
                    tiles_data,
                    BLIT_NO_ALPHA,
                    fb_back);

      #if LCD_COLORDEPTH == 16
        color_t alpha = 0xf81f;
      #elif LCD_COLORDEPTH == 8
        color_t alpha = 0xe3;
      #endif
      
      blit_buf(tm_x + tm_w / 2, tm_y + tm_h / 2, zoom / 2, M_PI, alpha, car_data, fb_back);

    }

    if ((mode7 == 0) || (mode7 == 3)) {
      // mode7 pseudo 3D
      int sky_height = 30;

      if (mode7 == 3) {
        tm_x = 0;
        tm_y = 0;
        tm_w = lcd->get_screen_width();
        tm_h = lcd->get_screen_height() - 10;
      } else {
        tm_x = lcd->get_screen_width() / 2;
        tm_y = 0;
        tm_w = lcd->get_screen_width() / 2;
        tm_h = lcd->get_screen_height() / 2;
      }

      int a = (mode7 == 0) ? 2 : 1;

      tile_blit_mode7(tm_x,
                      tm_y + sky_height / a,
                      tm_w,
                      tm_h - sky_height / a,
                      pos_x * tile_size,
                      pos_y * tile_size,
                      200,
                      v_theta * M_PI / 180.0f,
                      map_data,
                      tiles_data,
                      BLIT_NO_ALPHA,
                      fb_back);
      
      #if LCD_COLORDEPTH == 16
        color_t sky_color = rgb_col_888_565(0, 128, 180);
      #elif LCD_COLORDEPTH == 8
        color_t sky_color = rgb_col_888_332(0, 128, 180);
      #endif
      
      draw_rect_fill(tm_x, tm_y, tm_x + tm_w - 1, tm_y + (sky_height + 4) / a, sky_color, fb_back);

      #if LCD_COLORDEPTH == 16
        color_t alpha = 0xf81f;
      #elif LCD_COLORDEPTH == 8
        color_t alpha = 0xe3;      
      #endif

      if (mode7 == 0) {

        #if LCD_COLORDEPTH == 16
          color_t col = rgb_col_888_565(255, 255, 255);
        #elif LCD_COLORDEPTH == 8
          color_t col = rgb_col_888_332(255, 255, 255);
        #endif

        draw_rect_fill(lcd->get_screen_width() / 2 - 1, 0, lcd->get_screen_width() / 2 + 1, lcd->get_screen_height(), col, fb_back);
        draw_rect_fill(0, lcd->get_screen_height() / 2 - 1, lcd->get_screen_width(), lcd->get_screen_height() / 2 + 1, col, fb_back);
      }
    
    }

    fps_cnt++;
    if (millis() - fps_timer > 2000) {
      fps = fps_cnt * 1000 / (millis() - fps_timer);
      fps_cnt = 0;
      fps_timer = millis();
    }

    char debug_print[11];
    itoa(fps, debug_print, 10);
    font_write_string(11, 11, 0x00, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(10, 10, 0xff, debug_print, (font_t*) font_13x16, fb_back);

      itoa(pwr_get_bat_level(4 /* granularity */), debug_print, 10);
      font_write_string(11, 11 + 15, 0x0, debug_print, (font_t*) font_13x16, fb_back);
      font_write_string(10, 10 + 15, 0xffff, debug_print, (font_t*) font_13x16, fb_back);

      if (pwr_get_source() == PWR_ON_GRD) {
        font_write_string(11, 11 + 30, 0x0, "GRD", (font_t*) font_13x16, fb_back);
        font_write_string(10, 10 + 30, 0xffff, "GRD", (font_t*) font_13x16, fb_back);
      } else {
        font_write_string(11, 11 + 30, 0x0, "BAT", (font_t*) font_13x16, fb_back);
        font_write_string(10, 10 + 30, 0xffff, "BAT", (font_t*) font_13x16, fb_back);
      }

    #ifdef SOUND_EN
      if (music_on)
        snd_enqueue_buf(snd_music, SND_CHAN_0, SND_NONBLOCKING);
    #endif

#ifdef UART_ENABLE
    uart_puts(uart0, "A");
#endif


    lcd->enqueue_backbuffer();

    if (pe_cfg.lcd_refresh_mode == LCD_VRR_REFRESH)
      lcd->refresh();  // waits for vsync

    lcd->get_backbuffer(&fb_back);

    printf("laie\n");

  } // racing gam
}

void smileys() {
  // fps counter
  uint32_t fps_timer = millis();
  uint16_t fps = 0;
  uint16_t fps_cnt = 0;
  
  // get framebuffer
  gbuffer_t fb_back;
  lcd->get_backbuffer(&fb_back);

  // load palette
  color_palette_t custom_palette;
  lcd->get_palette_ptr(&custom_palette);

  // scale
  int scale = 3;
  uint32_t scale_timer = millis();

  for (int h = 0; h < 256; h++)
    custom_palette[h] = smiley_palette_data[h];

  // initialize sprite object values
  for (int h = 0; h < MAX_SMILEYS / 2; h++) {
    smiley[0][h].x = random(lcd->get_screen_width());
    smiley[0][h].y = random(lcd->get_screen_height());
    smiley[0][h].r = random(360);
    smiley[0][h].z = random(200) + 50;
    smiley[0][h].dz = random(7) - 3;
    smiley[0][h].dx = random(7) - 3;
    smiley[0][h].dy = random(7) - 3;
    smiley[0][h].dr = random(7) - 3;

    smiley[1][h].x = random(lcd->get_screen_width());
    smiley[1][h].y = random(lcd->get_screen_height());
    smiley[1][h].zx = random(200) + 50;
    smiley[1][h].zy = random(200) + 50;
    smiley[1][h].dx = random(7) - 3;
    smiley[1][h].dy = random(7) - 3;
    smiley[1][h].dzx = random(7) - 3;
    smiley[1][h].dzy = random(7) - 3;
  }
  
  int num_sprites = 200;

  uint32_t dbg_pr_timer = millis();
  
  uint16_t debounce_st = 0;

  while (1) {

#ifdef SOUND_EN
    snd_enqueue_buf(snd_music, SND_CHAN_0, SND_NONBLOCKING);
#endif

    ctrl_button_state_update();

    if (ctrl_is_pressed(BUTTON_R)) {
      lcd_backlight_set(0);
      pwr_shutdown();  
    }

    if (ctrl_is_pressed(BUTTON_ST) && !debounce_st) {
      if (num_sprites < MAX_SMILEYS)
        num_sprites += 10;
      if (num_sprites > MAX_SMILEYS)
        num_sprites = MAX_SMILEYS;
      debounce_st = 10;
    } else {
      if (debounce_st)
        debounce_st--;
    }

    if (ctrl_is_pressed(BUTTON_L))
      return;

    if (millis() - dbg_pr_timer > 1000) {
      dbg_pr_timer = millis();
      printf("Bat vol: %d\n", pwr_get_bat_vol());
      printf("Power src: %d\n", pwr_get_source());
      printf("A: %d\n", ctrl_is_pressed(BUTTON_A));
      printf("B: %d\n", ctrl_is_pressed(BUTTON_B));
      printf("X: %d\n", ctrl_is_pressed(BUTTON_X));
      printf("Y: %d\n", ctrl_is_pressed(BUTTON_Y));
      printf("L: %d\n", ctrl_is_pressed(BUTTON_L));
      printf("R: %d\n", ctrl_is_pressed(BUTTON_R));
      printf("ST: %d\n", ctrl_is_pressed(BUTTON_ST));
      printf("SEL: %d\n", ctrl_is_pressed(BUTTON_SEL));
      printf("ASW: %d\n", ctrl_is_pressed(BUTTON_SW));
      printf("LEFT: %d %d\n", ctrl_is_pressed(BUTTON_LEFT), analog_read(PIN_DPAD_LR));
      printf("RIGHT: %d %d\n", ctrl_is_pressed(BUTTON_RIGHT), analog_read(PIN_DPAD_LR));
      printf("UP: %d %d\n", ctrl_is_pressed(BUTTON_UP), analog_read(PIN_DPAD_UD));
      printf("DOWN: %d %d\n", ctrl_is_pressed(BUTTON_DOWN), analog_read(PIN_DPAD_UD));
      printf("ANA X: %d\n", analog_read(PIN_ANALOG_X));
      printf("ANA Y: %d\n", analog_read(PIN_ANALOG_Y));
      printf("PICO_PIO_USE_GPIO_BASE: %d\n", PICO_PIO_USE_GPIO_BASE);
      //printf("UD: %d\n", );
    }

//    if (pwr_get_source() == PWR_ON_BAT) {
    draw_rect_fill(0, 0, lcd->get_screen_width() - 1, lcd->get_screen_width() - 1, 0xff, fb_back);      

    // smiley 1 (the rotating, laughing one)
    int dx = 0;
    if (ctrl_is_pressed(BUTTON_LEFT))
      dx = -2;
    if (ctrl_is_pressed(BUTTON_RIGHT))
      dx = +2;
    
    int dy = 0;
    if (ctrl_is_pressed(BUTTON_UP))
      dy = -2;
    if (ctrl_is_pressed(BUTTON_DOWN))
      dy = +2;

    int dz = 0;
    if (ctrl_is_pressed(BUTTON_A))
      dz = -2;
    if (ctrl_is_pressed(BUTTON_B))
      dz = +2;

    int dr = 0;
    if (ctrl_is_pressed(BUTTON_X))
      dr = -2;
    if (ctrl_is_pressed(BUTTON_Y))
      dr = +2;
  
    smiley[0][0].x += dx; //smiley[0][0].dx;
    smiley[0][0].y += dy; // smiley[0][0].dy;
    smiley[0][0].z += dz; // smiley[0][0].dz;
    smiley[0][0].r += dr; // smiley[0][0].dr;

    if (smiley[0][0].r < 0)
      smiley[0][0].r += 360;
    if (smiley[0][0].r > 359)
      smiley[0][0].r -= 360;

#if 1
    for (int h = 1; h < num_sprites / 2; h++) {
      // smiley 1 (the rotating, laughing one)
      smiley[0][h].x += smiley[0][h].dx;
      smiley[0][h].y += smiley[0][h].dy;
      smiley[0][h].z += smiley[0][h].dz;
      smiley[0][h].r += smiley[0][h].dr;

      if ((smiley[0][h].x < 0) || smiley[0][h].x > lcd->get_screen_width() - 1)
        smiley[0][h].dx = -smiley[0][h].dx;
      if ((smiley[0][h].y < 0) || smiley[0][h].y > lcd->get_screen_height() - 1)
        smiley[0][h].dy = -smiley[0][h].dy;
      if ((smiley[0][h].z < 50) || smiley[0][h].z > 249)
        smiley[0][h].dz = -smiley[0][h].dz;
      if (smiley[0][h].r < 0)
        smiley[0][h].r += 360;
      if (smiley[0][h].r > 359)
        smiley[0][h].r -= 360;

      // rotation
      blit_buf(smiley[0][h].x,
               smiley[0][h].y,
               (float) smiley[0][h].z / 300.,
               (float) smiley[0][h].r * M_PI / 180.,
               TRANSPARENT_COLOR,
               smiley_data,
               fb_back);

      // smiley 2 (the other one being stretched)
      if ((smiley[1][h].x < 0) || smiley[1][h].x > lcd->get_screen_width() - 1)
        smiley[1][h].dx = -smiley[1][h].dx;
      if ((smiley[1][h].y < 0) || smiley[1][h].y > lcd->get_screen_height() - 1)
        smiley[1][h].dy = -smiley[1][h].dy;
      if ((smiley[1][h].zx + smiley[1][h].dzx < 50) || smiley[1][h].zx + smiley[1][h].dzx > 249)
        smiley[1][h].dzx = -smiley[1][h].dzx;
      if ((smiley[1][h].zy + smiley[1][h].dzy < 50) || smiley[1][h].zy + smiley[1][h].dzy > 249)
        smiley[1][h].dzy = -smiley[1][h].dzy;

      smiley[1][h].x += smiley[1][h].dx;
      smiley[1][h].y += smiley[1][h].dy;
      smiley[1][h].zx += smiley[1][h].dzx;
      smiley[1][h].zy += smiley[1][h].dzy;

      // x/y-zooming
      blit_buf(smiley[1][h].x,
               smiley[1][h].y,
               (float) smiley[1][h].zx / 500.,
               (float) smiley[1][h].zy / 500.,
               BLIT_FLIP_NONE,
               TRANSPARENT_COLOR,
               smiley2_data,
               fb_back);
    }
#endif

    // the big one you can control goes on top
    blit_buf(smiley[0][0].x,
              smiley[0][0].y,
              (float) smiley[0][0].z / 300.,
              (float) smiley[0][0].r * M_PI / 180.,
              TRANSPARENT_COLOR,
              smiley_data,
              fb_back);

    // frame rate counter
    fps_cnt++;
    if (millis() - fps_timer > 2000) {

      fps = fps_cnt * 1000 / (millis() - fps_timer);
      fps_cnt = 0;
      fps_timer = millis();
    }

    char debug_print[11];
    itoa(fps, debug_print, 10);

    font_write_string(10, 10, 0xff, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(9, 9, 0x00, debug_print, (font_t*) font_13x16, fb_back);
    //draw_line(0, 0, 0, lcd->get_screen_height() - 1, 150, fb_back);
    //draw_line(0, 0, lcd->get_screen_width() - 1, 0, 160, fb_back);

    char debug_print2[20] = "# of smileys: ";
    itoa(num_sprites, &debug_print2[14], 10);
    font_write_string(10, 30, 0xff, debug_print2, (font_t*) font_13x16, fb_back);
    font_write_string(9, 29, 0x00, debug_print2, (font_t*) font_13x16, fb_back);

    // Update screen
    lcd->enqueue_backbuffer();

    if (pe_cfg.lcd_refresh_mode == LCD_VRR_REFRESH)
      lcd->refresh();  // waits for vsync

    lcd->get_backbuffer(&fb_back);

#if 0
    if (millis() - scale_timer > 10 * 1000) {
      scale++;
      if (scale > 3)
        scale = 2;
      
      if (lcd->deinit() != LCD_SUCCESS)
        blink_error(27);
      if (lcd->init(pe_cfg.lcd_num_fb, pe_cfg.lcd_refresh_mode, scale) != LCD_SUCCESS)
        blink_error(69);

      // reload pallete
      for (int h = 0; h < 256; h++)
        custom_palette[h] = smiley_palette_data[h];

      scale_timer = millis();

      }
#endif
  }
} // smiley game

void reinit_lcd() {
      
  lcd_scale = lcd_scale == 2 ? 3 : 2;

//  lcd_backlight_set(0);

  printf("trying to change RES\n")  ;
  int ret = lcd->deinit();
  if (ret  != LCD_SUCCESS )
    blink_error(98);
  
  delete lcd;
  lcd = new LCD_DPI();

  printf("retinit\n")  ;
  ret = lcd->init(pe_cfg.lcd_num_fb, pe_cfg.lcd_refresh_mode, lcd_scale);
  if (ret  != LCD_SUCCESS)
    blink_error(99);

//  lcd_backlight_set(100);

}

void fps_counter() {
  uint32_t fps_cnt, fps_timer, fps, i;

  while (1) {
    i++;

    if (i > 255)
      i = 0;

    draw_rect_fill(0, 0, lcd->get_screen_width() - 1, lcd->get_screen_height() - 1, 0, fb_back);

    fps_cnt++;
    if (millis() - fps_timer > 2000) {
      fps = fps_cnt * 1000 / (millis() - fps_timer);
      fps_cnt = 0;
      fps_timer = millis();
    }

    char debug_print[11];
    itoa(i+5, debug_print, 10);
    font_write_string(10, 10, 0xff, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(9, 9, 0x0000, debug_print, (font_t*) font_13x16, fb_back);

    lcd->get_backbuffer(&fb_back);

    if (pe_cfg.lcd_refresh_mode == LCD_VRR_REFRESH)
      lcd->refresh();  // waits for vsync

  }  
}

int main() { 
  setup();

  lcd->get_backbuffer(&fb_back);

  while (1) {
  // Now send data over UART0
  //const char *msg = "Hello from UART on RP2350!\n";
  //uart_puts(uart0, msg);
    racing_game();
    while (1) {
      ctrl_button_state_update();
      if (!ctrl_is_pressed(BUTTON_L))
        break;
      if (pe_cfg.lcd_refresh_mode == LCD_VRR_REFRESH)
        lcd->refresh();  // waits for vsync
    }

    smileys();
    while (1) {
      ctrl_button_state_update();
      if (!ctrl_is_pressed(BUTTON_L))
        break;
      if (pe_cfg.lcd_refresh_mode == LCD_VRR_REFRESH)
        lcd->refresh();  // waits for vsync
    }

    reinit_lcd();
  }

} // loop()
