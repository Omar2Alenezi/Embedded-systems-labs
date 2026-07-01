//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <string.h>

//=====[Declaration and initialization of public global objects]===============

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn     potentiometer(A0);
AnalogIn     lm35(A1);
AnalogIn     mq2Analog(A2);
DigitalInOut sirenPin(PE_10);

//=====[Declaration and initialization of public global variables]=============

float potentiometerReading = 0.0;
float lm35Reading          = 0.0;
float mq2Reading           = 0.0;

int   sensitivityPct   = 0;
float tempThresholdC   = 0.0;
float gasThreshold     = 0.0;
float lm35TempC        = 0.0;

bool  tempAlarm        = false;
bool  gasAlarm         = false;

#define GAS_DEAD_BAND  0.02f

//=====[Declarations (prototypes) of public functions]=========================

void systemInit();
void readSensors();
void computeThresholds();
void evaluateAlarms();
void driveOutputs();
void reportToSerial();

float analogReadingScaledWithTheLM35Formula( float analogReading );
float sensitivityPctToTempThreshold( int pct );
float sensitivityPctToGasThreshold( int pct );

void uartWriteString( const char* str );
void uartWriteFloat ( float value, int decimalPlaces );
void uartWriteInt   ( int value );

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    systemInit();

    while ( true ) {
        readSensors();
        computeThresholds();
        evaluateAlarms();
        driveOutputs();
        reportToSerial();
        delay( 1000 );
    }
}

//=====[Implementations of public functions]===================================

void systemInit()
{
    sirenPin.mode( OpenDrain );
    sirenPin.input();

    const char* banner =
        "\r\n============================================\r\n"
        "  Smart Dual-Alarm System - Initialised\r\n"
        "============================================\r\n"
        "Potentiometer -> unified sensitivity (0-100)\r\n"
        "  0   = most sensitive\r\n"
        "  100 = least sensitive\r\n"
        "Temp range : 2 C (sens=0) to 150 C (sens=100)\r\n"
        "Gas  range : 0.02 (sens=0) to 1.0 (sens=100)\r\n"
        "============================================\r\n\r\n";

    uartUsb.write( banner, strlen(banner) );
}

void readSensors()
{
    potentiometerReading = potentiometer.read();
    lm35Reading          = lm35.read();
    mq2Reading           = mq2Analog.read();
    lm35TempC            = analogReadingScaledWithTheLM35Formula( lm35Reading );
}

void computeThresholds()
{
    sensitivityPct = (int)( potentiometerReading * 100.0f );
    if ( sensitivityPct <   0 ) sensitivityPct =   0;
    if ( sensitivityPct > 100 ) sensitivityPct = 100;

    tempThresholdC = sensitivityPctToTempThreshold( sensitivityPct );
    gasThreshold   = sensitivityPctToGasThreshold ( sensitivityPct );
}

void evaluateAlarms()
{
    tempAlarm = ( lm35TempC  > tempThresholdC );
    gasAlarm  = ( mq2Reading > gasThreshold   );
}

void driveOutputs()
{
    if ( tempAlarm || gasAlarm ) {
        sirenPin.output();
        sirenPin = LOW;
    } else {
        sirenPin.input();
    }
}

void reportToSerial()
{
    if ( tempAlarm || gasAlarm ) {

        if ( tempAlarm ) {
            const char* msg = "Temperature Warning Alert\r\n";
            uartUsb.write( msg, strlen(msg) );
        }
        if ( gasAlarm ) {
            const char* msg = "Gas Warning Alert\r\n";
            uartUsb.write( msg, strlen(msg) );
        }

        const char* prefix = "Buzzer Activated - Cause: ";
        uartUsb.write( prefix, strlen(prefix) );

        if      (  tempAlarm && !gasAlarm ) {
            const char* cause = "Temperature\r\n";
            uartUsb.write( cause, strlen(cause) );
        }
        else if ( !tempAlarm &&  gasAlarm ) {
            const char* cause = "Gas\r\n";
            uartUsb.write( cause, strlen(cause) );
        }
        else {
            const char* cause = "Temperature & Gas\r\n";
            uartUsb.write( cause, strlen(cause) );
        }

        const char* tLabel = "  Temp: ";
        uartUsb.write( tLabel, strlen(tLabel) );
        uartWriteFloat( lm35TempC, 1 );
        const char* tMid = " C  (threshold: ";
        uartUsb.write( tMid, strlen(tMid) );
        uartWriteFloat( tempThresholdC, 1 );
        const char* tEnd = " C)\r\n";
        uartUsb.write( tEnd, strlen(tEnd) );

        const char* gLabel = "  Gas ADC: ";
        uartUsb.write( gLabel, strlen(gLabel) );
        uartWriteFloat( mq2Reading, 3 );
        const char* gMid = "  (threshold: ";
        uartUsb.write( gMid, strlen(gMid) );
        uartWriteFloat( gasThreshold, 3 );
        const char* gEnd = ")\r\n";
        uartUsb.write( gEnd, strlen(gEnd) );

        const char* sLabel = "  Sensitivity: ";
        uartUsb.write( sLabel, strlen(sLabel) );
        uartWriteInt( sensitivityPct );
        const char* sEnd = "%\r\n\r\n";
        uartUsb.write( sEnd, strlen(sEnd) );

    } else {

        const char* msg = "System Normal | Temp: ";
        uartUsb.write( msg, strlen(msg) );
        uartWriteFloat( lm35TempC, 1 );
        const char* t1 = " C (thr: ";
        uartUsb.write( t1, strlen(t1) );
        uartWriteFloat( tempThresholdC, 1 );
        const char* t2 = " C) | Gas: ";
        uartUsb.write( t2, strlen(t2) );
        uartWriteFloat( mq2Reading, 3 );
        const char* t3 = " (thr: ";
        uartUsb.write( t3, strlen(t3) );
        uartWriteFloat( gasThreshold, 3 );
        const char* t4 = ") | Sens: ";
        uartUsb.write( t4, strlen(t4) );
        uartWriteInt( sensitivityPct );
        const char* t5 = "%\r\n";
        uartUsb.write( t5, strlen(t5) );
    }
}

//=====[Threshold mapping functions]===========================================

float sensitivityPctToTempThreshold( int pct )
{
    return 1.48f * (float)pct + 2.0f;
}

float sensitivityPctToGasThreshold( int pct )
{
    // Invert: pct=0 (pot fully left)  → threshold = 1.0  (hardest to trigger)
    //         pct=100 (pot fully right) → threshold = 0.0001 (easiest to trigger)
    float raw = 0.9f - ( (float)pct / 100.0f );
    return ( raw < GAS_DEAD_BAND ) ? GAS_DEAD_BAND : raw;
}


float analogReadingScaledWithTheLM35Formula( float analogReading )
{
    return analogReading * 330.0f;
}

//=====[UART helpers]==========================================================

void uartWriteString( const char* str )
{
    uartUsb.write( str, strlen(str) );
}

void uartWriteFloat( float value, int decimalPlaces )
{
    if ( value < 0.0f ) {
        uartUsb.write( "-", 1 );
        value = -value;
    }

    float scale = 1.0f;
    for ( int i = 0; i < decimalPlaces; i++ ) scale *= 10.0f;
    int rounded  = (int)( value * scale + 0.5f );
    int intPart  = rounded / (int)scale;
    int fracPart = rounded % (int)scale;

    uartWriteInt( intPart );

    if ( decimalPlaces > 0 ) {
        uartUsb.write( ".", 1 );
        int leadingScale = (int)scale / 10;
        while ( leadingScale > 1 && fracPart < leadingScale ) {
            uartUsb.write( "0", 1 );
            leadingScale /= 10;
        }
        uartWriteInt( fracPart );
    }
}

void uartWriteInt( int value )
{
    if ( value < 0 ) {
        uartUsb.write( "-", 1 );
        value = -value;
    }
    if ( value == 0 ) {
        uartUsb.write( "0", 1 );
        return;
    }
    char buf[12];
    int  idx = 0;
    while ( value > 0 ) {
        buf[idx++] = '0' + ( value % 10 );
        value /= 10;
    }
    for ( int i = idx - 1; i >= 0; i-- ) {
        uartUsb.write( &buf[i], 1 );
    }
}