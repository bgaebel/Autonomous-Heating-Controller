#ifndef SENSOR_H
#define SENSOR_H

/***************** initSensor ***************************************************
 * Initializes the GY-21 (Si7021) sensor via I2C and validates communication.
 ******************************************************************************/
bool initSensor();

/***************** handleSensor ***************************************************
 * Non-blocking periodic read of temperature and humidity. Call frequently.
 ******************************************************************************/
void handleSensor();

/***************** Getters ******************************************************
 * Access the latest measured values without exposing globals.
 ******************************************************************************/
float getLastTemperature();
float getLastHumidity();

#endif
