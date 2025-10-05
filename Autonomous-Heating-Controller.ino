#include <Arduino.h>
#include "config.h"
#include "secrets.h"
#include "wifi.h"
#include "mqtt.h"
#include "sensor.h"
#include "control.h"
#include "ota.h"
#include "web.h"
#include "led.h"
#include <ArduinoOTA.h>
#include "history.h"
#include "ntp.h"

/***************** setup *******************************************************
 * Description:
 * Initializes serial, LED, WiFi, MQTT, OTA, and control logic.
 ******************************************************************************/
void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("== Autonomous Heating Controller starting =="));
  Serial.printf("ChipID (HEX): 0x%06X\n", ESP.getChipId());

  initLed();

  loadConfig();          // from config.cpp
  initWifi();            // try WiFi connection
  initOta();             // set up OTA
  initMqtt();            // MQTT client setup
  initSensor();          // initialize GY-21
  initControl();         // control logic
  initWebServer();       // local web UI
  initNtp();        // falls noch nicht aufgerufen; ist idempotent
  initHistory();    // mount FS + Ringpuffer bereitstellen

  Serial.println(F("[SYS] Setup complete."));
}

/***************** loop ********************************************************
 * Description:
 * Main loop — handles all non-blocking tasks.
 ******************************************************************************/
void loop()
{
  if (otaIsActive())
  {
    handleOta();   // volle Bandbreite fürs OTA
    handleMdns();
    ntpTick();
    return;
  }
  handleLed();             // LED status indicator
  ensureWifi();            // reconnect WiFi if needed
  ensureMQTT();            // reconnect MQTT if needed
  handleSensor();          // update readings
  handleControl();         // apply control logic
  handleOta();             // process OTA updates
  handleWebServer();       // serve web requests
  handleMdns();
  ntpTick();               // <<< NTP tick
  handleHistory();         // log history
}
