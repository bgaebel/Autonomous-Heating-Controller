/*******************************************************
 *  Heizungssteuerung MQTT – Rev11
 *  ESP8266 (Wemos D1 mini)
 *  Sensor: GY-21 (I²C)
 *  Relais: D6 (HIGH = AN)
 *******************************************************/

// ========== Device Selection =========================
// Nur **eine** Option aktivieren!
#define DEVICE_WOHNZIMMER
// #define DEVICE_SCHLAFZIMMER
// #define DEVICE_KUECHE
// #define DEVICE_KL_KINDERZIMMER
// #define DEVICE_GR_KINDERZIMMER
// =====================================================

// ---------- BaseTopic je nach Gerät -------------------
#if defined(DEVICE_WOHNZIMMER)
  const char* BASE_TOPIC = "Wohnzimmer";
#elif defined(DEVICE_SCHLAFZIMMER)
  const char* BASE_TOPIC = "Schlafzimmer";
#elif defined(DEVICE_KUECHE)
  const char* BASE_TOPIC = "Küche";
#elif defined(DEVICE_KL_KINDERZIMMER)
  const char* BASE_TOPIC = "Kl. Kinderzimmer";
#elif defined(DEVICE_GR_KINDERZIMMER)
  const char* BASE_TOPIC = "Gr. Kinderzimmer";
#else
  #error "⚠️ Kein Gerät ausgewählt! Bitte oben ein #define aktivieren."
#endif
// ======================================================

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include "secrets.h"   // enthält WIFI_SSID, WIFI_PASS, MQTT_HOST, MQTT_USER, MQTT_PASS

// ---------- Pins ----------
#define PIN_HEATER D6
#define SDA_PIN    D2
#define SCL_PIN    D1

// ---------- Globale Objekte ----------
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
ESP8266WebServer webServer(80);

// ---------- Variablen ----------
float lastMeasurement = NAN;
float lastHumidity    = NAN;

struct Config
{
  float setPoint      = 21.0;
  float hysteresis    = 0.6;
  String mode         = "auto";  // auto/off/summer/boost
  uint16_t boostMinutes = 30;
} config;

enum HeatState
{
  HEAT_OFF,
  HEAT_ON
};
HeatState heatState = HEAT_OFF;

// ======================================================
// ================  SETUP  ==============================
void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(100);
  Serial.println();
  Serial.println(F("== HeatCtrl ESP8266 starting =="));
  Serial.printf("ChipID (HEX): 0x%06X\n", ESP.getChipId());
  Serial.print(F("Device BaseTopic: "));
  Serial.println(BASE_TOPIC);

  pinMode(PIN_HEATER, OUTPUT);
  digitalWrite(PIN_HEATER, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  // ---- WLAN ----
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print(F("[WiFi] Connecting"));
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print(F("[WiFi] Connected: "));
  Serial.println(WiFi.localIP());

  // ---- OTA ----
  ArduinoOTA.setHostname(BASE_TOPIC);
  ArduinoOTA.begin();
  Serial.println(F("[OTA] Ready"));

  // ---- MQTT ----
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // ---- Webserver ----
  setupWeb();

  Serial.println(F("[Setup] Completed"));
}

// ======================================================
// ================  LOOP  ===============================
void loop()
{
  ArduinoOTA.handle();
  mqttLoop();
  webServer.handleClient();

  // Temperatur lesen (Pseudo)
  readSensor();

  // Regelung
  controlLoop();

  delay(1000);
}

// ======================================================
// ================  MQTT  ===============================
void mqttConnect()
{
  if (mqttClient.connected()) return;

  Serial.println(F("[MQTT] Connecting..."));

  String clientId = "HeatCtrl-" + String(ESP.getChipId(), HEX);

  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
  {
    Serial.println(F("[MQTT] Connected"));
    String topicSet = String(BASE_TOPIC) + "/set";
    mqttClient.subscribe(topicSet.c_str());
  }
  else
  {
    Serial.print(F("[MQTT] Failed, rc="));
    Serial.println(mqttClient.state());
  }
}

void mqttLoop()
{
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqttClient.connected()) mqttConnect();
  mqttClient.loop();
}

void mqttCallback(char* topic, byte* payload, unsigned int len)
{
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) return;

  if (doc.containsKey("setPoint"))    config.setPoint = doc["setPoint"];
  if (doc.containsKey("hysteresis"))  config.hysteresis = doc["hysteresis"];
  if (doc.containsKey("mode"))        config.mode = doc["mode"].as<String>();
  if (doc.containsKey("boostMinutes"))config.boostMinutes = (uint16_t)max(0, (int)doc["boostMinutes"]);

  Serial.println(F("[MQTT] Config updated"));
}

void publishTelemetry()
{
  if (!mqttClient.connected()) return;

  StaticJsonDocument<256> doc;
  if (!isnan(lastMeasurement)) doc["temp"] = lastMeasurement;
  if (!isnan(lastHumidity))    doc["humidity"] = lastHumidity;
  doc["heaterOn"] = (heatState == HEAT_ON);

  char buf[256];
  size_t n = serializeJson(doc, buf);
  String topic = String(BASE_TOPIC) + "/telemetry";
  mqttClient.publish(topic.c_str(), buf, n);
}

// ======================================================
// ================  SENSOR  =============================
void readSensor()
{
  // hier echten GY-21-Code einfügen; Beispielwerte:
  lastMeasurement = 21.5;
  lastHumidity    = 45.0;
}

// ======================================================
// ================  REGELUNG  ==========================
void controlLoop()
{
  if (config.mode == "off")
  {
    heatState = HEAT_OFF;
  }
  else if (config.mode == "summer")
  {
    heatState = HEAT_OFF;   // Summer-Mode deaktiviert Heizung
  }
  else // auto / boost
  {
    if (!isnan(lastMeasurement))
    {
      float spHigh = config.setPoint + config.hysteresis / 2;
      float spLow  = config.setPoint - config.hysteresis / 2;
      if (lastMeasurement < spLow)  heatState = HEAT_ON;
      if (lastMeasurement > spHigh) heatState = HEAT_OFF;
    }
  }

  digitalWrite(PIN_HEATER, (heatState == HEAT_ON) ? HIGH : LOW);
  publishTelemetry();
}

// ======================================================
// ================  WEBSERVER  =========================
void setupWeb()
{
  webServer.on("/", handleRoot);
  webServer.begin();
  Serial.println(F("[WEB] Started on port 80"));
}

void handleRoot()
{
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");
  webServer.sendContent("<html><body><h2>HeatCtrl ");
  webServer.sendContent(BASE_TOPIC);
  webServer.sendContent("</h2>");
  webServer.sendContent("<p>SetPoint: " + String(config.setPoint) + "</p>");
  webServer.sendContent("<p>Temp: " + String(lastMeasurement) + "</p>");
  webServer.sendContent("</body></html>");
  webServer.sendContent("");
}
