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
  setLedState(LED_WIFI_CONNECTING);

  loadConfig();          // from config.cpp
  initWifi();            // try WiFi connection
  initOta();             // set up OTA
  initMqtt();            // MQTT client setup
  initSensor();          // initialize GY-21
  initControl();         // control logic
  initWebServer();       // local web UI

  Serial.println(F("[SYS] Setup complete."));
}

/***************** loop ********************************************************
 * Description:
 * Main loop — handles all non-blocking tasks.
 ******************************************************************************/
void loop()
{
  handleLed();             // LED status indicator
  ensureWifi();            // reconnect WiFi if needed
  mqttReconnectIfNeeded(); // reconnect MQTT if needed
  handleSensor();          // update readings
  handleControl();         // apply control logic
  ArduinoOTA.handle();     // process OTA updates
  handleWebServer();       // serve web requests
}
