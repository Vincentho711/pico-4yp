/**
 * @file blink.cpp
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-1-5
 * 
 * @copyright NA
 * 
 */

/*********************************************
 * A simple demonstration of blinking LED and semaphores in FreeRTOS.
 * LED blinks and a semaphore is given
 * every 5 blinks. Another task takes the semaphore and prints
 * a response on stdio.
 *********************************************/

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <stdio.h>
#include <iostream>

static SemaphoreHandle_t semPtr = NULL;

static void led_task(void*)
{   
    uint_fast8_t count = 0;
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }
    while (true) {
        // Every 5 times through the loop, give the semaphore
        if (++count >= 5)
        {
            count = 0;
            std::cout << "Giving semPtr!" << std::endl;
            xSemaphoreGive(semPtr);
        }
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(500);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(500);
    }
}

void usb_task(void *pvParameters)
{
    while (true)
    {
        if (xSemaphoreTake(semPtr, portMAX_DELAY) == pdPASS)
        {
            std::cout << "Semaphore taken!" << std::endl;
            vTaskDelay(100);
        }
    }
}

int main() {
    BaseType_t retval;
    stdio_init_all();

    // Create a semaphore using the FreeRTOS Heap
    semPtr = xSemaphoreCreateBinary();
    // Ensuire pointer is valid
    assert(semPtr != NULL);

    retval = xTaskCreate(led_task, "LED_Task", 256, NULL, 1, NULL);
    assert (retval == pdPASS);
    retval = xTaskCreate(usb_task, "USB_Task", 256, NULL, 1, NULL);
    assert (retval == pdPASS);
    
    vTaskStartScheduler();
    while (true) {};
}