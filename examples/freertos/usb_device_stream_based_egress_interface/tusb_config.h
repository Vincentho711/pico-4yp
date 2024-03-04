/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by board.mk
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

// File 1, modified 'CMakeLists.txt' of pico-sdk, changes shown in https://github.com/raspberrypi/pico-sdk/pull/1438
// File 2, self-modified 'family.cmake' in 'pico-sdk/lib/tinyusb/hw/bsp/rp2040/family.cmake'
// with line 'CFG_TUSB_OS=OPT_OS_PICO' in 'target_compile_definition(tinyusb_common_base ...)' commented out
// File 3, vanilla 'family_support.cmake' in 'pico-sdk/lib/tinyusb/hw/bsp/family_support.cmake

// Entry point is 'family_support.cmake' (file 3), it locates the family and navigate into
// the corresponding 'family.cmake' to compile the tinyusb_common_base lib. To compile this lib,
// the definition 'CFG_TUSB_OS' needs to be defined for the correct osal/ header files to be included
// This definition is passed down from line 'target_compile_definitions(tinyusb_common${SUFFIX} INTERFACE CFG_TUSB_OS=OPT_OS_${RTOS_UPPER}'
// of the modified 'CMakeLists.txt' (file 1). The tinyusb_common${SUFFIX} lib is then linked with tinyusb_common_base
// and the compile definition is passed on.
// In order for the compile definition is be passed on from higher lib, the line 'CFG_TUSB_OS=OPT_OS_PICO' of
// 'family.cmake' (file 2) needs to be commented out to avoid the higher compile definition being
// overriden by this line.
// Once the following has been done, the selection of compiling the pico or freertos
// version of tinyusb is done in the project 'CMakeLists.txt'. Using either
// 'tinyusb_{device | host | board}' or 'tinyusb_{device | host | board}_freertos' in 
// 'target_link_libraries()' of the project 'CMakelists.txt' to make the selection.
// When using the freertos version of tinyusb, the FreeRTOS lib needs to be linked after compilation.
// This is done by adding the line 'FreeRTOS-Kernel-Heap4' in the same 'target_link_libraries()'.

// The following '#ifndef CFG_TUSB_OS #define CFG_TUSB_OS' serves as a backup to pass the
// 'CFG_TUSB_OS' definition down to 'family_support.cmake' (file 3) if the logic in modified
// 'family.cmake' (file 1) fails to pass a compile definition down. With everything is properly set
// up, the following line is obsolete as 'CFG_TUSB_OS' should already be defined by 
// the modified 'family.cmake' (file 1). 
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- CLASS -------------//
#define CFG_TUD_CDC               1
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 1024)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 1024)

// CDC Endpoint transfer buffer size, more is faster
#define CFG_TUD_CDC_EP_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 1024)

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */