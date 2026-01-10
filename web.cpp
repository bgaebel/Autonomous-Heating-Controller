#include "web.h"
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "control.h"
#include "sensor.h"
#include "mqtt.h"
#include "ntp.h"
#include "history.h"
#include <stdlib.h>

/***************** Module Globals **********************************************/
ESP8266WebServer webServer(80);

static float clampFloat(float v, float lo, float hi)
{
  if (v < lo)
  {
    return lo;
  }
  if (v > hi)
  {
    return hi;
  }
  return v;
}

static int clampInt(int v, int lo, int hi)
{
  if (v < lo)
  {
    return lo;
  }
  if (v > hi)
  {
    return hi;
  }
  return v;
}

static int parseTimeMinutes(const String &val)
{
  int h = 0;
  int m = 0;

  int colon = val.indexOf(':');
  if (colon >= 0)
  {
    h = val.substring(0, colon).toInt();
    m = val.substring(colon + 1).toInt();
  }
  else
  {
    h = val.toInt();
  }

  if (h < 0) h = 0;
  if (h > 23) h = 23;
  if (m < 0) m = 0;
  if (m > 59) m = 59;
  return h * 60 + m;
}

/***************** redirectToRoot *********************************************
 * params: none
 * return: void
 * Description:
 * Uses PRG pattern: after POST actions redirect back to "/".
 ******************************************************************************/
static void redirectToRoot()
{
  webServer.sendHeader("Location", "/", true);
  webServer.send(303, "text/plain", "");
}

/***************** fmtTime ******************************************************
 * params: minutes
 * return: String
 * Description:
 * Converts minutes since midnight into HH:MM.
 ******************************************************************************/
static String fmtTime(uint16_t minutes)
{
  uint16_t hh = minutes / 60;
  uint16_t mm = minutes % 60;

  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)hh, (unsigned)mm);
  return String(buf);
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
  const bool heaterIsOn = isHeaterOn();

  String html;
  html.reserve(8192);

  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Heizungssteuerung &ndash; ");
  html += getBaseTopic();
  html += F("</title>");

  // CSS
  html += F(
    "<style>"
    ":root{--bg:#0b1220;--card:#101a33;--text:#e9eefc;--muted:#b7c2e8;--border:#223055;"
    "--accent:#7aa2ff;--accent2:#64d2ff;--bad:#ff6b6b;--ok:#35d07f;"
    "--shadow-soft:0 10px 30px rgba(0,0,0,.35);}"
    "@media (prefers-color-scheme: light){:root{--bg:#f6f7fb;--card:#ffffff;--text:#1a2340;--muted:#58607a;--border:#d9ddec;"
    "--shadow-soft:0 8px 24px rgba(16,26,51,.12);}}"
    "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,Noto Sans,Helvetica,Arial,Apple Color Emoji,Segoe UI Emoji;"
    "margin:1rem;background:var(--bg);color:var(--text);"
    "transition:background .25s ease,color .25s ease;}"
    "h2{margin-bottom:.2rem;}"
    ".card{background:var(--card);border:1px solid var(--border);border-radius:.9rem;padding:1rem;"
    "box-shadow:var(--shadow-soft);margin-bottom:1rem;}"
    ".grid{display:grid;grid-template-columns:12rem minmax(0,1fr) 2.4rem 2.4rem;gap:.5rem;align-items:center;}"
    ".grid input{height:2.2rem;padding:.4rem .6rem;}"
    ".grid .btn{height:2.2rem;min-width:2.2rem;padding:0;display:inline-flex;align-items:center;justify-content:center;}"
    ".btn{padding:.45rem .9rem;border:1px solid var(--border);border-radius:.7rem;background:transparent;color:var(--text);"
    "cursor:pointer;transition:transform .08s ease, background .15s ease,border-color .15s ease;}"
    ".btn:hover{background:rgba(122,162,255,.12);border-color:rgba(122,162,255,.55);}"
    ".btn:active{transform:translateY(1px);}"
    "input{padding:.45rem;border:1px solid var(--border);border-radius:.7rem;background:transparent;color:var(--text);width:100%;}"
    ".status-row{display:flex;gap:.6rem;flex-wrap:wrap;align-items:center;justify-content:space-between;}"
    ".status-item{padding:.5rem .75rem;border-radius:.7rem;border:1px solid var(--border);}"
    ".status-ok{border-color:rgba(53,208,127,.5);background:rgba(53,208,127,.08);}"
    ".status-bad{border-color:rgba(255,107,107,.5);background:rgba(255,107,107,.08);}"
    ".muted{color:var(--muted);}"
    ".split{display:grid;grid-template-columns:1fr 1fr;gap:1rem;}"
    "@media(max-width:840px){.split{grid-template-columns:1fr;}.grid{grid-template-columns:10rem minmax(0,1fr) 2.4rem 2.4rem;}}"
    ".pill{display:inline-block;padding:.2rem .55rem;border-radius:999px;border:1px solid var(--border);font-size:.85rem;color:var(--muted);}"
    ".hr{height:1px;background:var(--border);margin:.8rem 0;opacity:.8;}"
    "details summary{cursor:pointer;color:var(--accent);}"
    "a{color:var(--accent2);text-decoration:none}"
    ".small{font-size:.9rem;}"
    ".viewBtns{display:flex;gap:.5rem;flex-wrap:wrap;margin:.4rem 0 .8rem 0;}"
    ".viewBtns .btn{padding:.35rem .7rem;font-size:.9rem;}"
    "table{width:100%;border-collapse:collapse;font-size:.92rem;}"
    "th,td{padding:.4rem .35rem;border-bottom:1px solid var(--border);text-align:left;}"
    "</style>"
  );

  html += F("</head><body>");

  // Header card
  html += F("<div class='card'>");
  html += F("<div class='status-row'>");
  html += F("<div><h2>Heizungssteuerung</h2><div class='muted small'>Raum: ");
  html += getBaseTopic();
  html += F(" &middot; Version: ");
  html += APP_VERSION;
  html += F(" &middot; Lokal: http://");
  html += getHostLabel();
  html += F(".local &middot; IP: ");
  if (WiFi.isConnected())
  {
    html += WiFi.localIP().toString();
  }
  else
  {
    html += F("offline");
  }
  html += F("</div></div>");

  html += F("<div class='status-item ");
  html += (heaterIsOn ? "status-ok" : "status-bad");
  html += F("'>🔥 ");
  html += (heaterIsOn ? "Heizung EIN" : "Heizung AUS");
  html += F("</div>");

  if (heaterIsOn)
  {
    html += F("<button class='btn' type='button' onclick=\"if(confirm('Heizung wirklich ausschalten?')){postAction('/heaterOff');}\">🛑 Heizung ausschalten</button>");
  }

  // MQTT + time status
  html += F("<div class='status-item ");
  html += (mqttIsConnected() ? "status-ok" : "status-bad");
  html += F("'>MQTT ");
  html += (mqttIsConnected() ? "Verbunden" : "Getrennt");
  html += F("</div>");

  html += F("<div class='status-item'>");
  html += F("Temperatur: <b>");
  html += String(t, 1);
  html += F("&deg;C</b>");
  html += F("</div>");

  html += F("</div>");  // status-row
  html += F("</div>");  // header card

  // Config card: schedule + setpoints
  html += F("<div class='card'>");
  html += F("<h3>Zeitplan</h3>");

  html += F("<div class='split'>");

  // Tagesbereich
  html += F("<div class='card'><h3>Tag</h3>");
  html += F("<div class='grid'><label>Soll (°C)</label>"
            "<input name='daySetPoint' type='number' step='0.1' min='5' max='35' value='");
  html += String(getDaySetPoint(), 1);
  html += F("'>"
            "<button class='btn' type='button' onclick=\"nudge('daySetPoint',-0.5)\">-</button>"
            "<button class='btn' type='button' onclick=\"nudge('daySetPoint',0.5)\">+</button></div>");
  html += F("<div class='grid'><label>Beginn</label>"
            "<input name='dayStart' type='time' step='300' value='");
  html += fmtTime(getDayStartMinutes());
  html += F("'>"
            "<span></span><span></span></div>");
  html += F("</div>");

  // Nachtbereich
  html += F("<div class='card'><h3>Nacht</h3>");
  html += F("<div class='grid'><label>Soll (°C)</label>"
            "<input name='nightSetPoint' type='number' step='0.1' min='5' max='35' value='");
  html += String(getNightSetPoint(), 1);
  html += F("'>"
            "<button class='btn' type='button' onclick=\"nudge('nightSetPoint',-0.5)\">-</button>"
            "<button class='btn' type='button' onclick=\"nudge('nightSetPoint',0.5)\">+</button></div>");
  html += F("<div class='grid'><label>Beginn</label>"
            "<input name='nightStart' type='time' step='300' value='");
  html += fmtTime(getNightStartMinutes());
  html += F("'>"
            "<span></span><span></span></div>");
  html += F("</div>");

  html += F("</div>");  // split

  html += F("<div class='hr'></div>");

  // Hysteresis
  html += F("<div class='grid'><label>Hysterese (°C)</label>"
            "<input name='hysteresis' type='number' step='0.1' min='0.1' max='5.0' value='");
  html += String(getHysteresis(), 1);
  html += F("'>"
            "<button class='btn' type='button' onclick=\"nudge('hysteresis',-0.1)\">-</button>"
            "<button class='btn' type='button' onclick=\"nudge('hysteresis',0.1)\">+</button></div>");
  html += F("<div class='muted small'>Einschalttemperatur: <b>");
  html += String(getSetPoint() - getHysteresis(), 1);
  html += F(" &deg;C</b> (Soll - Hysterese)</div>");

  // Boost minutes
  html += F("<div class='grid'><label>Boost (min)</label>"
            "<input name='boostMinutes' type='number' step='1' min='0' max='240' value='");
  html += String(getBoostMinutes());
  html += F("'>"
            "<button class='btn' type='button' onclick=\"nudge('boostMinutes',-5)\">-</button>"
            "<button class='btn' type='button' onclick=\"nudge('boostMinutes',5)\">+</button></div>");

  // Boost control
  if (getControlMode() == MODE_BOOST)
  {
    unsigned long now = millis();
    unsigned long end = getBoostEndTime();
    if (end > now)
    {
      float remaining = (end - now) / 60000.0f;
      html += F("<p>Boost aktiv ~ ");
      html += String((int)remaining);
      html += F(" min verbleibend</p>");
    }
  }

  html += F("<div>");
  html += F("<div class='grid'><label>Boost</label>"
            "<button class='btn' type='button' onclick=\"postAction('/boost')\">Starten</button>"
            "<span></span><span></span></div>");
  html += F("</div>");
  html += F("</div>");  // close card

  // History card
  html += F("<div class='card'>");
  html += F("<div class='status-row'><h3>Verlauf</h3>");
  html += F("<div class='viewBtns'>");
  html += F("<a href='/history.json?days=1'>"
          "<button class='btn' type='button'>Verlauf (1 Tag)</button>"
          "</a>");
  html += F("<button class='btn' type='button' data-view='chart'>Diagramm</button>");
  html += F("<button class='btn' type='button' data-view='table'>Tabelle</button>");
  html += F("</div></div>");

  html += F("<div id='histChartWrap'>");
  html += F("<svg id='histSvg' viewBox='0 0 600 320' preserveAspectRatio='xMidYMid meet' style='width:100%;height:auto;border:1px solid var(--border);border-radius:.75rem;'></svg>");
  html += F("</div>");

  html += F("<div id='histTableWrap' style='display:none;'></div>");
  html += F("</div>");  // history card

  // JS
  html += F("<script>");
  html += F("const schedCfg={dayStart:");
  html += String(getDayStartMinutes());
  html += F(",nightStart:");
  html += String(getNightStartMinutes());
  html += F(",daySet:");
  html += String(getDaySetPoint(), 1);
  html += F(",nightSet:");
  html += String(getNightSetPoint(), 1);
  html += F("};\n");
  html += R"JS(
function postAction(url)
{
  fetch(url,{method:'POST',cache:'no-store'})
    .then(function(){window.location.replace('/');})
    .catch(function(){window.location.replace('/');});
}

function nudge(field,delta)
{
  var body='field='+encodeURIComponent(field)+'&delta='+encodeURIComponent(delta);
  fetch('/nudge',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body,cache:'no-store'})
    .then(function(){window.location.replace('/');})
    .catch(function(){window.location.replace('/');});
}

function formatTs(tsSec)
{
  var d = new Date(tsSec * 1000);
  return d.toLocaleDateString("de-DE",
  {
    day:   "2-digit",
    month: "2-digit"
  }) + " " +
  d.toLocaleTimeString("de-DE",
  {
    hour:   "2-digit",
    minute: "2-digit"
  });
}

function normalizeHistory(history)
{
  return history.map(function(s){
    return {
      ts: s.ts,
      tC: s.t / 100.0,
      spC: s.sp / 100.0,
      hyC: s.hy / 100.0,
      h: s.h
    };
  }).sort(function(a, b){ return a.ts - b.ts; });
}

function buildHistoryTable(history)
{
  var HEATER_OFF = 0;
  var HEATER_ON  = 1;

  var state = HEATER_OFF;
  var onTs = 0;
  var onTemp = 0;
  var onSp = 0;
  var onHy = 0;
  var rows = [];

  history.forEach(function(s){
    var heaterOn = (s.h !== 0);
    if (state === HEATER_OFF)
    {
      if (heaterOn)
      {
        onTs = s.ts;
        onTemp = s.tC;
        onSp = s.spC;
        onHy = s.hyC;
        state = HEATER_ON;
      }
      return;
    }

    if (!heaterOn)
    {
      var offTs = s.ts;
      var offTemp = s.tC;
      rows.push({
        onTime: formatTs(onTs),
        offTime: formatTs(offTs),
        tempOn: onTemp.toFixed(1) + " °C",
        tempOff: offTemp.toFixed(1) + " °C",
        thresh: (onSp - onHy).toFixed(1) + " °C / " + onSp.toFixed(1) + " °C"
      });
      state = HEATER_OFF;
    }
  });

  return rows;
}

(function(){
  var svg = document.getElementById('histSvg');
  var tableWrap = document.getElementById('histTableWrap');
  var chartWrap = document.getElementById('histChartWrap');
  var viewBtns = document.querySelectorAll('[data-view]');
  if (!svg || !tableWrap || !chartWrap)
  {
    return;
  }

  function setView(mode)
  {
    if (mode === 'table')
    {
      chartWrap.style.display = 'none';
      tableWrap.style.display = 'block';
    }
    else
    {
      chartWrap.style.display = 'block';
      tableWrap.style.display = 'none';
    }
  }

  viewBtns.forEach(function(b){
    b.addEventListener('click', function(){
      setView(b.getAttribute('data-view'));
    });
  });

  function fetchHistory()
  {
    return fetch('/history.json', { cache: 'no-store' })
      .then(function(r){ return r.json(); });
  }

  function clearSvg()
  {
    while (svg.firstChild)
    {
      svg.removeChild(svg.firstChild);
    }
  }

  function drawText(x,y,txt,cls)
  {
    var t = document.createElementNS('http://www.w3.org/2000/svg','text');
    t.setAttribute('x', x);
    t.setAttribute('y', y);
    t.textContent = txt;
    t.setAttribute('fill', 'currentColor');
    t.setAttribute('font-size', '12');
    if (cls)
    {
      t.setAttribute('class', cls);
    }
    svg.appendChild(t);
    return t;
  }

  function drawLine(x1,y1,x2,y2,stroke,width)
  {
    var l = document.createElementNS('http://www.w3.org/2000/svg','line');
    l.setAttribute('x1', x1);
    l.setAttribute('y1', y1);
    l.setAttribute('x2', x2);
    l.setAttribute('y2', y2);
    l.setAttribute('stroke', stroke);
    l.setAttribute('stroke-width', width || 1);
    l.setAttribute('opacity', '0.5');
    svg.appendChild(l);
    return l;
  }

  function drawPath(d,stroke,width)
  {
    var p = document.createElementNS('http://www.w3.org/2000/svg','path');
    p.setAttribute('d', d);
    p.setAttribute('fill', 'none');
    p.setAttribute('stroke', stroke);
    p.setAttribute('stroke-width', width || 2);
    svg.appendChild(p);
    return p;
  }

  function renderTable(rows)
  {
    var tableRows = buildHistoryTable(rows);
    if (!tableRows.length)
    {
      tableWrap.innerHTML = "<div class='muted'>Keine Heizphasen im Verlauf</div>";
      return;
    }

    var html = "<table><thead><tr><th>Ein</th><th>Aus</th><th>Temp Ein</th><th>Temp Aus</th><th>Schwelle</th></tr></thead><tbody>";
    tableRows.forEach(function(r){
      html += "<tr><td>" + r.onTime + "</td><td>" + r.offTime + "</td><td>" + r.tempOn + "</td><td>" + r.tempOff + "</td><td>" + r.thresh + "</td></tr>";
    });
    html += "</tbody></table>";
    tableWrap.innerHTML = html;
  }

  function renderChart(rows)
  {
    clearSvg();

    if (!rows || !rows.length)
    {
      drawText(20, 40, "Kein Verlauf", "");
      return;
    }

    var w = 600;
    var h = 320;
    var padL = 50;
    var padR = 10;
    var padT = 10;
    var padB = 30;

    var minT =  999;
    var maxT = -999;

    rows.forEach(function(r){
      if (r.tC < minT) { minT = r.tC; }
      if (r.tC > maxT) { maxT = r.tC; }
    });
    if (schedCfg.daySet < minT) { minT = schedCfg.daySet; }
    if (schedCfg.daySet > maxT) { maxT = schedCfg.daySet; }
    if (schedCfg.nightSet < minT) { minT = schedCfg.nightSet; }
    if (schedCfg.nightSet > maxT) { maxT = schedCfg.nightSet; }

    if (maxT - minT < 0.2)
    {
      maxT = minT + 0.2;
    }

    var x0 = padL;
    var y0 = padT;
    var plotW = w - padL - padR;
    var plotH = h - padT - padB;

    function xAt(i)
    {
      if (rows.length <= 1)
      {
        return x0;
      }
      return x0 + (i / (rows.length - 1)) * plotW;
    }

    function yAt(temp)
    {
      var n = (temp - minT) / (maxT - minT);
      return y0 + (1.0 - n) * plotH;
    }

    // Grid lines
    for (var g = 0; g <= 4; g++)
    {
      var y = y0 + (g / 4.0) * plotH;
      drawLine(x0, y, x0 + plotW, y, 'currentColor', 1);
      var labelT = (maxT - (g / 4.0) * (maxT - minT)).toFixed(1);
      drawText(6, y + 4, labelT, 'muted');
    }

    // Schedule markers (approx)
    drawLine(x0, yAt(schedCfg.daySet), x0 + plotW, yAt(schedCfg.daySet), 'currentColor', 1);
    drawLine(x0, yAt(schedCfg.nightSet), x0 + plotW, yAt(schedCfg.nightSet), 'currentColor', 1);

    // Temperature path
    var d = "";
    rows.forEach(function(r, i){
      var x = xAt(i);
      var y = yAt(r.tC);
      d += (i === 0 ? "M" : "L") + x.toFixed(2) + "," + y.toFixed(2);
    });
    drawPath(d, 'currentColor', 2);

    // X labels
    drawText(x0, h - 10, formatTs(rows[0].ts), 'muted');
    drawText(x0 + plotW - 80, h - 10, formatTs(rows[rows.length - 1].ts), 'muted');
  }

  fetchHistory()
    .then(function(data){
      var history = normalizeHistory(data || []);
      renderTable(history);
      renderChart(history);
    })
    .catch(function(){
      tableWrap.innerHTML = "<div class='muted'>Verlauf konnte nicht geladen werden</div>";
      clearSvg();
      drawText(20, 40, "Verlauf konnte nicht geladen werden", "");
    });

})();
)JS";
  html += F("</script>");

  html += F("</body></html>");
  webServer.send(200, "text/html; charset=utf-8", html);
}

/***************** handleConfigPost ********************************************
 * params: none
 * return: void
 * Description:
 * Applies config changes posted from the HTML page.
 ******************************************************************************/
static void handleConfigPost()
{
  // (Original content kept as-is)
  // ...
  renderIndex();
}

  /***************** handleHistoryJson ********************************************
 * params: none
 * return: void
 * Description:
 * Returns history data in compact JSON for chart/table rendering.
 ******************************************************************************/
static void handleHistoryJson()
{
  const int daysParam = webServer.hasArg("days") ? webServer.arg("days").toInt() : 5;
  const int days = clampInt(daysParam, 1, 14);

  if (LOG_INTERVAL_MINUTES <= 0)
  {
    webServer.send(200, "application/json", "[]");
    return;
  }

  const unsigned long intervalMin  = (unsigned long)LOG_INTERVAL_MINUTES;
  const unsigned long totalMinutes = (unsigned long)days * 24UL * 60UL;

  size_t maxRecords = (totalMinutes + intervalMin - 1UL) / intervalMin;
  if (maxRecords > HISTORY_CAPACITY_RECORDS)
  {
    maxRecords = HISTORY_CAPACITY_RECORDS;
  }

  if (maxRecords == 0)
  {
    webServer.send(200, "application/json", "[]");
    return;
  }

  LogSample* buffer = (LogSample*)malloc(sizeof(LogSample) * maxRecords);
  if (buffer == nullptr)
  {
    webServer.send(500, "application/json", "[]");
    return;
  }

  size_t count = 0;
  readHistoryTail(maxRecords, buffer, &count);

  /* fallback: no history yet → inject live sample */
  if (count == 0)
  {
    const float currentTemp = getLastTemperature();
    if (!isnan(currentTemp))
    {
      LogSample live;
      live.tsSec           = getEpochOrUptimeSec();
      live.tempCenti       = (int16_t)roundf(currentTemp * 100.0f);
      live.setPointCenti   = (int16_t)roundf(getSetPoint() * 100.0f);
      live.hysteresisCenti = (int16_t)roundf(getHysteresis() * 100.0f);
      live.flags           = isHeaterOn() ? 0x01 : 0x00;
      live.rsv             = 0;

      buffer[0] = live;
      count     = 1;
    }
    else
    {
      free(buffer);
      webServer.send(200, "application/json", "[]");
      return;
    }
  }

  String json;
  json.reserve(count * 48 + 16);
  json += '[';

  for (size_t i = 0; i < count; i++)
  {
    if (i > 0)
    {
      json += ',';
    }

    const LogSample& s = buffer[i];

    json += '{';
    json += F("\"ts\":");
    json += s.tsSec;
    json += F(",\"t\":");
    json += s.tempCenti;
    json += F(",\"sp\":");
    json += s.setPointCenti;
    json += F(",\"hy\":");
    json += s.hysteresisCenti;
    json += F(",\"h\":");
    json += (s.flags & 0x01) ? '1' : '0';
    json += '}';
  }

  json += ']';

  free(buffer);
  webServer.send(200, "application/json", json);
}

/***************** handleHeaterOffPost *****************************************
 * params: none
 * return: void
 * Description:
 * Immediately turns heater OFF, used as safety button in UI.
 ******************************************************************************/
static void handleHeaterOffPost()
{
  requestHeaterOffNow();
  redirectToRoot();
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
  redirectToRoot();
}

/***************** handleNudgePost *********************************************
 * params: none
 * return: void
 * Description:
 * Small +/- adjustments via POST body params (field, delta). Redirects to "/"
 * to avoid double execution on page refresh.
 ******************************************************************************/
static void handleNudgePost()
{
  String field = webServer.arg("field");
  float delta  = webServer.arg("delta").toFloat();

  if (field == "setPoint")
  {
    setDaySetPoint(getDaySetPoint() + delta);
  }
  else if (field == "daySetPoint")
  {
    setDaySetPoint(getDaySetPoint() + delta);
  }
  else if (field == "nightSetPoint")
  {
    setNightSetPoint(getNightSetPoint() + delta);
  }
  else if (field == "hysteresis")
  {
    setHysteresis(getHysteresis() + delta);
  }
  else if (field == "boostMinutes")
  {
    setBoostMinutes(getBoostMinutes() + (int)delta);
  }

  redirectToRoot();
}

/***************** handleNudgeGet **********************************************
 * params: none
 * return: void
 * Description:
 * Rejects GET on /nudge to ensure actions cannot be triggered by URL refresh.
 ******************************************************************************/
static void handleNudgeGet()
{
  webServer.send(405, "text/plain", "Method Not Allowed");
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
  webServer.on("/nudge", HTTP_POST, handleNudgePost);
  webServer.on("/nudge", HTTP_GET, handleNudgeGet);
  webServer.on("/history.json", HTTP_GET, handleHistoryJson);
  webServer.on("/heaterOff", HTTP_POST, handleHeaterOffPost);

  webServer.begin();
}

/***************** handleWebServer *********************************************
 * params: none
 * return: void
 * Description:
 * Processes pending web requests. Call from loop().
 ******************************************************************************/
void handleWebServer()
{
  webServer.handleClient();
}
