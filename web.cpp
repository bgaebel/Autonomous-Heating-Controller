#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "web.h"
#include "sensor.h"
#include "control.h"

ESP8266WebServer webServer(80);

void handleRoot()
{
  String html = "<html><body>";
  html += "<h2>Heating Controller</h2>";
  html += "<p>Temp: " + String(getLastTemperature(), 1) + " °C</p>";
  html += "<p>Humidity: " + String(getLastHumidity(), 0) + " %</p>";
  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

void initWebServer()
{
  webServer.on("/", handleRoot);
  webServer.begin();
  Serial.println(F("[WEB] Server started"));
}

void handleWebServer()
{
  webServer.handleClient();
}
