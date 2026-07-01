//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "user_interface.h"

#include "code.h"
#include "alarm.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "intruder_alarm.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "motion_sensor.h"
#include "matrix_keypad.h"
#include "display.h"
#include "GLCD_fire_alarm.h"
#include "GLCD_intruder_alarm.h"
#include "motor.h"
#include "gate.h"
#include "light_system.h"
#include "light_level_control.h"
#include "event_log.h"
#include "siren.h"
#include "alert_log.h"
//=====[Declaration of private defines]========================================

#define DISPLAY_REFRESH_TIME_REPORT_MS 1000
#define DISPLAY_REFRESH_TIME_ALARM_MS 300
#define DISPLAY_REFRESH_TIME_MS 1000

//=====[Declaration of private data types]=====================================

typedef enum {
    DISPLAY_ALARM_STATE,
    DISPLAY_REPORT_STATE
} displayState_t;

//=====[Declaration and initialization of public global objects]===============

InterruptIn gateOpenButton(PF_9);
InterruptIn gateCloseButton(PF_8);

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

//=====[Declaration of external public global variables]=======================

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static displayState_t displayState = DISPLAY_REPORT_STATE;
static int displayFireAlarmGraphicSequence = 0;
static int displayIntruderAlarmGraphicSequence = 0;
static int displayRefreshTimeMs = DISPLAY_REFRESH_TIME_REPORT_MS;

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;

static bool codeComplete = false;
static int numberOfCodeChars = 0;

//=====[Declarations (prototypes) of private functions]========================

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();
static void userInterfaceDisplayReportStateInit();
static void userInterfaceDisplayReportStateUpdate();
static void userInterfaceDisplayAlarmStateInit();
static void userInterfaceDisplayAlarmStateUpdate();

static void gateOpenButtonCallback();
static void gateCloseButtonCallback();

//=====[Implementations of public functions]===================================
static void userInterfaceDisplayInit() {
    displayInit(DISPLAY_TYPE_LCD_HD44780, DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER);

    displayCharPositionWrite(0, 0);
    displayStringWrite("Temperature:");

    displayCharPositionWrite(0, 1);
    displayStringWrite("Gas:");

    displayCharPositionWrite(0, 2);
    displayStringWrite("Alarm:");

    displayCharPositionWrite(0, 3);
    displayStringWrite("Enter Code:");
}

void userInterfaceInit()
{
    gateOpenButton.mode(PullUp);
    gateCloseButton.mode(PullUp);

    gateOpenButton.fall(&gateOpenButtonCallback);
    gateCloseButton.fall(&gateCloseButtonCallback);
    
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
    matrixKeypadInit( SYSTEM_TIME_INCREMENT_MS );
    userInterfaceDisplayInit();
    
    lightLevelControlInit();
}

void userInterfaceUpdate()
{
    userInterfaceMatrixKeypadUpdate();
    incorrectCodeIndicatorUpdate();
    systemBlockedIndicatorUpdate();
    userInterfaceDisplayUpdate();
    lightLevelControlUpdate();
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

//=====[Implementations of private functions]==================================
void showEventLoggedNotification()
{
    eventLoggedNotificationActive = true;
    eventLoggedNotificationTimer = 0;
}

static void displayEventLoggedNotification()
{
    displayClear();
    displayCharPositionWrite(0, 0);
    displayStringWrite("  EVENT LOGGED  ");
    displayCharPositionWrite(0, 1);
    displayStringWrite(" TO SD CARD ");
    displayCharPositionWrite(0, 2);
    displayStringWrite(" SUCCESSFULLY ");
    
    eventLoggedNotificationTimer += SYSTEM_TIME_INCREMENT_MS;
    if (eventLoggedNotificationTimer >= 3000) { // Show for 3 seconds
        eventLoggedNotificationActive = false;
        userInterfaceDisplayReportStateInit(); // Return to normal display
    }
}
static void userInterfaceMatrixKeypadUpdate()
{
    static int numberOfHashKeyReleased = 0;
    char keyReleased = matrixKeypadUpdate();
 
    if ( keyReleased != '\0' ) {
 
        if ( alarmStateRead() && !systemBlockedStateRead() ) {
            // ── Alarm active: handle code entry ──────────────────────────
            if ( !incorrectCodeStateRead() ) {
                codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
                numberOfCodeChars++;
                if ( numberOfCodeChars >= CODE_NUMBER_OF_KEYS ) {
                    codeComplete = true;
                    numberOfCodeChars = 0;
                }
            } else {
                if ( keyReleased == '#' ) {
                    numberOfHashKeyReleased++;
                    if ( numberOfHashKeyReleased >= 2 ) {
                        numberOfHashKeyReleased = 0;
                        numberOfCodeChars       = 0;
                        codeComplete            = false;
                        incorrectCodeState      = OFF;
                    }
                }
            }
 
        } else if ( !systemBlockedStateRead() ) {
            // ── Normal operation ──────────────────────────────────────────
 
            // Sensor selection for alert logging
            if ( keyReleased == 'A' ) {
                alertLogSetMonitoredSensors( ALERT_SENSOR_TEMPERATURE );
                // (motionSensorActivate() removed — reassigned to 'A'
                //  adjust if you still need motion activation elsewhere)
            }
            if ( keyReleased == 'B' ) {
                alertLogSetMonitoredSensors( ALERT_SENSOR_GAS );
                // (motionSensorDeactivate() removed — reassigned to 'B')
            }
 
            // Read all stored alerts from SD card → serial monitor
            if ( keyReleased == '*' ) {
                alertLogReadFromSdCard();
            }
 
            // RGB brightness control (unchanged)
            if ( keyReleased == '1' ) {
                lightSystemBrightnessChangeRGBFactor( RGB_LED_RED,   true  );
            }
            if ( keyReleased == '2' ) {
                lightSystemBrightnessChangeRGBFactor( RGB_LED_GREEN, true  );
            }
            if ( keyReleased == '3' ) {
                lightSystemBrightnessChangeRGBFactor( RGB_LED_BLUE,  true  );
            }
            if ( keyReleased == '4' ) {
                lightSystemBrightnessChangeRGBFactor( RGB_LED_RED,   false );
            }
            if ( keyReleased == '5' ) {
                lightSystemBrightnessChangeRGBFactor( RGB_LED_GREEN, false );
            }
            if ( keyReleased == '6' ) {
                lightSystemBrightnessChangeRGBFactor( RGB_LED_BLUE,  false );
            }
        }
    }
}

static void userInterfaceDisplayReportStateInit()
{
    displayState = DISPLAY_REPORT_STATE;
    displayRefreshTimeMs = DISPLAY_REFRESH_TIME_REPORT_MS;

    displayModeWrite( DISPLAY_MODE_CHAR );

    displayClear();

    displayCharPositionWrite ( 0,0 );
    displayStringWrite( "Temperature:" );

    displayCharPositionWrite ( 0,1 );
    displayStringWrite( "Gas:" );

    displayCharPositionWrite ( 0,2 );
    displayStringWrite( "Alarm:" );
}

static void userInterfaceDisplayReportStateUpdate()
{
    char temperatureString[3] = "";

    sprintf(temperatureString, "%.0f", temperatureSensorReadCelsius());
    displayCharPositionWrite ( 12,0 );
    displayStringWrite( temperatureString );
    displayCharPositionWrite ( 14,0 );
    displayStringWrite( "'C" );

    displayCharPositionWrite ( 4,1 );

    if ( gasDetectorStateRead() ) {
        displayStringWrite( "Detected    " );
    } else {
        displayStringWrite( "Not Detected" );
    }
    displayCharPositionWrite ( 6,2 );
    displayStringWrite( "OFF" );
}

static void userInterfaceDisplayAlarmStateInit()
{
    displayState = DISPLAY_ALARM_STATE;
    displayRefreshTimeMs = DISPLAY_REFRESH_TIME_ALARM_MS;

    displayClear();

    displayModeWrite( DISPLAY_MODE_GRAPHIC );

    displayFireAlarmGraphicSequence = 0;
}

static void userInterfaceDisplayAlarmStateUpdate()
{
    if ( ( gasDetectedRead() ) || ( overTemperatureDetectedRead() ) ) {
        switch( displayFireAlarmGraphicSequence ) {
        case 0:
            displayBitmapWrite( GLCD_fire_alarm[0] );
            displayFireAlarmGraphicSequence++;
            break;
        case 1:
            displayBitmapWrite( GLCD_fire_alarm[1] );
            displayFireAlarmGraphicSequence++;
            break;
        case 2:
            displayBitmapWrite( GLCD_fire_alarm[2] );
            displayFireAlarmGraphicSequence++;
            break;
        case 3:
            displayBitmapWrite( GLCD_fire_alarm[3] );
            displayFireAlarmGraphicSequence = 0;
            break;
        default:
            displayBitmapWrite( GLCD_ClearScreen );
            displayFireAlarmGraphicSequence = 0;
            break;
        }
    } else if ( intruderDetectedRead() ) {
        switch( displayIntruderAlarmGraphicSequence ) {
        case 0:
            displayBitmapWrite( GLCD_intruder_alarm );
            displayIntruderAlarmGraphicSequence++;
            break;
        case 1:
        default:
            displayBitmapWrite( GLCD_ClearScreen );
            displayIntruderAlarmGraphicSequence = 0;
            break;
        }
    }
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;

    if (displayLogConfirmation) {
        if (displayLogConfirmationTimerMs == 0) {
            displayClear();
            displayCharPositionWrite(2, 1); // Centered
            displayStringWrite("Event Saved!");
        }
        displayLogConfirmationTimerMs += SYSTEM_TIME_INCREMENT_MS;
        if (displayLogConfirmationTimerMs >= LOG_CONFIRMATION_DISPLAY_TIME_MS) {
            displayLogConfirmation = false;
            displayLogConfirmationTimerMs = 0;
            displayClear(); // optional: clear after confirmation
            userInterfaceDisplayInit(); // reinitialize main display
        }
    } else {
        if (accumulatedDisplayTime >= DISPLAY_REFRESH_TIME_MS) {
            accumulatedDisplayTime = 0;

            char temperatureString[3] = "";
            sprintf(temperatureString, "%.0f", temperatureSensorReadCelsius());
            displayCharPositionWrite(12,0);
            displayStringWrite(temperatureString);
            displayCharPositionWrite(14,0);
            displayStringWrite("'C");

            displayCharPositionWrite(4,1);
            if (gasDetectorStateRead()) {
                displayStringWrite("Detected    ");
            } else {
                displayStringWrite("Not Detected");
            }

            displayCharPositionWrite(6,2);
            if (sirenStateRead()) {
                displayStringWrite("ON ");
            } else {
                displayStringWrite("OFF");
            }
        } else {
            accumulatedDisplayTime += SYSTEM_TIME_INCREMENT_MS;
        }
    }
} // <-- THIS closing brace was missing

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedState;
}

static void gateOpenButtonCallback()
{
    gateOpen();
}

static void gateCloseButtonCallback()
{
    gateClose();
}
