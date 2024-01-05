/**
 * @file blink.cpp
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-1-5
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <iostream>

static void led_task(void*)
{   
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }
    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        std::cout << "Hello, world! :):)" << std::endl;
        vTaskDelay(100);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(100);
    }
}

int main() {
    stdio_init_all();

    xTaskCreate(led_task, "LED_Task", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    while (true) {};
}