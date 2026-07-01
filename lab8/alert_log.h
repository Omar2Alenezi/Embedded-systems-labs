//=====[#ifndef #define #endif]=================================================

#ifndef _ALERT_LOG_H_
#define _ALERT_LOG_H_

//=====[Declaration of public defines]=========================================

// Temperature alert window (°C)
#define ALERT_TEMP_MIN_C    25.0f
#define ALERT_TEMP_MAX_C    40.0f

// Gas alert window (approximate ppm from analogue read × scale factor)
#define ALERT_GAS_MIN_PPM   300.0f
#define ALERT_GAS_MAX_PPM   600.0f

// Cooldown between successive alerts for the same sensor (ms)
#define ALERT_COOLDOWN_MS   5000

//=====[Declaration of public data types]======================================

typedef enum {
    ALERT_SENSOR_NONE        = 0x00,
    ALERT_SENSOR_TEMPERATURE = 0x01,
    ALERT_SENSOR_GAS         = 0x02,
    ALERT_SENSOR_BOTH        = 0x03
} alertSensorMask_t;

//=====[Declarations (prototypes) of public functions]=========================

void alertLogInit();
void alertLogUpdate();                          // call every system tick

// Keypad-driven sensor selection
void alertLogSetMonitoredSensors( alertSensorMask_t mask );
alertSensorMask_t alertLogGetMonitoredSensors();

// Read all logged alerts from the SD card and print via pcSerialCom
void alertLogReadFromSdCard();

#endif // _ALERT_LOG_H_