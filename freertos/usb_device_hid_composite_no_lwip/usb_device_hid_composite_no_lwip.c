/**
 * @file usb_device_hid_composite_freertos_no_lwip.c
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-01-16
 * 
 * @copyright NA
 * 
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"


/******************************************************
 * A demonstration of using TinyUSB to construct
 * a usb hid_composite device on a pico w running freertos with WiFi/lwip disabled. 
 * It is largely based on 
 * https://github.com/hathach/tinyusb/tree/master/examples/device/hid_composite_freertos
 * For this example to work, it will require some modification of the pico-sdk and tinyusb lib,
 * see comments in tusb_config.h and add in the changes in order to use 'tinyusb_device_freertos'
 * and 'tinyusb_board_freertos' in 'target_link_libraries()' of project 'CMakeLists.txt'.
 * The hid polling task is disabled as it seems to be having some 
 * freertos related issues, but the success criteria of this example is to
 * see the led blinking frequency changes when it is being mounted as a 
 * usb device. The pico w should be recognised as a TinyUSB device on host (i.e. laptop).
 * On linux/ macOS, if we open up a terminal, type 'ioreg -p IOUSB', we should
 * see something like 'TinyUSB Device@00130000' with the led blinking at 0.5Hz. 
 *****************************************************/

// 128*4 = 512 bytes
// (recommended min stack size for each task)
#define STACK_SIZE 128

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)

// Stack size for the HID task
#define HID_STACK_SIZE      configMINIMAL_STACK_SIZE

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

// LED blinky timer handler
TimerHandle_t blinky_tm = NULL;

void led_blinky_cb(TimerHandle_t xTimer);
static void usb_device_task(void *param);
static void hid_task(void *param);

int main()
{
    // Following line commented out as board_init() does a similar thing
    // stdio_init_all();
    board_init();

    // cyw43_arch_init() placed in main() when the cyw43_arch_none is used.
    // When cyw43_arch_lwip_sys_freertos is used, cyw43_arch_init() should be placed
    // in a task. A standard lwipopts.h with  '#define NO_SYS 0' should be supplied.
    // An extra define '#define configNUM_CORES configNUMBER_OF_CORES' in FreeRTOSConfig.h
    // is also required for pico-sdk compatability issues.
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }else
    {
        printf("Wi-F init!\n");
    }
    // Start with LED ON
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // Create a sw timer for blinky
    blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), pdTRUE, NULL, led_blinky_cb);
    assert(blinky_tm != NULL);

    // Create a task for tinyusb device stack task
    assert(xTaskCreate(usb_device_task, "usbd", 1024, NULL, configMAX_PRIORITIES-1, NULL) == pdPASS);

    // Create a task for hid polling task
    // Currently disabled as it seems to be causing issues with freertos
    // assert(xTaskCreate(hid_task, "hid", HID_STACK_SIZE, NULL, configMAX_PRIORITIES-2, NULL) == pdPASS);

    // Start sw timer
    assert(xTimerStart(blinky_tm, 0) == pdPASS);

    // Start scheduler
    vTaskStartScheduler();
    while (true) {};
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{

    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    if (tud_mounted())
    {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
    }else
    {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
    }
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
    //
    // skip if hid is not ready yet
    if ( !tud_hid_ready() ) return;

    switch(report_id)
    {
        case REPORT_ID_KEYBOARD:
        {
        // use to avoid send multiple consecutive zero report for keyboard
        static bool has_keyboard_key = false;

        if ( btn )
        {
            uint8_t keycode[6] = { 0 };
            keycode[0] = HID_KEY_A;

            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
            has_keyboard_key = true;
        }else
        {
            // send empty key report if previously has key pressed
            if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
            has_keyboard_key = false;
        }
        }
        break;

        case REPORT_ID_MOUSE:
        {
            int8_t const delta = 5;

            // no button, right + down, no scroll, no pan
            tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);
        }
        break;

        case REPORT_ID_CONSUMER_CONTROL:
        {
          // use to avoid send multiple consecutive zero report
          static bool has_consumer_key = false;
    
          if ( btn )
          {
            // volume down
            uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
            tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
            has_consumer_key = true;
          }else
          {
            // send empty key report (release key) if previously has key pressed
            uint16_t empty_key = 0;
            if (has_consumer_key) tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
            has_consumer_key = false;
          }
        }
        break;
    
        case REPORT_ID_GAMEPAD:
        {
          // use to avoid send multiple consecutive zero report for keyboard
          static bool has_gamepad_key = false;
    
          hid_gamepad_report_t report =
          {
            .x   = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
            .hat = 0, .buttons = 0
          };
    
          if ( btn )
          {
            report.hat = GAMEPAD_HAT_UP;
            report.buttons = GAMEPAD_BUTTON_A;
            tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
    
            has_gamepad_key = true;
          }else
          {
            report.hat = GAMEPAD_HAT_CENTERED;
            report.buttons = 0;
            if (has_gamepad_key) tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
            has_gamepad_key = false;
          }
        }
        break;
    
        default: break;
    }
}

//--------------------------------------------------------------------+
// HID task
//--------------------------------------------------------------------+
// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
static void hid_task(void *param)
{
    while (true)
    {
        // Poll every 10ms
        vTaskDelay(pdMS_TO_TICKS(10));
        uint32_t const btn = board_button_read();
        if ( tud_suspended() && btn )
        {
            // Wake up host if we are in suspend mode
            // and REMOTE_WAKEUP feature is enabled by host
            tud_remote_wakeup();
        }else
        {

            // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
            // Either the vTaskDelay() or the following line seems to be causing issues...
            send_hid_report(REPORT_ID_KEYBOARD, btn);
        }
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void) instance;
    (void) len;

    uint8_t next_report_id = report[0] + 1u;

    if (next_report_id < REPORT_ID_COUNT)
    {
        send_hid_report(next_report_id, board_button_read());
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
  
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
      // Set keyboard LED e.g Capslock, Numlock etc...
      if (report_id == REPORT_ID_KEYBOARD)
      {
        // bufsize should be (at least) 1
        if ( bufsize < 1 ) return;
  
        uint8_t const kbd_leds = buffer[0];
  
        if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
        {
          // Capslock On: disable blink, turn led on
          xTimerStop(blinky_tm, portMAX_DELAY);
          cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        }else
        {
          // Caplocks Off: back to normal blink
          cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
          xTimerStart(blinky_tm, portMAX_DELAY);
        }
      }
    }
}

//--------------------------------------------------------------------+
// TinyUSB device stack task
//--------------------------------------------------------------------+
static void usb_device_task(void *param)
{
    (void) param;

    // If the freertos version of cyw43_arch is ran, the init has to be in a freertos task
    /*
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }else
    {
        printf("Wi-F init!\n");
    }
    */

    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    // We don't need to init more stuff after tud_init()

    while (true)
    {
        // Put tud_task() in the waiting state until new event
        tud_task();
    }
}

//--------------------------------------------------------------------+
// Blinky timer callback
//--------------------------------------------------------------------+
void led_blinky_cb(TimerHandle_t xTimer)
{
    (void) xTimer;
    static bool led_state = false;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
    printf("LED: %d\n", led_state);
    led_state = 1 - led_state;
}