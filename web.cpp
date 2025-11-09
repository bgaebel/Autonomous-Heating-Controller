#include "web.h"
#include <ESP8266WebServer.h>   // Use WebServer for ESP32
#include "config.h"
#include "control.h"
#include "sensor.h"
#include "mqtt.h"

/***************** Module Globals **********************************************/
ESP8266WebServer webServer(80);

static float clampFloat(float v, float lo, float hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int clampInt(int v, int lo, int hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/***************** renderIndex **************************************************
 * params: none
 * return: void
 * Description:
 * Renders status & configuration page, including current heater state (ON/OFF)
 * and the current room (Base-Topic). Sends UTF-8 so special chars render fine.
 ******************************************************************************/
static void renderIndex()
{
  const float t = getLastTemperature();
  const bool heaterIsOn = digitalRead(RELAY_PIN);

  String html;
  html.reserve(4096);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F("</title>");
  html += F("<style>body{font-family:sans-serif;margin:1rem} .grid{display:grid;grid-template-columns:12rem 1fr auto auto;gap:.5rem;align-items:center} .btn{padding:.4rem .8rem;border:1px solid #ccc;border-radius:.5rem;text-decoration:none} input{padding:.4rem} .select-lg{font-size:1.1rem;padding:.55rem .9rem;min-width:12rem;height:2.3rem}</style>");
  html += F("</head><body>");

  // Header mit Raum/Topic
  html += F("<h2>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F("</h2>");
  html += F("<div>Host: ");
  html += getHostLabel();
  html += F(".local</div>");
  html += F("<div>Version: ");
  html += getFirmwareVersion();
  html += F("</div>");

  // Einzige Statuszeile
  html += F("<div>Status: Mode=");
  html += modeToStr(getControlMode());
  html += F(" | Heater=");
  html += (heaterIsOn ? "ON" : "OFF");
  html += F(" | Temp=");
  html += (isnan(t) ? String("NaN") : String(t, 1));
  html += F(" &deg;C");
  html += F(" | MQTT=");
  if (mqttIsConnected())
  {
    html += F("OK");
  }
  else
  {
    html += F("DOWN (");
    html += mqttConnStateText();
    html += F(")");
  }
  html += F("</div><hr>");

  html += F("<form method='POST' action='/config'>");

  // Setpoint
  html += F("<div class='grid'><label>Setpoint (°C)</label>"
            "<input name='setPoint' type='number' step='0.1' min='5' max='35' value='");
  html += String(getSetPoint(), 1);
  html += F("'>"
            "<a class='btn' href='/nudge?field=setPoint&delta=-0.5'>-</a>"
            "<a class='btn' href='/nudge?field=setPoint&delta=0.5'>+</a></div>");

  // Hysteresis
  html += F("<div class='grid'><label>Hysteresis (°C)</label>"
            "<input name='hysteresis' type='number' step='0.1' min='0.1' max='5' value='");
  html += String(getHysteresis(), 1);
  html += F("'>"
            "<a class='btn' href='/nudge?field=hysteresis&delta=-0.1'>-</a>"
            "<a class='btn' href='/nudge?field=hysteresis&delta=0.1'>+</a></div>");

  // Boost minutes
  html += F("<div class='grid'><label>Boost (min)</label>"
            "<input name='boostMinutes' type='number' step='1' min='0' max='240' value='");
  html += String(getBoostMinutes());
  html += F("'>"
            "<a class='btn' href='/nudge?field=boostMinutes&delta=-5'>-</a>"
            "<a class='btn' href='/nudge?field=boostMinutes&delta=5'>+</a></div>");

  // Mode select
  html += F("<div class='grid'><label>Mode</label><select name='mode' class='select-lg'>");
  ControlMode m = getControlMode();
  html += F("<option value='0'"); if (m == MODE_AUTO)  html += F(" selected"); html += F(">AUTO</option>");
  html += F("<option value='1'"); if (m == MODE_OFF)   html += F(" selected"); html += F(">OFF</option>");
  html += F("<option value='2'"); if (m == MODE_BOOST) html += F(" selected"); html += F(">BOOST</option>");
  html += F("</select><span></span><span></span></div>");

  html += F("<div class='grid'><span></span><button class='btn' type='submit'>Save</button><span></span><span></span></div>");
  html += F("</form>");

  // Boost control
  html += F("<h3>Boost</h3>");
  if (getControlMode() == MODE_BOOST)
  {
    unsigned long remaining = 0;
    unsigned long end = getBoostEndTime();
    if (end > millis()) remaining = (end - millis()) / 60000UL;
    html += F("<p>Boost active ~ ");
    html += String((int)remaining);
    html += F(" min remaining</p>");
  }
  html += F("<form method='POST' action='/boost'>");
  html += F("<div class='grid'><label>Start Boost</label><button class='btn' type='submit'>Start</button><span></span><span></span></div>");
  html += F("</form>");

  html += F("</body></html>");

  // WICHTIG: UTF-8 senden, sonst gibt's â€“ o.ä.
  webServer.send(200, "text/html; charset=utf-8", html);
}


/***************** handleConfigPost *********************************************
 * params: none
 * return: void
 * Description:
 * Parses config form and persists values via Config setters.
 ******************************************************************************/
static void handleConfigPost()
{
  float sp = clampFloat(webServer.arg("setPoint").toFloat(), 5.0f, 35.0f);
  float hy = clampFloat(webServer.arg("hysteresis").toFloat(), 0.1f, 5.0f);
  int   bm = clampInt(webServer.arg("boostMinutes").toInt(), 0, 240);
  int   md = webServer.arg("mode").toInt();

  setSetPoint(sp);
  setHysteresis(hy);
  setBoostMinutes(bm);
  setControlMode((ControlMode)md);

  renderIndex();
}

/***************** handleBoostPost **********************************************
 * params: none
 * return: void
 * Description:
 * Starts BOOST using configured duration and persists end timestamp.
 ******************************************************************************/
static void handleBoostPost()
{
  unsigned long now = millis();
  unsigned long end = now + (unsigned long)getBoostMinutes() * 60000UL;
  setBoostEndTime(end);
  setControlMode(MODE_BOOST);
  renderIndex();
}

/***************** handleNudge **************************************************
 * params: none
 * return: void
 * Description:
 * Small +/- adjustments via query params; persists via setters.
 ******************************************************************************/
static void handleNudge()
{
  String field = webServer.arg("field");
  float delta  = webServer.arg("delta").toFloat();

  if (field == "setPoint")
  {
    setSetPoint(getSetPoint() + delta);
  }
  else if (field == "hysteresis")
  {
    setHysteresis(getHysteresis() + delta);
  }
  else if (field == "boostMinutes")
  {
    setBoostMinutes(getBoostMinutes() + (int)delta);
  }

  renderIndex();
}

/***************** initWebServer ************************************************
 * params: none
 * return: void
 * Description:
 * Initializes HTTP routes. Assumes loadConfig() was already called in setup().
 ******************************************************************************/
void initWebServer()
{
  webServer.on("/", HTTP_GET, renderIndex);
  webServer.on("/config", HTTP_POST, handleConfigPost);
  webServer.on("/boost", HTTP_POST, handleBoostPost);
  webServer.on("/nudge", HTTP_GET, handleNudge);
  webServer.begin();
}

/***************** handleWebServer **********************************************
 * params: none
 * return: void
 * Description:
 * Handle client requests; call frequently in loop().
 ******************************************************************************/
void handleWebServer()
{
  webServer.handleClient();
}
