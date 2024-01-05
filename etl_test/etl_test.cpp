/**
 * @file etl_test.cpp
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief A test to output content loaded into etl vector to usb
 * @version 0.1
 * @date 2023-10-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "etl/vector.h"
#include "etl/numeric.h"
#include <stdio.h>
#include <iostream>
#include <iterator>
#include <algorithm>

/**
 * @brief This function prints the contents of etl::vector
 * 
 * @param v The vector object to be printed
 */
void print_etl_vec(const etl::ivector<int> &v)
{
    std::copy(v.begin(),v.end(),std::ostream_iterator<int>(std::cout, " "));
    std::cout << "\n";
}

int main() {
    stdio_init_all();

    // Declare the vector instances
    etl::vector<int, 10> v1(10);
    etl::vector<int, 20> v2(20);

    // Fill with 0 to 9
    etl::iota(v1.begin(), v1.end(), 0);
    // Fill with 10 to 29
    etl::iota(v2.begin(), v2.end(), 10);

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return -1;
    }
    while (true) {
        print_etl_vec(v1);
        print_etl_vec(v2);
        sleep_ms(1000);
    }
}