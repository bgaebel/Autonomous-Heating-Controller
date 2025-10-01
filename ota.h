#ifndef OTA_H
#define OTA_H

/***************** initOta ******************************************************
 * params: none
 * return: void
 * Description:
 * Configures and starts OTA update service.
 * Must be called in setup(); ArduinoOTA.handle() must run in loop().
 ******************************************************************************/
void initOta();

#endif
