#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

/***************** initOta ******************************************************
 * params: none
 * return: void
 * Description:
 * Initializes ArduinoOTA. Call this AFTER WiFi is connected and mDNS is running
 * (i.e., right after startMdns()). Safe to call again after a reconnect.
 ******************************************************************************/
void initOta();

/***************** handleOta ****************************************************
 * params: none
 * return: void
 * Description:
 * Must be called frequently in loop() to process OTA traffic.
 ******************************************************************************/
void handleOta();

/***************** otaIsActive **************************************************
 * params: none
 * return: bool
 * Description:
 * Returns true while an OTA update is in progress. Use it to short-circuit
 * other work in loop() so ArduinoOTA.handle() gets maximum CPU time.
 ******************************************************************************/
bool otaIsActive();

#endif
