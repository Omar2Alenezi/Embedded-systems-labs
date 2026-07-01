//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <string.h>
#include <stdio.h>

//=====[Defines]===============================================================

#define NUMBER_OF_KEYS                           3   // 3-digit code
#define BLINKING_TIME_GAS_ALARM               1000
#define BLINKING_TIME_OVER_TEMP_ALARM          500
#define BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM  100
#define NUMBER_OF_AVG_SAMPLES                   100
#define TIME_INCREMENT_MS                        100
#define DEBOUNCE_KEY_TIME_MS                     40
#define KEYPAD_NUMBER_OF_ROWS                     4
#define KEYPAD_NUMBER_OF_COLS                     4
#define EVENT_MAX_STORAGE                       100
#define EVENT_NAME_MAX_LENGTH                    20  // wider: stores "TEMP:XXX.XX C"

// Potentiometer maps to 25°C – 37°C
#define OVER_TEMP_MIN_C                        25.0f
#define OVER_TEMP_MAX_C                        37.0f

//=====[Declaration of public data types]======================================

typedef enum {
    MATRIX_KEYPAD_SCANNING,
    MATRIX_KEYPAD_DEBOUNCE,
    MATRIX_KEYPAD_KEY_HOLD_PRESSED
} matrixKeypadState_t;

typedef struct systemEvent {
    time_t seconds;
    char typeOfEvent[EVENT_NAME_MAX_LENGTH];
} systemEvent_t;

//=====[Declaration and initialization of public global objects]===============

DigitalIn alarmTestButton(BUTTON1);
DigitalIn mq2(PE_12);

DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

DigitalInOut sirenPin(PE_10);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn lm35(A1);
AnalogIn potentiometer(A0);          //  reads threshold

DigitalOut keypadRowPins[KEYPAD_NUMBER_OF_ROWS] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn  keypadColPins[KEYPAD_NUMBER_OF_COLS]  = {PB_12, PB_13, PB_15, PC_6};

//=====[Declaration and initialization of public global variables]=============

bool alarmState       = OFF;
bool incorrectCode    = false;
bool overTempDetector = OFF;

int  numberOfIncorrectCodes        = 0;
int  numberOfHashKeyReleasedEvents = 0;
int  keyBeingCompared              = 0;

// 3-digit code (default "1 8 0")
char codeSequence[NUMBER_OF_KEYS] = { '1', '8', '0' };
char keyPressed[NUMBER_OF_KEYS]   = { '0', '0', '0' };

int accumulatedTimeAlarm = 0;

bool alarmLastState = OFF;
bool gasLastState   = OFF;
bool tempLastState  = OFF;
bool ICLastState    = OFF;
bool SBLastState    = OFF;

bool gasDetectorState      = OFF;
bool overTempDetectorState = OFF;

float potReading          = 0.0f;
float overTempLevelC      = OVER_TEMP_MIN_C;   //  dynamic threshold
float overTempLevelC_last = -1.0f;             //  detect threshold changes

float lm35ReadingsAverage = 0.0f;
float lm35ReadingsSum     = 0.0f;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float lm35TempC           = 0.0f;

int  accumulatedDebounceMatrixKeypadTime = 0;
int  matrixKeypadCodeIndex               = 0;
char matrixKeypadLastKeyPressed          = '\0';
char matrixKeypadIndexToCharArray[]      = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D',
};
matrixKeypadState_t matrixKeypadState;

int           eventsIndex = 0;
systemEvent_t arrayOfStoredEvents[EVENT_MAX_STORAGE];

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();

void alarmActivationUpdate();
void alarmDeactivationUpdate();

void uartTask();
void availableCommands();
bool areEqual();

void eventLogUpdate();
void systemElementStateUpdate( bool lastState,
                               bool currentState,
                               const char* elementName );
void logTemperatureEvent( float tempC );      
void printEventLog();                         
void printThresholdUpdate( float threshC );   

float celsiusToFahrenheit( float tempInCelsiusDegrees );
float analogReadingScaledWithTheLM35Formula( float analogReading );
void  lm35ReadingsArrayInit();

void matrixKeypadInit();
char matrixKeypadScan();
char matrixKeypadUpdate();

//=====[Main function]=========================================================

int main()
{
    inputsInit();
    outputsInit();
    while (true) {
        alarmActivationUpdate();
        alarmDeactivationUpdate();
        uartTask();
        eventLogUpdate();
        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    lm35ReadingsArrayInit();
    alarmTestButton.mode(PullDown);
    sirenPin.mode(OpenDrain);
    sirenPin.input();
    matrixKeypadInit();
}

void outputsInit()
{
    alarmLed         = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}

// ----------------------------------------------------------------------------
// alarmActivationUpdate
// reads potentiometer → overTempLevelC (25–37°C), prints when it changes
// ----------------------------------------------------------------------------
void alarmActivationUpdate()
{
    static int lm35SampleIndex = 0;
    int i = 0;

    // LM35 rolling average 
    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex++;
    if ( lm35SampleIndex >= NUMBER_OF_AVG_SAMPLES ) lm35SampleIndex = 0;

    lm35ReadingsSum = 0.0f;
    for ( i = 0; i < NUMBER_OF_AVG_SAMPLES; i++ )
        lm35ReadingsSum += lm35ReadingsArray[i];
    lm35ReadingsAverage = lm35ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    lm35TempC = analogReadingScaledWithTheLM35Formula( lm35ReadingsAverage );

    //  potentiometer sets threshold 25–37°C 
    potReading     = potentiometer.read();
    overTempLevelC = OVER_TEMP_MIN_C + potReading * (OVER_TEMP_MAX_C - OVER_TEMP_MIN_C);

    // Print threshold on serial whenever it changes by ≥0.5°C
    if ( (overTempLevelC - overTempLevelC_last) >  0.5f ||
         (overTempLevelC - overTempLevelC_last) < -0.5f ) {
        overTempLevelC_last = overTempLevelC;
        printThresholdUpdate( overTempLevelC );
    }

    // Temperature alarm 
    if ( lm35TempC > overTempLevelC ) {
        overTempDetector = ON;
    } else {
        overTempDetector = OFF;
    }

    // Gas / over-temp / test-button activation
    if ( !mq2 ) {
        gasDetectorState = ON;
        alarmState       = ON;
    }
    if ( overTempDetector ) {
        overTempDetectorState = ON;
        alarmState            = ON;
    }
    if ( alarmTestButton ) {
        overTempDetectorState = ON;
        gasDetectorState      = ON;
        alarmState            = ON;
    }

    //  show deactivation prompt once while alarm is active 
    static bool promptShown = false;
    if ( alarmState && !promptShown ) {
        uartUsb.write( "Enter 3-Digit Code to Deactivate\r\n", 34 );
        promptShown = true;
    }
    if ( !alarmState ) {
        promptShown = false;
    }

    // Buzzer + LED blinking 
    if ( alarmState ) {
        accumulatedTimeAlarm += TIME_INCREMENT_MS;
        sirenPin.output();
        sirenPin = LOW;

        if ( gasDetectorState && overTempDetectorState ) {
            if ( accumulatedTimeAlarm >= BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if ( gasDetectorState ) {
            if ( accumulatedTimeAlarm >= BLINKING_TIME_GAS_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if ( overTempDetectorState ) {
            if ( accumulatedTimeAlarm >= BLINKING_TIME_OVER_TEMP_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        }
    } else {
        alarmLed              = OFF;
        gasDetectorState      = OFF;
        overTempDetectorState = OFF;
        sirenPin.input();
    }
}

// ----------------------------------------------------------------------------
// alarmDeactivationUpdate
// CHANGED: accepts 3-digit code; '#' submits, '*' clears entry
// '#' on keypad also prints event log (new requirement)
// ----------------------------------------------------------------------------
void alarmDeactivationUpdate()
{
    if ( numberOfIncorrectCodes < 5 ) {
        char keyReleased = matrixKeypadUpdate();

        // '*' clears the current entry attempt
        if ( keyReleased == '*' ) {
            matrixKeypadCodeIndex = 0;
            uartUsb.write( "Code entry cleared\r\n", 20 );
            return;
        }

        // Accumulate digits (not '#' or '*')
        if ( keyReleased != '\0' && keyReleased != '#' && keyReleased != '*' ) {
            if ( matrixKeypadCodeIndex < NUMBER_OF_KEYS ) {
                keyPressed[matrixKeypadCodeIndex] = keyReleased;
                uartUsb.write( "*", 1 );   // echo masked digit
                matrixKeypadCodeIndex++;
            }
        }

        // '#' = submit OR show event log (when alarm is off)
        if ( keyReleased == '#' ) {

            if ( !alarmState ) {
                //'#' when no alarm → print event log
                printEventLog();
                return;
            }

            if ( incorrectCodeLed ) {
                numberOfHashKeyReleasedEvents++;
                if ( numberOfHashKeyReleasedEvents >= 2 ) {
                    incorrectCodeLed              = OFF;
                    numberOfHashKeyReleasedEvents = 0;
                    matrixKeypadCodeIndex         = 0;
                }
            } else {
                if ( matrixKeypadCodeIndex < NUMBER_OF_KEYS ) {
                    // Not enough digits yet
                    uartUsb.write( "\r\nEnter all 3 digits first\r\n", 27 );
                } else {
                    if ( areEqual() ) {
                        uartUsb.write( "\r\nCode correct - Alarm deactivated\r\n", 36 );
                        alarmState             = OFF;
                        numberOfIncorrectCodes = 0;
                        matrixKeypadCodeIndex  = 0;
                    } else {
                        uartUsb.write( "\r\nCode incorrect\r\n", 18 );
                        incorrectCodeLed = ON;
                        numberOfIncorrectCodes++;
                        matrixKeypadCodeIndex = 0;
                    }
                }
            }
        }
    } else {
        systemBlockedLed = ON;
    }
}


void uartTask()
{
    char receivedChar = '\0';
    char str[100];
    int  stringLength;

    if ( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
        switch ( receivedChar ) {

        case '1':
            uartUsb.write( alarmState
                ? "The alarm is activated\r\n"
                : "The alarm is not activated\r\n",
                alarmState ? 24 : 28 );
            break;

        case '2':
            uartUsb.write( !mq2
                ? "Gas is being detected\r\n"
                : "Gas is not being detected\r\n",
                !mq2 ? 22 : 27 );
            break;

        case '3':
            uartUsb.write( overTempDetector
                ? "Temperature is above the threshold\r\n"
                : "Temperature is below the threshold\r\n", 36 );
            break;

        //  enter 3-digit deactivation code via serial
        case '4':
            uartUsb.write( "Enter 3-digit code to deactivate: ", 34 );
            incorrectCode = false;
            for ( keyBeingCompared = 0;
                  keyBeingCompared < NUMBER_OF_KEYS;
                  keyBeingCompared++ ) {
                uartUsb.read( &receivedChar, 1 );
                uartUsb.write( "*", 1 );
                if ( codeSequence[keyBeingCompared] != receivedChar )
                    incorrectCode = true;
            }
            if ( !incorrectCode ) {
                uartUsb.write( "\r\nCode correct - Alarm deactivated\r\n\r\n", 37 );
                alarmState             = OFF;
                incorrectCodeLed       = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                uartUsb.write( "\r\nCode incorrect\r\n\r\n", 20 );
                incorrectCodeLed = ON;
                numberOfIncorrectCodes++;
            }
            break;

        //  set new 3-digit code
        case '5':
            uartUsb.write( "Enter new 3-digit code: ", 23 );
            for ( keyBeingCompared = 0;
                  keyBeingCompared < NUMBER_OF_KEYS;
                  keyBeingCompared++ ) {
                uartUsb.read( &codeSequence[keyBeingCompared], 1 );
                uartUsb.write( "*", 1 );
            }
            uartUsb.write( "\r\nNew code saved\r\n\r\n", 20 );
            break;

        case 'c':
        case 'C':
            sprintf( str, "Temperature: %.2f C  |  Threshold: %.2f C\r\n",
                     lm35TempC, overTempLevelC );
            uartUsb.write( str, strlen(str) );
            break;

        case 'f':
        case 'F':
            sprintf( str, "Temperature: %.2f F\r\n",
                     celsiusToFahrenheit( lm35TempC ) );
            uartUsb.write( str, strlen(str) );
            break;

        // 'e'/'E' — print event log from serial keyboard too
        case 'e':
        case 'E':
            printEventLog();
            break;

        case 's':
        case 'S': {
            struct tm rtcTime;
            int strIndex;
            uartUsb.write( "\r\nYear (YYYY): ", 14 );
            for ( strIndex=0; strIndex<4; strIndex++ ) {
                uartUsb.read( &str[strIndex], 1 );
                uartUsb.write( &str[strIndex], 1 );
            }
            str[4] = '\0'; rtcTime.tm_year = atoi(str) - 1900;
            uartUsb.write( "\r\nMonth (01-12): ", 16 );
            for ( strIndex=0; strIndex<2; strIndex++ ) {
                uartUsb.read( &str[strIndex], 1 );
                uartUsb.write( &str[strIndex], 1 );
            }
            str[2] = '\0'; rtcTime.tm_mon = atoi(str) - 1;
            uartUsb.write( "\r\nDay (01-31): ", 14 );
            for ( strIndex=0; strIndex<2; strIndex++ ) {
                uartUsb.read( &str[strIndex], 1 );
                uartUsb.write( &str[strIndex], 1 );
            }
            str[2] = '\0'; rtcTime.tm_mday = atoi(str);
            uartUsb.write( "\r\nHour (00-23): ", 15 );
            for ( strIndex=0; strIndex<2; strIndex++ ) {
                uartUsb.read( &str[strIndex], 1 );
                uartUsb.write( &str[strIndex], 1 );
            }
            str[2] = '\0'; rtcTime.tm_hour = atoi(str);
            uartUsb.write( "\r\nMinutes (00-59): ", 18 );
            for ( strIndex=0; strIndex<2; strIndex++ ) {
                uartUsb.read( &str[strIndex], 1 );
                uartUsb.write( &str[strIndex], 1 );
            }
            str[2] = '\0'; rtcTime.tm_min = atoi(str);
            uartUsb.write( "\r\nSeconds (00-59): ", 18 );
            for ( strIndex=0; strIndex<2; strIndex++ ) {
                uartUsb.read( &str[strIndex], 1 );
                uartUsb.write( &str[strIndex], 1 );
            }
            str[2] = '\0'; rtcTime.tm_sec = atoi(str);
            rtcTime.tm_isdst = -1;
            set_time( mktime( &rtcTime ) );
            uartUsb.write( "\r\nDate and time has been set\r\n", 29 );
            break;
        }

        case 't':
        case 'T': {
            time_t epochSeconds = time(NULL);
            sprintf( str, "Date and Time = %s", ctime(&epochSeconds) );
            uartUsb.write( str, strlen(str) );
            uartUsb.write( "\r\n", 2 );
            break;
        }

        default:
            availableCommands();
            break;
        }
    }
}

void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 20 );
    uartUsb.write( "Press '1' to get the alarm state\r\n", 34 );
    uartUsb.write( "Press '2' to get the gas detector state\r\n", 41 );
    uartUsb.write( "Press '3' to get the temperature vs threshold\r\n", 47 );
    uartUsb.write( "Press '4' to enter 3-digit deactivation code\r\n", 46 );
    uartUsb.write( "Press '5' to set a new 3-digit code\r\n", 37 );
    uartUsb.write( "Press 'C' to get temperature in Celsius + threshold\r\n", 53 );
    uartUsb.write( "Press 'F' to get temperature in Fahrenheit\r\n", 44 );
    uartUsb.write( "Press 'E' to print the event log\r\n", 34 );
    uartUsb.write( "Press 'S' to set date and time\r\n", 31 );
    uartUsb.write( "Press 'T' to get date and time\r\n", 31 );
    uartUsb.write( "Keypad '#' submits code / shows log when no alarm\r\n\r\n", 53 );
}

bool areEqual()
{
    int i;
    for ( i = 0; i < NUMBER_OF_KEYS; i++ ) {
        if ( codeSequence[i] != keyPressed[i] ) return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// eventLogUpdate 
// ----------------------------------------------------------------------------
void eventLogUpdate()
{
    systemElementStateUpdate( alarmLastState, alarmState,        "ALARM"     );
    alarmLastState = alarmState;

    systemElementStateUpdate( gasLastState,  !mq2,               "GAS_DET"   );
    gasLastState = !mq2;

    //  when over-temp transitions ON, log the actual temperature value
    if ( tempLastState != overTempDetector ) {
        if ( overTempDetector ) {
            logTemperatureEvent( lm35TempC );
        }
    }
    systemElementStateUpdate( tempLastState, overTempDetector,   "OVER_TEMP" );
    tempLastState = overTempDetector;

    systemElementStateUpdate( ICLastState,   incorrectCodeLed,   "LED_IC"    );
    ICLastState = incorrectCodeLed;

    systemElementStateUpdate( SBLastState,   systemBlockedLed,   "LED_SB"    );
    SBLastState = systemBlockedLed;
}

// ----------------------------------------------------------------------------
// systemElementStateUpdate , alert messages on gas/temp transitions
// ----------------------------------------------------------------------------
void systemElementStateUpdate( bool lastState,
                               bool currentState,
                               const char* elementName )
{
    char eventAndStateStr[EVENT_NAME_MAX_LENGTH] = "";

    if ( lastState != currentState ) {

        strcat( eventAndStateStr, elementName );
        strcat( eventAndStateStr, currentState ? "_ON" : "_OFF" );

        arrayOfStoredEvents[eventsIndex].seconds = time(NULL);
        strcpy( arrayOfStoredEvents[eventsIndex].typeOfEvent, eventAndStateStr );
        if ( eventsIndex < EVENT_MAX_STORAGE - 1 ) eventsIndex++;
        else                                        eventsIndex = 0;

        // Print the event name
        uartUsb.write( eventAndStateStr, strlen(eventAndStateStr) );
        uartUsb.write( "\r\n", 2 );

        // human-readable alert messages on rising edge
        if ( currentState ) {
            if ( strcmp( elementName, "GAS_DET"   ) == 0 ) {
                uartUsb.write( "Gas Warning Alert\r\n", 19 );
                uartUsb.write( "Buzzer Activated - Cause: Gas\r\n", 31 );
            }
            if ( strcmp( elementName, "OVER_TEMP" ) == 0 ) {
                uartUsb.write( "Temperature Warning Alert\r\n", 27 );
                uartUsb.write( "Buzzer Activated - Cause: Temperature\r\n", 39 );
            }
        }
    }
}

// ----------------------------------------------------------------------------
// logTemperatureEvent stores "TEMP:XX.XX C" in event log
// ----------------------------------------------------------------------------
void logTemperatureEvent( float tempC )
{
    // Build entry string without sprintf %f (safe on newlib-nano)
    // Format: "TEMP:" + integer part + "." + frac part + " C"
    char entry[EVENT_NAME_MAX_LENGTH] = "TEMP:";
    int  whole = (int)tempC;
    int  frac  = (int)( (tempC - (float)whole) * 100.0f + 0.5f );

    // Append whole digits
    char numBuf[8];
    int  n = 0;
    if ( whole == 0 ) { numBuf[n++] = '0'; }
    else {
        int tmp = whole;
        while ( tmp > 0 ) { numBuf[n++] = '0' + (tmp % 10); tmp /= 10; }
        // reverse
        for ( int a=0, b=n-1; a<b; a++, b-- ) {
            char t = numBuf[a]; numBuf[a] = numBuf[b]; numBuf[b] = t;
        }
    }
    numBuf[n] = '\0';
    strncat( entry, numBuf, EVENT_NAME_MAX_LENGTH - strlen(entry) - 1 );
    strncat( entry, ".",    EVENT_NAME_MAX_LENGTH - strlen(entry) - 1 );

    // Append frac digits (always 2)
    char fracBuf[4];
    fracBuf[0] = '0' + (frac / 10);
    fracBuf[1] = '0' + (frac % 10);
    fracBuf[2] = '\0';
    strncat( entry, fracBuf, EVENT_NAME_MAX_LENGTH - strlen(entry) - 1 );
    strncat( entry, "C",     EVENT_NAME_MAX_LENGTH - strlen(entry) - 1 );

    arrayOfStoredEvents[eventsIndex].seconds = time(NULL);
    strcpy( arrayOfStoredEvents[eventsIndex].typeOfEvent, entry );
    if ( eventsIndex < EVENT_MAX_STORAGE - 1 ) eventsIndex++;
    else                                        eventsIndex = 0;

    uartUsb.write( "Temp logged: ", 13 );
    uartUsb.write( entry, strlen(entry) );
    uartUsb.write( "\r\n", 2 );
}

// ----------------------------------------------------------------------------
// printEventLog prints all stored events (called by '#' or 'E')
// ----------------------------------------------------------------------------
void printEventLog()
{
    char str[60];
    if ( eventsIndex == 0 ) {
        uartUsb.write( "No events stored yet\r\n", 22 );
        return;
    }
    uartUsb.write( "\r\n--- Event Log ---\r\n", 20 );
    for ( int i = 0; i < eventsIndex; i++ ) {
        uartUsb.write( "Event: ", 7 );
        uartUsb.write( arrayOfStoredEvents[i].typeOfEvent,
                       strlen(arrayOfStoredEvents[i].typeOfEvent) );
        uartUsb.write( "\r\n", 2 );
        sprintf( str, "Time : %s", ctime(&arrayOfStoredEvents[i].seconds) );
        uartUsb.write( str, strlen(str) );
        uartUsb.write( "\r\n", 2 );
    }
    uartUsb.write( "--- End of Log ---\r\n\r\n", 22 );
}

// ----------------------------------------------------------------------------
// printThresholdUpdate called when potentiometer changes threshold
// ----------------------------------------------------------------------------
void printThresholdUpdate( float threshC )
{
    // Write without %f: split into integer + 1 decimal place
    int whole = (int)threshC;
    int frac  = (int)( (threshC - (float)whole) * 10.0f + 0.5f );

    uartUsb.write( "Threshold updated: ", 19 );

    // Write whole part
    char buf[8];
    int  n = 0;
    if ( whole == 0 ) { buf[n++] = '0'; }
    else {
        int tmp = whole;
        while ( tmp > 0 ) { buf[n++] = '0' + (tmp%10); tmp/=10; }
        for ( int a=0,b=n-1; a<b; a++,b-- ) {
            char t=buf[a]; buf[a]=buf[b]; buf[b]=t;
        }
    }
    uartUsb.write( buf, n );
    uartUsb.write( ".", 1 );

    // Write 1 decimal digit
    char fracCh = '0' + frac;
    uartUsb.write( &fracCh, 1 );
    uartUsb.write( " C\r\n", 4 );
}

//=====[Conversion / sensor functions]=========================================

float analogReadingScaledWithTheLM35Formula( float analogReading )
{
    return ( analogReading * 2.0f / 0.01f );
}

float celsiusToFahrenheit( float tempInCelsiusDegrees )
{
    return ( tempInCelsiusDegrees * 9.0f / 5.0f + 32.0f );
}

void lm35ReadingsArrayInit()
{
    for ( int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++ )
        lm35ReadingsArray[i] = 0;
}

//=====[Matrix keypad]=========================================================

void matrixKeypadInit()
{
    matrixKeypadState = MATRIX_KEYPAD_SCANNING;
    for ( int i = 0; i < KEYPAD_NUMBER_OF_COLS; i++ )
        keypadColPins[i].mode(PullUp);
}

char matrixKeypadScan()
{
    for ( int row = 0; row < KEYPAD_NUMBER_OF_ROWS; row++ ) {
        for ( int i = 0; i < KEYPAD_NUMBER_OF_ROWS; i++ ) keypadRowPins[i] = ON;
        keypadRowPins[row] = OFF;
        for ( int col = 0; col < KEYPAD_NUMBER_OF_COLS; col++ ) {
            if ( keypadColPins[col] == OFF )
                return matrixKeypadIndexToCharArray[row*KEYPAD_NUMBER_OF_ROWS + col];
        }
    }
    return '\0';
}

char matrixKeypadUpdate()
{
    char keyDetected = '\0';
    char keyReleased = '\0';

    switch ( matrixKeypadState ) {

    case MATRIX_KEYPAD_SCANNING:
        keyDetected = matrixKeypadScan();
        if ( keyDetected != '\0' ) {
            matrixKeypadLastKeyPressed          = keyDetected;
            accumulatedDebounceMatrixKeypadTime = 0;
            matrixKeypadState                   = MATRIX_KEYPAD_DEBOUNCE;
        }
        break;

    case MATRIX_KEYPAD_DEBOUNCE:
        if ( accumulatedDebounceMatrixKeypadTime >= DEBOUNCE_KEY_TIME_MS ) {
            keyDetected = matrixKeypadScan();
            matrixKeypadState = ( keyDetected == matrixKeypadLastKeyPressed )
                                ? MATRIX_KEYPAD_KEY_HOLD_PRESSED
                                : MATRIX_KEYPAD_SCANNING;
        }
        accumulatedDebounceMatrixKeypadTime += TIME_INCREMENT_MS;
        break;

    case MATRIX_KEYPAD_KEY_HOLD_PRESSED:
        keyDetected = matrixKeypadScan();
        if ( keyDetected != matrixKeypadLastKeyPressed ) {
            if ( keyDetected == '\0' ) keyReleased = matrixKeypadLastKeyPressed;
            matrixKeypadState = MATRIX_KEYPAD_SCANNING;
        }
        break;

    default:
        matrixKeypadInit();
        break;
    }
    return keyReleased;
}