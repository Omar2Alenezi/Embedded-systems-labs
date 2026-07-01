#include "mbed.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);

#define BLINK_ON_MS   100
#define BLINK_OFF_MS  100
#define BLINK_COUNT   4

void blink_then_hold() {

    // ── PHASE 1: all three LEDs blink 4 times ──────────────────────
    for (int i = 0; i < BLINK_COUNT; i++) {
        // ON
        led1 = 1;
        led2 = 1;
        led3 = 1;
        thread_sleep_for(BLINK_ON_MS);   // 100 ms on

        // OFF
        led1 = 0;
        led2 = 0;
        led3 = 0;
        thread_sleep_for(BLINK_OFF_MS);  // 100 ms off
    }
    // After loop: all LEDs are OFF, 4 blinks completed

    // ── PHASE 2: LED1 on permanently, LED2 & LED3 stay off ─────────
    led2 = 0;   // ensure off (already off, but explicit is safer)
    led3 = 0;

    led1 = 1;   // turn LED1 on and hold it there

    while (true) {
        // Do nothing — LED1 stays on indefinitely
        // Use a long sleep to avoid burning CPU in a tight loop
        thread_sleep_for(1000);
    }
}

int main() {
    blink_then_hold();
}