#include "mbed.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);

#define TICK_MS 250   // base resolution — must divide into all periods

void multi_rate_blink() {
    int counter_led1 = 0;
    int counter_led2 = 0;
    int counter_led3 = 0;

    while (true) {
        thread_sleep_for(TICK_MS);

        counter_led1 += TICK_MS;
        counter_led2 += TICK_MS;
        counter_led3 += TICK_MS;

        // LED1: toggle every 1000 ms
        if (counter_led1 >= 1000) {
            led1 = !led1;
            counter_led1 = 0;
        }

        // LED2: toggle every 500 ms (2x faster than LED1)
        if (counter_led2 >= 500) {
            led2 = !led2;
            counter_led2 = 0;
        }

        // LED3: toggle every 500 ms (2x faster than LED1)
        if (counter_led3 >= 500) {
            led3 = !led3;
            counter_led3 = 0;
        }
    }
}

int main() {
    multi_rate_blink();
}