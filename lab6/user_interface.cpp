//[Libraries]
#include "mbed.h"
#include "arm_book_lib.h"

#include "user_interface.h"

#include "code.h"
#include "siren.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "matrix_keypad.h"
#include "display.h"

//Declaration of private defines]

#define DISPLAY_REFRESH_TIME_MS 1000

// Requirement 2: safe limit used to trigger an LCD warning 
// Adjust this value to whatever your hardware/sensor setup considers unsafe.
#define TEMPERATURE_SAFE_LIMIT_C   50

// Requirement 1: how long an on-demand sensor report stays on screen
#define REPORT_DISPLAY_TIME_MS     3000

// Row used for status/warning/report/code-entry messages on the 20x4 LCD
#define DISPLAY_MESSAGE_ROW 3

//[Declaration of private data types]

//  Requirement 1: which on-demand report (if any) is currently selected 
typedef enum {
    REPORT_NONE,
    REPORT_GAS,
    REPORT_TEMPERATURE
} report_t;

//[Declaration and initialization of public global objects]

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

//[Declaration of external public global variables]

//Declaration and initialization of public global variables]

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//Declaration and initialization of private global variables]

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;

static bool codeComplete = false;
static int numberOfCodeChars = 0;

//  Requirement 1: on-demand report state 
static report_t activeReport = REPORT_NONE;
static int reportDisplayTimeAccumulated = 0;

// Requirement 2: warning state
static bool warningActive = false;

//Declarations (prototypes) of private functions]

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();

static void displayMessageRowWrite( const char * text );
static void userInterfaceDisplayMessageRowUpdate();
static void userInterfaceRequestReport( report_t report );
static void warningConditionUpdate();

//[Implementations of public functions]

void userInterfaceInit()
{
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
    matrixKeypadInit( SYSTEM_TIME_INCREMENT_MS );
    userInterfaceDisplayInit();
}

void userInterfaceUpdate()
{
    userInterfaceMatrixKeypadUpdate();
    incorrectCodeIndicatorUpdate();
    systemBlockedIndicatorUpdate();
    userInterfaceDisplayUpdate();
}

bool incorrectCodeStateRead()
{
    return incorrectCodeState;
}

void incorrectCodeStateWrite( bool state )
{
    incorrectCodeState = state;
}

bool systemBlockedStateRead()
{
    return systemBlockedState;
}

void systemBlockedStateWrite( bool state )
{
    systemBlockedState = state;
}

bool userInterfaceCodeCompleteRead()
{
    return codeComplete;
}

void userInterfaceCodeCompleteWrite( bool state )
{
    codeComplete = state;
}

//[Implementations of private functions]

static void userInterfaceMatrixKeypadUpdate()
{
    // Tracks whether the previous released key was '#', enabling the
    // '#2' / '#3' combos for on-demand sensor reports from anywhere
    // (alarm ON or OFF) without consuming those keys as code digits.
    static bool lastKeyWasHash = false;
    static int  numberOfHashKeyReleased = 0;

    char keyReleased = matrixKeypadUpdate();
    if( keyReleased == '\0' ) return;

    // --- '#2' and '#3' combos: on-demand sensor reports -------------------
    if( lastKeyWasHash ) {
        if( keyReleased == '2' ) {
            lastKeyWasHash = false;
            userInterfaceRequestReport( REPORT_TEMPERATURE );
            return;                         // do NOT pass to code-entry
        }
        if( keyReleased == '3' ) {
            lastKeyWasHash = false;
            userInterfaceRequestReport( REPORT_GAS );
            return;                         // do NOT pass to code-entry
        }
        // Any other key after '#': fall through; handle the new key normally.
    }

    // Remember whether THIS key was '#' for the next press
    lastKeyWasHash = ( keyReleased == '#' );

    //  Normal code-entry / alarm handling
    if( sirenStateRead() && !systemBlockedStateRead() ) {
        if( !incorrectCodeStateRead() ) {
            // '2' and '3' are now valid code digits
            codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
            numberOfCodeChars++;
            userInterfaceDisplayMessageRowUpdate();
            if( numberOfCodeChars >= CODE_NUMBER_OF_KEYS ) {
                codeComplete = true;
                numberOfCodeChars = 0;
            }
        } else {
            // Wrong code: user must press '##' to retry
            if( keyReleased == '#' ) {
                numberOfHashKeyReleased++;
                if( numberOfHashKeyReleased >= 2 ) {
                    numberOfHashKeyReleased = 0;
                    numberOfCodeChars = 0;
                    codeComplete = false;
                    incorrectCodeState = OFF;
                    userInterfaceDisplayMessageRowUpdate();
                }
            }
        }
    }
    // When the alarm is OFF there is nothing else to do; sensor reports are
    // the only interactive feature and are handled by '#2'/'#3' above.
}

static void userInterfaceDisplayInit()
{
    displayInit( DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER );

    // Rows 0 and 1 are intentionally left blank at startup.
    // They are only used to show warning messages when sensor
    // thresholds are exceeded (see warningDisplayUpdate).
    displayCharPositionWrite( 0, 0 );
    displayStringWrite( "                    " );

    displayCharPositionWrite( 0, 1 );
    displayStringWrite( "                    " );

    displayCharPositionWrite ( 0,2 );
    displayStringWrite( "Alarm:" );

    displayMessageRowWrite( "Enter Code to Deact." );
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;
    
    if( accumulatedDisplayTime >=
        DISPLAY_REFRESH_TIME_MS ) {

        accumulatedDisplayTime = 0;

       
        warningConditionUpdate();

        bool tempUnsafe = temperatureSensorReadCelsius() > TEMPERATURE_SAFE_LIMIT_C;
        bool gasUnsafe  = gasDetectorStateRead();
        char tempStr[8];
        sprintf( tempStr, "%.0f'C  ", temperatureSensorReadCelsius() );

        // Row 0: temperature warning with current value, or blank when safe
        displayCharPositionWrite( 0, 0 );
        if( tempUnsafe ) {
            displayStringWrite( "WARN HIGH TEMP:     " );
            displayCharPositionWrite( 15, 0 );
            displayStringWrite( tempStr );
        } else {
            displayStringWrite( "                    " );
        }

        // Row 1: gas warning or blank when safe
        displayCharPositionWrite( 0, 1 );
        if( gasUnsafe ) {
            displayStringWrite( "WARN: GAS LEAK!     " );
        } else {
            displayStringWrite( "                    " );
        }

        displayCharPositionWrite ( 6,2 );
        
        if ( sirenStateRead() ) {
            displayStringWrite( "ON " );
        } else {
            displayStringWrite( "OFF" );
        }

        if( activeReport != REPORT_NONE ) {
            reportDisplayTimeAccumulated += DISPLAY_REFRESH_TIME_MS;
            if( reportDisplayTimeAccumulated >= REPORT_DISPLAY_TIME_MS ) {
                activeReport = REPORT_NONE;
                reportDisplayTimeAccumulated = 0;
            }
        }

        userInterfaceDisplayMessageRowUpdate();

    } else {
        accumulatedDisplayTime =
            accumulatedDisplayTime + SYSTEM_TIME_INCREMENT_MS;        
    } 
}

static void warningConditionUpdate()
{
    warningActive = ( temperatureSensorReadCelsius() > TEMPERATURE_SAFE_LIMIT_C ) ||
                    gasDetectorStateRead();
}

static void userInterfaceRequestReport( report_t report )
{
    activeReport = report;
    reportDisplayTimeAccumulated = 0;
    userInterfaceDisplayMessageRowUpdate();
}

// Writes exactly one 20-character-wide row (row 3) on the LCD, padding or
// truncating as needed so leftover characters from a previous message never
// remain on screen.
static void displayMessageRowWrite( const char * text )
{
    char buffer[21];
    sprintf( buffer, "%-20.20s", text );
    displayCharPositionWrite( 0, DISPLAY_MESSAGE_ROW );
    displayStringWrite( buffer );
}


static void userInterfaceDisplayMessageRowUpdate()
{
    char message[32];
    char mask[CODE_NUMBER_OF_KEYS + 1];
    int i;

    // Priority 1: code-entry feedback while alarm is sounding
    if( sirenStateRead() ) {
        if( activeReport == REPORT_NONE ) {
            if( incorrectCodeStateRead() ) {
                displayMessageRowWrite( "Incorrect! Press ##" );
                return;
            }
            for( i = 0; i < CODE_NUMBER_OF_KEYS; i++ ) {
                mask[i] = ( i < numberOfCodeChars ) ? '*' : '_';
            }
            mask[CODE_NUMBER_OF_KEYS] = '\0';
            sprintf( message, "Code: %s", mask );
            displayMessageRowWrite( message );
            return;
        }
    }

    // Priority 2: on-demand report (keys '2' or '3'), works alarm ON or OFF
    if( activeReport == REPORT_TEMPERATURE ) {
        if( temperatureSensorReadCelsius() > TEMPERATURE_SAFE_LIMIT_C ) {
            displayMessageRowWrite( "Temp:Over Limit" );
        } else {
            displayMessageRowWrite( "Temp:Normal" );
        }
        return;
    }

    if( activeReport == REPORT_GAS ) {
        if( gasDetectorStateRead() ) {
            displayMessageRowWrite( "Gas:Detected" );
        } else {
            displayMessageRowWrite( "Gas:Not Det." );
        }
        return;
    }

    // Priority 3: default idle message (warning now handled on rows 0 & 1)
    displayMessageRowWrite( "Enter Code to Deact." );
}

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedState;
}