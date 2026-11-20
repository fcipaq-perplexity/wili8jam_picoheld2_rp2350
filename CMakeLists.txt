cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Board: Pico Held 2 (RP2350B), see boards/picoheld_2.h ---
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards)
set(PICO_PLATFORM rp2350)
set(PICO_BOARD picoheld_2 CACHE STRING "Board type")

# 0 = Pico Held 2 v1.5 (SunmaxIC panel), 1 = v1.4 (Buydisplay panel)
set(PICOHELD2_HW_VARIANT 0 CACHE STRING "Pico Held 2 hardware variant (0 = v1.5 SunmaxIC, 1 = v1.4 Buydisplay)")
set_property(CACHE PICOHELD2_HW_VARIANT PROPERTY STRINGS 0 1)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

include(pico_sdk_import.cmake)

project(wili8jam C CXX ASM)

pico_sdk_init()

# Board variant is visible to every target (board header + Pico Engine hwcfg.h)
add_compile_definitions(
    PICOHELD2_HW_VARIANT=${PICOHELD2_HW_VARIANT}
    PE_CONFIG_NO=${PICOHELD2_HW_VARIANT}
)

# --- Lua 5.4.7 static library ---
set(LUA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4.7/src)

file(GLOB LUA_CORE_SOURCES ${LUA_SRC_DIR}/*.c)
# Remove standalone program mains
list(REMOVE_ITEM LUA_CORE_SOURCES
    ${LUA_SRC_DIR}/lua.c
    ${LUA_SRC_DIR}/luac.c
)

add_library(lua54 STATIC ${LUA_CORE_SOURCES})

target_include_directories(lua54 PUBLIC ${LUA_SRC_DIR})

# luaconf overrides for embedded use (luaconf.h has #ifndef guards for these)
target_compile_definitions(lua54 PUBLIC
    LUA_32BITS=1
    LUAI_MAXSTACK=2000
)
target_compile_definitions(lua54 PRIVATE
    LUA_USE_C89
)

# Ensure Lua is built as C
set_target_properties(lua54 PROPERTIES LINKER_LANGUAGE C)

# --- TLSF allocator (vendored) ---
add_library(tlsf STATIC tlsf/tlsf.c)
target_include_directories(tlsf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- FatFS library (SD card) ---
add_library(fatfs STATIC
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/ffsystem.c
    fatfs/diskio.c
)
target_include_directories(fatfs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/fatfs
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(fatfs PUBLIC pico_stdlib hardware_spi)

# --- Pico Engine hardware drivers (picoengine/, used unmodified) ---
# Only the parts wili8jam needs: DPI LCD, backlight, buttons, power.
# (picoengine/CMakeLists.txt is not used: it pulls in a 2nd FatFS and the MCU LCD drivers.)
set(PE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/picoengine/src)

add_library(picoengine_hw STATIC
    ${PE_DIR}/hardware/lcd/lcd_dpi/lcd_dpi.cpp
    ${PE_DIR}/hardware/lcd/backlight/backlight.cpp
    ${PE_DIR}/hardware/controls/controls.cpp
    ${PE_DIR}/hardware/power/power.cpp
)
target_include_directories(picoengine_hw PUBLIC
    ${PE_DIR}
    ${PE_DIR}/fonts
    ${PE_DIR}/graphics
    ${PE_DIR}/hardware
    ${PE_DIR}/hardware/lcd
    ${PE_DIR}/hardware/controls
    ${PE_DIR}/utils
)
foreach(PIO_SRC
        lcd_dpi_pio_scanout_3x
        lcd_dpi_pio_scanout_2x
        lcd_dpi_pio_addr_dec
        lcd_dpi_pio_addr_dec_8bit
        lcd_dpi_pio_clut)
    pico_generate_pio_header(picoengine_hw ${PE_DIR}/hardware/lcd/lcd_dpi/${PIO_SRC}.pio)
endforeach()
target_compile_options(picoengine_hw PRIVATE -O3)
target_link_libraries(picoengine_hw PUBLIC
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_clocks
    hardware_resets
    hardware_sync
)

# --- Main executable ---
add_executable(wili8jam
    src/main.cpp
    src/held_hw.cpp
    src/sdcard.c
    src/gfx.c
    src/input.c
    src/audio.c
    src/p8_preprocess.c
    src/p8_api.c
    src/p8_sfx.c
    src/p8_cart.c
    src/p8_png.c
    src/p8_console.c
)

# Generate PIO header for I2S audio
pico_generate_pio_header(wili8jam ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_i2s.pio)

target_include_directories(wili8jam PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LUA_SRC_DIR}
)

target_link_libraries(wili8jam PRIVATE
    lua54
    tlsf
    fatfs
    picoengine_hw
    pico_stdlib
    hardware_pio
    hardware_spi
    hardware_sync
    hardware_dma
    hardware_clocks
    hardware_watchdog
)

# Increase stack size — default 0x800 (2KB) is too small for Lua + FatFS
# Lua call chain can be deep; FIL structs now static to save stack space
target_compile_definitions(wili8jam PRIVATE PICO_STACK_SIZE=0x1000)

# USB CDC serial for debug output + REPL (no UART: its pins are shared with the SD card)
pico_enable_stdio_usb(wili8jam 1)
pico_enable_stdio_uart(wili8jam 0)

pico_add_extra_outputs(wili8jam)

# --- Hardware bring-up test (display, colours, scaling, buttons, audio) ---
add_executable(picoheld2_hwtest
    src/hwtest.c
    src/held_hw.cpp
    src/gfx.c
    src/input.c
    src/audio.c
    src/p8_sfx.c
)
pico_generate_pio_header(picoheld2_hwtest ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_i2s.pio)
target_include_directories(picoheld2_hwtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LUA_SRC_DIR}
)
target_link_libraries(picoheld2_hwtest PRIVATE
    lua54
    picoengine_hw
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_watchdog
)
pico_enable_stdio_usb(picoheld2_hwtest 1)
pico_enable_stdio_uart(picoheld2_hwtest 0)
pico_add_extra_outputs(picoheld2_hwtest)
