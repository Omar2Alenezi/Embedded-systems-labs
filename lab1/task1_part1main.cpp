/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"


// Blinking rate in milliseconds
#define BLINKING_RATE     500ms


int main()
{
    // Initialise the digital pin LED1 as an output
#ifdef LED1
    DigitalOut led1(LED1);
    #else
        bool led1;
        #endif

#ifdef LED2
    DigitalOut led2(LED2);
    #else
        bool led2;
        #endif

#ifdef LED3
    DigitalOut led3(LED3);
    #else
        bool led3;
    #endif

    while (true) {
        led1 = !led1;
        led2 = !led2;
        led3 = !led3;

        ThisThread::sleep_for(BLINKING_RATE);
    }
}
