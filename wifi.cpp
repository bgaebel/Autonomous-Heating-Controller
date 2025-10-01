#include <ESP8266WiFi.h>
#include "wifi.h"
#include "led.h"
#include "secrets.h"

/***************** ensureWifi ***************************************************
 * Description:
 * Ensures that the device is connected to the configured WiFi network.
 * If disconnected, tries to reconnect.
 * Updates LED state and logs progress via Serial.
 ******************************************************************************/
void ensureWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("[WIFI] Not connected. Trying to connect..."));
    setLedState(LED_WIFI_CONNECTING);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long startAttempt = millis();
    const unsigned long timeout = 15000; // 15s timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout)
    {
      handleLed();
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print(F("[WIFI] Connected! IP: "));
      Serial.println(WiFi.localIP());
      setLedState(LED_RUNNING);
    }
    else
    {
      Serial.println(F("[WIFI] Connection failed! Entering fallback."));
      setLedState(LED_ERROR);
    }
  }
}

/***************** initWifi *****************************************************
 * Description:
 * Initializes the WiFi connection at system startup.
 * Called once in setup().
 ******************************************************************************/
void initWifi()
{
  Serial.println(F("[WIFI] Initializing..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  setLedState(LED_WIFI_CONNECTING);

  unsigned long startAttempt = millis();
  const unsigned long timeout = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout)
  {
    handleLed();
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("[WIFI] Connected! IP: "));
    Serial.println(WiFi.localIP());
    setLedState(LED_RUNNING);
  }
  else
  {
    Serial.println(F("[WIFI] Failed to connect. Check credentials."));
    setLedState(LED_ERROR);
  }
}
