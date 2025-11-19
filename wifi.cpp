#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "wifi.h"
#include "config.h"
#include "secrets.h"
#include "ota.h"

/***************** applyDhcpHostname *******************************************
 * params: none
 * return: void
 * Description:
 * Sets the DHCP hostname BEFORE WiFi.begin(). Router DNS can resolve
 * <host>.fritz.box to the device IP afterwards.
 ******************************************************************************/
void applyDhcpHostname()
{
#ifdef ARDUINO_ARCH_ESP8266
  WiFi.hostname(getHostLabel()); // z.B. "wohnzimmer-heizung"
#else
  WiFi.setHostname(getHostLabel());
#endif
}

/***************** startMdns ****************************************************
 * params: none
 * return: void
 * Description:
 * Starts (or restarts) mDNS AFTER WiFi is connected and has an IP address.
 * Adds HTTP service and performs a gratuitous announce.
 ******************************************************************************/
void startMdns()
{
  MDNS.end();  // defensiv

  bool ok = false;

#ifdef ARDUINO_ARCH_ESP8266
  // ESP8266: das Overload mit IP verhält sich je nach Core; nimm die einfache Variante
  ok = MDNS.begin(getHostLabel());   // "wohnzimmer-heizung"
#else
  // ESP32 ist hier toleranter – beides geht. Nimm ohne IP für Konsistenz.
  ok = MDNS.begin(getHostLabel());
#endif

  if (!ok)
  {
    Serial.println(F("[mDNS] MDNS.begin failed"));
    return;
  }

  MDNS.addService("http", "tcp", 80);

  // Manche Stacks reagieren erst nach einem Announce vernünftig auf A-Queries
  MDNS.announce();

  Serial.print(F("[mDNS] Started as "));
  Serial.print(getHostLabel());
  Serial.print(F(".local @ "));
  Serial.println(WiFi.localIP());
}


/***************** handleMdns ***************************************************
 * params: none
 * return: void
 * Description:
 * Keeps the mDNS responder responsive (ESP8266).
 ******************************************************************************/
void handleMdns()
{
  MDNS.update();
}


/***************** ensureWifi ***************************************************
 * Description:
 * Ensures that the device is connected to the configured WiFi network.
 * If disconnected, tries to reconnect (blocking attempt with short timeouts).
 ******************************************************************************/
void ensureWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("[WIFI] Not connected. Trying to connect..."));

    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);   // ESP8266
    applyDhcpHostname();
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long startAttempt = millis();
    const unsigned long timeout = 15000; // 15s timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout)
    {
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      startMdns();   // mDNS zuerst
      initOta();
      Serial.print(F("[WIFI] Connected! IP: "));
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.println(F("[WIFI] Connection failed! Will retry later."));
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
  WiFi.setSleepMode(WIFI_NONE_SLEEP);   // ESP8266
  applyDhcpHostname();
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttempt = millis();
  const unsigned long timeout = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout)
  {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    startMdns();   // mDNS zuerst
    initOta();
    Serial.print(F("[WIFI] Connected! IP: "));
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println(F("[WIFI] Failed to connect. Check credentials."));
  }
}
