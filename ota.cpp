#include "ota.h"
#include "led.h"
#include "control.h"
#include "config.h"
#include "secrets.h"
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

/***************** setupOta *************************************/
void initOta()
{
  if (WiFi.status() != WL_CONNECTED) return;

  String chip = String(ESP.getChipId(), HEX);
  String host = "heatctrl-" + chip + "-" + BASE_TOPIC;
  host.toLowerCase(); host.replace(" ", "-");

  ArduinoOTA.setHostname(host.c_str());
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([](){ Serial.println(F("[OTA] Start")); });
  ArduinoOTA.onEnd([](){ Serial.println(F("[OTA] End")); });
  ArduinoOTA.onError([](ota_error_t e){ Serial.printf("[OTA] Error %u\n", e); });

  ArduinoOTA.begin();
  MDNS.begin(host.c_str());
  Serial.printf("[OTA] Ready: %s.local\n", host.c_str());
}
