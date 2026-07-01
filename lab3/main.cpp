#include "mbed.h"
#include "arm_book_lib.h"

// ── Pin Definitions ──────────────────────────────────────────
DigitalIn  aButton(D4);
DigitalIn  bButton(D5);
DigitalIn  cButton(D6);
DigitalIn  enterButton(D7);        // D = confirm/enter digit

DigitalOut alarmLed(LED1);         // Lockdown: steady ON
DigitalOut incorrectCodeLed(LED3); // Incorrect entry flash
DigitalOut systemBlockedLed(LED2); // Warning blink / lockdown blink

// ── Passcode Configuration ───────────────────────────────────
// Codes stored as 3-digit arrays: each digit = 1(A), 2(B), or 3(C)
const int USER_CODE[3]  = {1, 2, 3};   // correct user code: A, B, C
const int ADMIN_CODE[3] = {3, 3, 1};   // admin override: C, C, A

// ── System States (without enum) ─────────────────────────────
const int STATE_NORMAL   = 0;
const int STATE_WARNING  = 1;
const int STATE_LOCKDOWN = 2;

// ── Helper: Read One Button Press (A=1, B=2, C=3) ────────────
int read_digit()
{
    while (true) {
        if (aButton) {
            while (aButton) {}
            return 1;
        }
        if (bButton) {
            while (bButton) {}
            return 2;
        }
        if (cButton) {
            while (cButton) {}
            return 3;
        }
        thread_sleep_for(10);
    }
}

// ── Helper: Read Full 3-Digit Code ───────────────────────────
bool read_code(int* enteredCode)
{
    for (int i = 0; i < 3; i++) {
        enteredCode[i] = read_digit();

        // Acknowledge digit received
        incorrectCodeLed = ON;
        thread_sleep_for(100);
        incorrectCodeLed = OFF;
    }

    int timeout = 5000; // 5 seconds to press Enter

    while (timeout > 0) {
        if (enterButton) {
            while (enterButton) {}
            return true;
        }

        thread_sleep_for(10);
        timeout -= 10;
    }

    return false;
}

// ── Helper: Compare Codes ────────────────────────────────────
bool codes_match(const int* a, const int* b)
{
    return (a[0] == b[0] &&
            a[1] == b[1] &&
            a[2] == b[2]);
}

// ── Warning Mode ─────────────────────────────────────────────
void run_warning_mode()
{
    systemBlockedLed = OFF;

    int elapsed = 0;

    while (elapsed < 10000) {

        systemBlockedLed = ON;
        thread_sleep_for(500);

        systemBlockedLed = OFF;
        thread_sleep_for(500);

        elapsed += 1000;
    }

    systemBlockedLed = OFF;
}

// ── Lockdown Mode ────────────────────────────────────────────
bool run_lockdown_mode()
{
    alarmLed = ON;
    systemBlockedLed = OFF;

    int elapsed = 0;
    int blinkTimer = 0;

    while (elapsed < 120000) {

        blinkTimer += 50;

        if (blinkTimer >= 1000) {
            systemBlockedLed = !systemBlockedLed;
            blinkTimer = 0;
        }

        if (enterButton) {

            while (enterButton) {}

            int enteredCode[3] = {3, 3, 1};

            bool confirmed = read_code(enteredCode);

            if (confirmed &&
                codes_match(enteredCode, ADMIN_CODE)) {

                alarmLed = OFF;
                systemBlockedLed = OFF;
                incorrectCodeLed = OFF;

                return true;
            }
        }

        thread_sleep_for(50);
        elapsed += 50;
    }

    // Continue indefinitely until admin override
    while (true) {

        systemBlockedLed = !systemBlockedLed;
        thread_sleep_for(1000);

        if (enterButton) {

            while (enterButton) {}

            int enteredCode[3] = {0, 0, 0};

            bool confirmed = read_code(enteredCode);

            if (confirmed &&
                codes_match(enteredCode, ADMIN_CODE)) {

                alarmLed = OFF;
                systemBlockedLed = OFF;
                incorrectCodeLed = OFF;

                return true;
            }
        }
    }
}

// ── Main ─────────────────────────────────────────────────────
int main()
{
    // Configure pull-down resistors
    aButton.mode(PullDown);
    bButton.mode(PullDown);
    cButton.mode(PullDown);
    enterButton.mode(PullDown);

    // Initialise LEDs OFF
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;

    int state = STATE_NORMAL;

    int incorrectCount = 0;
    bool warningTriggered = false;

    while (true) {

        // ── NORMAL STATE ──────────────────────────────────────
        if (state == STATE_NORMAL) {

            if (enterButton) {

                while (enterButton) {}

                int enteredCode[3] = {0, 0, 0};

                bool confirmed = read_code(enteredCode);

                if (!confirmed) {
                    continue;
                }

                if (codes_match(enteredCode, USER_CODE)) {

                    // Access granted
                    incorrectCodeLed = OFF;
                    incorrectCount = 0;
                    warningTriggered = false;

                    alarmLed = ON;
                    thread_sleep_for(500);
                    alarmLed = OFF;

                } else {

                    // Incorrect code
                    incorrectCodeLed = ON;
                    thread_sleep_for(300);
                    incorrectCodeLed = OFF;

                    incorrectCount++;

                    if (incorrectCount >= 2 &&
                        !warningTriggered) {

                        state = STATE_WARNING;
                        warningTriggered = true;

                    } else if (incorrectCount >= 3 &&
                               warningTriggered) {

                        state = STATE_LOCKDOWN;
                    }
                }
            }
        }

        // ── WARNING STATE ─────────────────────────────────────
        else if (state == STATE_WARNING) {

            run_warning_mode();

            state = STATE_NORMAL;

            // incorrectCount retained intentionally
        }

        // ── LOCKDOWN STATE ────────────────────────────────────
        else if (state == STATE_LOCKDOWN) {

            bool adminReset = run_lockdown_mode();

            if (adminReset) {

                state = STATE_NORMAL;
                incorrectCount = 0;
                warningTriggered = false;
            }
        }

        thread_sleep_for(10);
    }
}