//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "gas_sensor.h"

//=====[Declaration of private defines]========================================

#define GAS_SENSOR_ANALOG_SCALE   1000.0f  // Scale factor: 0.0-1.0 → 0-1000 ppm (approx)

//=====[Declaration and initialization of public global objects]===============

DigitalIn mq2Digital(PE_12);
AnalogIn  mq2Analog(PF_3);

//=====[Implementations of public functions]===================================

void gasSensorInit()
{
}

void gasSensorUpdate()
{
}

/*
 * Returns true if the MQ-2 digital output pin indicates gas detection
 * (active LOW: pin goes LOW when gas is above threshold set by onboard pot).
 */
bool gasSensorRead()
{
    return mq2Digital;    
}

/*
 * Returns an approximate gas concentration in ppm derived from the
 * MQ-2 analogue output (0.0 – 1.0 full-scale ADC reading).
 * Calibrate GAS_SENSOR_ANALOG_SCALE to your specific sensor / supply.
 */
float gasSensorAnalogRead()
{
    return mq2Analog.read() * GAS_SENSOR_ANALOG_SCALE;
}

//=====[Implementations of private functions]==================================