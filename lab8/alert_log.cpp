//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "alert_log.h"

#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "date_and_time.h"
#include "sd_card.h"
#include "pc_serial_com.h"
#include "smart_home_system.h"
//=====[Declaration of private defines]========================================

#define ALERT_FILENAME          "alerts.txt"
#define ALERT_READ_FILENAME     "alerts.txt"    // same file; separated for clarity
#define ALERT_LINE_MAX_LEN      80

//=====[Declaration and initialization of private global variables]============

static alertSensorMask_t monitoredSensors = ALERT_SENSOR_BOTH;  // default: monitor both

// Per-sensor cooldown timers (ms)
static int tempCooldownMs = 0;
static int gasCooldownMs  = 0;

//=====[Declarations (prototypes) of private functions]========================

static void checkTemperatureAlert();
static void checkGasAlert();
static bool writeAlertToSdCard( const char* alertLine );

//=====[Implementations of public functions]===================================

void alertLogInit()
{
    monitoredSensors = ALERT_SENSOR_BOTH;
    tempCooldownMs   = 0;
    gasCooldownMs    = 0;
}

void alertLogUpdate()
{
    // Decrement cooldown timers (saturate at 0)
    if ( tempCooldownMs > 0 ) {
        tempCooldownMs -= SYSTEM_TIME_INCREMENT_MS;
        if ( tempCooldownMs < 0 ) tempCooldownMs = 0;
    }
    if ( gasCooldownMs > 0 ) {
        gasCooldownMs -= SYSTEM_TIME_INCREMENT_MS;
        if ( gasCooldownMs < 0 ) gasCooldownMs = 0;
    }

    if ( monitoredSensors & ALERT_SENSOR_TEMPERATURE ) {
        checkTemperatureAlert();
    }
    if ( monitoredSensors & ALERT_SENSOR_GAS ) {
        checkGasAlert();
    }
}

void alertLogSetMonitoredSensors( alertSensorMask_t mask )
{
    monitoredSensors = mask;

    // Confirm the new selection on the serial monitor
    pcSerialComStringWrite( "\r\n[Alert Log] Monitoring: " );
    switch ( monitoredSensors ) {
        case ALERT_SENSOR_TEMPERATURE:
            pcSerialComStringWrite( "Temperature only\r\n" ); break;
        case ALERT_SENSOR_GAS:
            pcSerialComStringWrite( "Gas only\r\n" );         break;
        case ALERT_SENSOR_BOTH:
            pcSerialComStringWrite( "Temperature + Gas\r\n" ); break;
        default:
            pcSerialComStringWrite( "None\r\n" );              break;
    }
}

alertSensorMask_t alertLogGetMonitoredSensors()
{
    return monitoredSensors;
}

/*
 * Opens ALERT_READ_FILENAME on the SD card and streams every line to the
 * serial monitor.  Call this when the user presses '*'.
 */
void alertLogReadFromSdCard()
{
    pcSerialComStringWrite( "\r\n=== Stored Alerts ===\r\n" );

    int lineCount = sdCardReadFileLineCount( ALERT_READ_FILENAME );

    if ( lineCount <= 0 ) {
        pcSerialComStringWrite( "(No alerts found or SD card unavailable)\r\n" );
        pcSerialComStringWrite( "=====================\r\n\r\n" );
        return;
    }

    char line[ALERT_LINE_MAX_LEN] = "";
    for ( int i = 0; i < lineCount; i++ ) {
        if ( sdCardReadFileLine( ALERT_READ_FILENAME, i, line, ALERT_LINE_MAX_LEN ) ) {
            pcSerialComStringWrite( line );
            pcSerialComStringWrite( "\r\n" );
        }
    }

    pcSerialComStringWrite( "=====================\r\n\r\n" );
}
//=====[Implementations of private functions]==================================

static void checkTemperatureAlert()
{
    if ( tempCooldownMs > 0 ) return;   // still cooling down

    float tempC = temperatureSensorReadCelsius();

    if ( tempC >= ALERT_TEMP_MIN_C && tempC <= ALERT_TEMP_MAX_C ) {

        // Build alert line: "2025-06-20 14:32:05 | TEMP | 28 C"
        char alertLine[ALERT_LINE_MAX_LEN] = "";
        char tempStr[8]  = "";
        char timeStr[30] = "";

        // Use dateAndTimeRead() which returns a pre-formatted string
        const char* dt = dateAndTimeRead();
        // Copy only date+time portion (strip trailing newline if present)
        int dtLen = 0;
        while ( dt[dtLen] != '\0' && dt[dtLen] != '\n' && dtLen < 24 ) {
            timeStr[dtLen] = dt[dtLen];
            dtLen++;
        }
        timeStr[dtLen] = '\0';

        sprintf( tempStr, "%.0f", tempC );

        strcat( alertLine, timeStr );
        strcat( alertLine, " | TEMP | " );
        strcat( alertLine, tempStr );
        strcat( alertLine, " C" );

        // Write to SD card
        writeAlertToSdCard( alertLine );

        // Confirm on serial monitor
        pcSerialComStringWrite( "Alert Logged: Temp = " );
        pcSerialComStringWrite( tempStr );
        pcSerialComStringWrite( " C\r\n" );

        tempCooldownMs = ALERT_COOLDOWN_MS;
    }
}

static void checkGasAlert()
{
    if ( gasCooldownMs > 0 ) return;

    float gasPpm = gasSensorAnalogRead();

    if ( gasPpm >= ALERT_GAS_MIN_PPM && gasPpm <= ALERT_GAS_MAX_PPM ) {

        char alertLine[ALERT_LINE_MAX_LEN] = "";
        char ppmStr[10]  = "";
        char timeStr[30] = "";

        const char* dt = dateAndTimeRead();
        int dtLen = 0;
        while ( dt[dtLen] != '\0' && dt[dtLen] != '\n' && dtLen < 24 ) {
            timeStr[dtLen] = dt[dtLen];
            dtLen++;
        }
        timeStr[dtLen] = '\0';

        sprintf( ppmStr, "%.0f", gasPpm );

        strcat( alertLine, timeStr );
        strcat( alertLine, " | GAS  | " );
        strcat( alertLine, ppmStr );
        strcat( alertLine, " ppm" );

        writeAlertToSdCard( alertLine );

        // Confirm on serial monitor
        pcSerialComStringWrite( "Alert Logged: Gas = " );
        pcSerialComStringWrite( ppmStr );
        pcSerialComStringWrite( " ppm\r\n" );

        gasCooldownMs = ALERT_COOLDOWN_MS;
    }
}

static bool writeAlertToSdCard( const char* alertLine )
{
    char lineWithNewline[ALERT_LINE_MAX_LEN + 2] = "";
    strcat( lineWithNewline, alertLine );
    strcat( lineWithNewline, "\n" );
    return sdCardWriteFile( ALERT_FILENAME, lineWithNewline );
}