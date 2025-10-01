#ifndef LED_H
#define LED_H

#include <Arduino.h>

/***************** LED State Definitions ***************************************
 * LED_OFF             : LED always off
 * LED_WIFI_CONNECTING : 2 short blinks → WiFi connecting
 * LED_MQTT_CONNECTING : 3 short blinks → MQTT connecting
 * LED_RUNNING         : 1 short blink → normal operation
 * LED_ERROR           : 5 short blinks → error condition
 ******************************************************************************/
#define LED_OFF               0
#define LED_WIFI_CONNECTING   1
#define LED_MQTT_CONNECTING   2
#define LED_RUNNING           3
#define LED_ERROR             4

/***************** initLed ******************************************************
 * params:
 *   none
 * return: void
 * Description:
 * Initializes the LED pin and sets LED state to OFF.
 ******************************************************************************/
void initLed();

/***************** setLedState **************************************************
 * params:
 *   state: one of LED_OFF, LED_WIFI_CONNECTING, ...
 * return: void
 * Description:
 * Sets the current LED blink pattern to reflect system state.
 ******************************************************************************/
void setLedState(uint8_t state);

/***************** handleLed ****************************************************
 * params: none
 * return: void
 * Description:
 * Non-blocking LED blink handler. Must be called frequently in loop().
 ******************************************************************************/
void handleLed();

#endif
