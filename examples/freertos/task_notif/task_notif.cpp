/**
 * @file sw_timer.cpp
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-1-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include <stdio.h>
#include <iostream>

/*********************************************
 * A simple demonstration of using task
 * notifications in place of queues
 * Compared to queues, task notif:
 * Can only pass a 32-bit integer
 * Do not offer a mean of waiting to push a
 * notif to a busy task. It will overwrite or
 * return immediately without writing
 * Can only be used with only one receiver
 * Faster
 *********************************************/

// 128*4 = 512 bytes
// (recommended min stack size for each task)
#define STACK_SIZE 256

void recvTask ( void* NotUsed );
void sendingTask ( void* NotUsed );

// This task handle will be used to send direct task notif to recvTask
static TaskHandle_t recvTaskHandle = NULL;

int main() {
    BaseType_t retval;
    stdio_init_all();
    printf("Hello world.");
    std::cout << "Hello world." << std::endl;
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }else
    {
        std::cout << "Wi-Fi init success" << std::endl;
    }

    retval = xTaskCreate(recvTask, "recvTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &recvTaskHandle);
    assert(retval == pdPASS);
    assert(recvTaskHandle != NULL);
    retval = xTaskCreate(sendingTask, "sendingTask", STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    assert (retval == pdPASS);

    vTaskStartScheduler();
    while (true) {};
} 

/**
 * @brief This receive task watches a task notification, and receive it when it exists 
 * 
 * @param NotUsed 
 */
void recvTask ( void* NotUsed)
{
    while (true)
    {
        // Wait for the next notification value
        uint32_t notificationvalue = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        // uint32_t notificationvalue = xTaskNotifyWait( pdTRUE, portMAX_DELAY );
        std::cout << "NotifVal = " << notificationvalue << std::endl;
        if ((notificationvalue == 0U))
        {
            std::cout << "Turning LED off." << std::endl;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
        if ((notificationvalue == 1U))
        {
            std::cout << "Turning LED on." << std::endl;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        }
        if ((notificationvalue != 0U || notificationvalue != 1U))
        {
            std::cout << "Unknown notif val." << std::endl;
        }
    }
}
/**
 * @brief sendingTask pushing a different notifVal to the recvTask,
 * cycling between them.
 * 
 * @param NotUsed 
 */
void sendingTask ( void* NotUsed )
{
    while (true)
    {
        // Sending a notif to recvTask - tuning on LED
        // Since we are overwriting the value, even if another notif is pending
        // There is no need to check the return value
        xTaskNotify( recvTaskHandle, 0U, eSetValueWithOverwrite);
        vTaskDelay(200);

        // Turn off the LED
        xTaskNotify( recvTaskHandle, 1U, eSetValueWithOverwrite);
        vTaskDelay(1000);

        // Send undefined notifVal, 2U, nothing should happen other than printf
        xTaskNotify( recvTaskHandle, 2U, eSetValueWithOverwrite);
        vTaskDelay(1000);
    }
}