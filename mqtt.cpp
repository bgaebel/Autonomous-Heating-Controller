#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "mqtt.h"
#include "config.h"
#include "secrets.h"
#include "control.h"
#include "sensor.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastReconnectAttempt = 0;
unsigned long lastTelemetryPublish = 0;
unsigned long lastStatePublish = 0;
static unsigned long nextReconnectDue = 0;
static unsigned long reconnectDelayMs = 5000;  // start with 5s
static const unsigned long reconnectDelayMaxMs = 120000; // cap at 2min

/***************** mqttCallback *************************************************
 * Description:
 * Handles incoming MQTT messages.
 * Parses JSON payloads and updates controller state accordingly.
 ******************************************************************************/
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error)
  {
    Serial.print(F("[MQTT] JSON parse failed: "));
    Serial.println(error.f_str());
    return;
  }

  if (doc["setPoint"].is<float>())     { setDaySetPoint(doc["setPoint"]); setNightSetPoint(doc["setPoint"]); }
  if (doc["daySetPoint"].is<float>())  { setDaySetPoint(doc["daySetPoint"]); }
  if (doc["nightSetPoint"].is<float>()){ setNightSetPoint(doc["nightSetPoint"]); }
  if (doc["dayStart"].is<int>())       { setDayStartMinutes(doc["dayStart"]); }
  if (doc["nightStart"].is<int>())     { setNightStartMinutes(doc["nightStart"]); }
  if (doc["hysteresis"].is<float>())   { setHysteresis(doc["hysteresis"]); }
  if (doc["boostMinutes"].is<int>())   { setBoostMinutes(doc["boostMinutes"]); }
  if (doc["mode"].is<int>())           { setControlMode((ControlMode)doc["mode"]); }

  publishState();
}

/***************** mqttReconnect ************************************************
 * Description:
 * Attempts to connect to the MQTT broker.
 ******************************************************************************/
bool mqttReconnect()
{
  Serial.print(F("[MQTT] Connecting to broker "));
  Serial.print(MQTT_HOST);
  Serial.println(F(":1883 ..."));

  String clientId = "HeatCtrl-" + String(ESP.getChipId(), HEX);
  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
  {
    Serial.println(F("[MQTT] Connected!"));

    String topic = String(BASE_TOPIC) + "/cmd";
    mqttClient.subscribe(topic.c_str());
    Serial.print(F("[MQTT] Subscribed to "));
    Serial.println(topic);
    return true;
  }
  else
  {
    Serial.print(F("[MQTT] Connect failed, state="));
    Serial.println(mqttClient.state());
    return false;
  }
}

/***************** ensureMQTT ***************************************************
 * Description:
 * Ensures MQTT connection is alive.
 * Exponential backoff reconnect; LED overlays indicate status.
 ******************************************************************************/
void ensureMQTT()
{
  if (!mqttClient.connected())
  {
    // indicate MQTT problem while disconnected (unless WiFi overlay masks it)
    unsigned long now = millis();
    if (now >= nextReconnectDue)
    {
      if (mqttReconnect())
      {
        reconnectDelayMs = 5000;
        nextReconnectDue = 0;
      }
      else
      {
        reconnectDelayMs = min(reconnectDelayMs * 2, reconnectDelayMaxMs);
        nextReconnectDue = now + reconnectDelayMs;
        Serial.printf("[MQTT] Next reconnect in %lu s\n", reconnectDelayMs/1000);
      }
    }
  }
  else
  {
    // connected – clear MQTT overlay, keep heartbeat
    mqttClient.loop();
  }
}

/***************** publishTelemetry ********************************************
 * Description:
 * Publishes current telemetry (temperature, humidity, state) via MQTT.
 ******************************************************************************/
void publishTelemetry()
{
  if (!mqttClient.connected())
  {
    return;
  }

  StaticJsonDocument<256> doc;
  doc["temp"]     = isnan(getLastTemperature()) ? 0 : getLastTemperature();
  doc["humidity"] = isnan(getLastHumidity()) ? 0 : getLastHumidity();
  doc["state"]    = stateToStr(getControlState());
  doc["mode"]     = modeToStr(getControlMode());

  String payload;
  serializeJson(doc, payload);
  String topic = String(BASE_TOPIC) + "/telemetry";

  if (mqttClient.publish(topic.c_str(), payload.c_str()))
  {
    Serial.println(F("[MQTT] Telemetry published"));
  }
  else
  {
    Serial.println(F("[MQTT] Telemetry publish failed"));
  }
}

/***************** publishState *************************************************
 * Description:
 * Publishes controller configuration and state.
 ******************************************************************************/
void publishState()
{
  if (!mqttClient.connected())
  {
    return;
  }

  StaticJsonDocument<384> doc;
  doc["setPoint"]      = getSetPoint();
  doc["daySetPoint"]   = getDaySetPoint();
  doc["nightSetPoint"] = getNightSetPoint();
  doc["dayStart"]      = getDayStartMinutes();
  doc["nightStart"]    = getNightStartMinutes();
  doc["hysteresis"]    = getHysteresis();
  doc["boostMinutes"]  = getBoostMinutes();
  doc["mode"]          = modeToStr(getControlMode());
  doc["state"]         = stateToStr(getControlState());

  String payload;
  serializeJson(doc, payload);
  String topic = String(BASE_TOPIC) + "/state";

  if (mqttClient.publish(topic.c_str(), payload.c_str()))
  {
    Serial.println(F("[MQTT] State published"));
  }
  else
  {
    Serial.println(F("[MQTT] State publish failed"));
  }
}

/***************** initMqtt *****************************************************
 * Description:
 * Initializes MQTT client and sets callback.
 ******************************************************************************/
void initMqtt()
{
  mqttClient.setServer(MQTT_HOST, 1883);
  mqttClient.setCallback(mqttCallback);
  Serial.println(F("[MQTT] Initialized"));
}

/***************** mqttIsConnected *********************************************
 * params: none
 * return: bool
 * Description:
 * Returns whether the MQTT client is currently connected.
 ******************************************************************************/
bool mqttIsConnected()
{
  return mqttClient.connected();
}

/***************** mqttConnStateText ********************************************
 * params: none
 * return: const char*
 * Description:
 * Maps PubSubClient::state() to a human-readable short string.
 ******************************************************************************/
const char* mqttConnStateText()
{
  int s = mqttClient.state();
  switch (s)
  {
    case MQTT_CONNECTED:               return "connected";
    case MQTT_CONNECTION_TIMEOUT:      return "timeout";
    case MQTT_CONNECTION_LOST:         return "lost";
    case MQTT_CONNECT_FAILED:          return "connect-failed";
    case MQTT_DISCONNECTED:            return "disconnected";
    case MQTT_CONNECT_BAD_PROTOCOL:    return "bad-protocol";
    case MQTT_CONNECT_BAD_CLIENT_ID:   return "bad-client-id";
    case MQTT_CONNECT_UNAVAILABLE:     return "unavailable";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "bad-credentials";
    case MQTT_CONNECT_UNAUTHORIZED:    return "unauthorized";
    default:                           return "?";
  }
}

