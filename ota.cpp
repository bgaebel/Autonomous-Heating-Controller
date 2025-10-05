#include "ota.h"
#include "config.h"
#include <ArduinoOTA.h>

static volatile bool g_otaActive = false;

/***************** initOta ******************************************************
 * params: none
 * return: void
 * Description:
 * Initializes ArduinoOTA AFTER WiFi + mDNS are up. Does NOT start mDNS.
 ******************************************************************************/
void initOta()
{
  ArduinoOTA.setHostname(getHostLabel());
  ArduinoOTA.onStart([]()
  {
    g_otaActive = true;
    // Optional: hier ggf. kurz Dinge drosseln (MQTT publish stoppen etc.)
    Serial.println(F("[OTA] Start (fast-path engaged)"));
  });

  ArduinoOTA.onEnd([]()
  {
    Serial.println(F("[OTA] End"));
    g_otaActive = false;
  });

  ArduinoOTA.onError([](ota_error_t e)
  {
    Serial.printf("[OTA] Error %u\n", e);
    g_otaActive = false;
  });

  ArduinoOTA.begin();
  Serial.println(F("[OTA] Ready (announced via mDNS)"));
}

/***************** handleOta ****************************************************/
void handleOta()
{
  ArduinoOTA.handle();
}

/***************** otaIsActive **************************************************/
bool otaIsActive()
{
  return g_otaActive;
}
