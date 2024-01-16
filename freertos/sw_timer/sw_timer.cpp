/**
 * @file sw_timer.cpp
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-1-12
 * 
 * @copyright NA
 * 
 */


/*********************************************
 * A simple demonstration of software timers
 * in FreeRTOS. A oneshot and a repeated timer are created.
 * Upon expiration, their corresponding callback toggles the
 * the onboard LED of the pico w.
 *********************************************/

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include <stdio.h>
#include <iostream>

void oneShotCallBack( TimerHandle_t xTimer);
void repeatCallBack( TimerHandle_t xTimer);

int main() {
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }
    // Start with LED ON
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    // Create a one shot timer
    TimerHandle_t oneShotHandle = xTimerCreate(
        "myOneShotTimer",          // Name for timer
        2200 / portTICK_PERIOD_MS, // period of timer in ticks
        pdFALSE,                   // Auto reload flag
        NULL,                      // Unique ID for timer
        oneShotCallBack            // CallBack function
    );
    assert(oneShotHandle != NULL);

    // Create a repeat timer
    TimerHandle_t repeatHandle = xTimerCreate(
        "myRepeatedTimer",         // Name for timer
        500 / portTICK_PERIOD_MS,  // Period of timer in ticks
        pdTRUE,                    // Auto reload flag
        NULL,                      // Unique ID for timer
        repeatCallBack             // CallBack function 
    );
    assert(repeatHandle != NULL);

    // Start timer
    xTimerStart(oneShotHandle, 0);
    xTimerStart(repeatHandle, 0);
    
    vTaskStartScheduler();
    while (true) {};
}

void oneShotCallBack ( TimerHandle_t xTimer)
{
    std::cout << "OneShot timer expired." << std::endl;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

void repeatCallBack( TimerHandle_t xTimer)
{
    static uint8_t counter = 0;
    std::cout << "Repeat timer expired. Toggling LED." << std::endl;
    if (counter++ % 2)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }else
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }

}