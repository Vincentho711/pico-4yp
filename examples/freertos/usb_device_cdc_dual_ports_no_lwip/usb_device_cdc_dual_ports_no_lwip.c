/**
 * @file usb_device_cdc_dual_ports_no_lwip.c
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-01-18
 * 
 * @copyright NA
 * 
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include <ctype.h> // Needed for islower() and isupper()

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "bsp/board.h"
#include "tusb.h"

/******************************************************
 * A demonstration of using TinyUSB to construct
 * a usb device with 2 CDC devices echoing each other on a pico w 
 * running freertos with WiFi/lwip disabled. 
 * It is largely based on 
 * https://github.com/hathach/tinyusb/tree/master/examples/device/cdc_dual_ports
 * but it's modified to run on FreeRTOS.
 * For this example to work, it will require some modification of the pico-sdk and tinyusb lib,
 * see comments in tusb_config.h and add in the changes in order to use 'tinyusb_device_freertos'
 * and 'tinyusb_board_freertos' in 'target_link_libraries()' of project 'CMakeLists.txt'.
 * The pico w should be recognised as a TinyUSB device on host (i.e. laptop).
 * On linux/ macOS, if we open up a terminal, type 'ioreg -p IOUSB', we should
 * see something like 'TinyUSB Device@00130000' with the led blinking at 0.5Hz. 
 * You should see 2 CDC devices on the serial console, input into one serial port will be
 * echoed back to both serial ports.
 *****************************************************/

// 128*4 = 512 bytes
// (recommended min stack size for each task)
#define STACK_SIZE         128


// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)

// Stack size for the CDC task
#define CDC_STACK_SIZE     configMINIMAL_STACK_SIZE

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
static void cdc_task(void *param);

int main(void)
{
    board_init();
  
    if (cyw43_arch_init())
    {
      printf("Wi-Fi init failed.\n");
    }else
    {
      printf("Wi-Fi init suceeded.\n");
    }
    // Start with LED ON
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  
    // Create a sw timer for blinky
    blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), pdTRUE, NULL, led_blinky_cb);
    assert (blinky_tm != NULL);
  
    // Create a task for tinyusb device stack task
    assert(xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, NULL) == pdPASS);

    // Create a task for tinyusb cdc task
    assert(xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES-2, NULL) == pdPASS);

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
// Local functions
//--------------------------------------------------------------------+

// echo to either Serial0 or Serial1
// with Serial0 as all lower case, Serial1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
    uint8_t const case_diff = 'a' - 'A';
  
    for(uint32_t i=0; i<count; i++)
    {
        if (itf == 0)
        {
            // echo back 1st port as lower case
            if (isupper(buf[i])) buf[i] += case_diff;
        }
        else
        {
            // echo back 2nd port as upper case
            if (islower(buf[i])) buf[i] -= case_diff;
        }
  
        tud_cdc_n_write_char(itf, buf[i]);
    }
    tud_cdc_n_write_flush(itf);
}

//--------------------------------------------------------------------+
// TinyUSB device stack task
//--------------------------------------------------------------------+
static void usb_device_task(void *param)
{
    (void) param;
    tud_init(BOARD_TUD_RHPORT);

    while (true)
    {
        tud_task();
    }
}

//--------------------------------------------------------------------+
// USB CDC Task
//--------------------------------------------------------------------+
static void cdc_task(void *param)
{
    while (true)
    {
        uint8_t itf;

        for (itf = 0; itf < CFG_TUD_CDC; itf++)
        {
            // connected() check for DTR bit
            // Most but not all terminal client set this when making connection
            if ( tud_cdc_n_connected(itf) )
            {
                if ( tud_cdc_n_available(itf) )
                {
                    uint8_t buf[64];

                    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

                    // echo back to both serial ports
                    echo_serial_port(0, buf, count);
                    echo_serial_port(1, buf, count);
                }
            }
        }
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