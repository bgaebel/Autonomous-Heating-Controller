#ifndef LED_H
#define LED_H

#include <Arduino.h>

/***************** Concept ******************************************************
 * Base (Heater) is always shown.
 * Every LED_GROUP_PERIOD_MS, show one "signature group" if any subsystem fails:
 *   Pulse #1 -> WiFi   (short = OK, long = FAIL)
 *   Pulse #2 -> MQTT   (short = OK, long = FAIL)
 *   Pulse #3 -> Sensor (short = OK, long = FAIL)
 * Pulses invert the base (XOR). No other heartbeat.
 ******************************************************************************/

#ifndef LED_PIN
#define LED_PIN LED_BUILTIN
#endif

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 1   // 1 = active-low (ESP Onboard-LED), 0 = active-high
#endif

/***************** Timing (ms) **************************************************/
#ifndef LED_PULSE_SHORT_MS
#define LED_PULSE_SHORT_MS   120UL
#endif

#ifndef LED_PULSE_LONG_MS
#define LED_PULSE_LONG_MS    420UL
#endif

#ifndef LED_INTER_PULSE_GAP_MS
#define LED_INTER_PULSE_GAP_MS 150UL
#endif

#ifndef LED_GROUP_PERIOD_MS
#define LED_GROUP_PERIOD_MS  300000UL   // 5 Minuten zwischen Gruppen
#endif

/***************** API **********************************************************/

/***************** initLed ******************************************************
 * params: none
 * return: void
 * Description:
 * Initializes LED GPIO and sets default (base OFF, subsystems all unknown=false).
 ******************************************************************************/
void initLed();

/***************** ledSetBaseFromHeater *****************************************
 * params:
 *   heaterOn: true if relay is ON
 * return: void
 * Description:
 * Mirrors the heater state as LED base (final LED = base XOR overlayPulse).
 ******************************************************************************/
void ledSetBaseFromHeater(bool heaterOn);

/***************** ledSetSubsystemStatus ****************************************
 * params:
 *   wifiOk, mqttOk, sensorOk
 * return: void
 * Description:
 * Sets subsystem health flags (true = OK). The signature group is shown only
 * if at least one of them is false, and only every LED_GROUP_PERIOD_MS.
 ******************************************************************************/
void ledSetSubsystemStatus(bool wifiOk, bool mqttOk, bool sensorOk);

/***************** Convenience setters ******************************************/
void ledSetWifiOk(bool ok);
void ledSetMqttOk(bool ok);
void ledSetSensorOk(bool ok);

/***************** handleLed ****************************************************
 * params: none
 * return: void
 * Description:
 * Non-blocking LED driver. Call frequently in loop().
 ******************************************************************************/
void handleLed();

#endif
