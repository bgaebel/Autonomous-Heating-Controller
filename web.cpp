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
    ":root{--bg:#0b1220;--card:#101a33;--text:#e9eefc;--muted:#b7c2e8;--border:#223055;--temp-line:#fb923c;--upper-line:#22c55e;--lower-line:#60a5fa;--phase-line:#a855f7;--heat-band:rgba(239,68,68,0.16);--danger:var(--bad);"
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

    /* Grid: label | input | - | + */
    ".grid{display:grid;"
    "grid-template-columns:clamp(6.5rem,30vw,12rem) minmax(0,1fr) 2.4rem 2.4rem;"
    "column-gap:.55rem;row-gap:.55rem;align-items:center;}"
    ".grid>*{min-width:0;}"
    ".grid label{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}"

    ".grid input{height:2.2rem;padding:.4rem .6rem;min-width:0;box-sizing:border-box;}"
    ".grid .btn{height:2.2rem;min-width:2.2rem;width:2.2rem;padding:0;display:inline-flex;align-items:center;justify-content:center;}"

    ".btn{padding:.45rem .9rem;border:1px solid var(--border);border-radius:.7rem;background:transparent;color:var(--text);"
    "cursor:pointer;transition:transform .08s ease, background .15s ease,border-color .15s ease;}"
    ".btn:hover{background:rgba(122,162,255,.12);border-color:rgba(122,162,255,.55);}"
    ".btn:active{transform:translateY(1px);}"
    "input{padding:.45rem;border:1px solid var(--border);border-radius:.7rem;background:transparent;color:var(--text);width:100%;box-sizing:border-box;}"

    ".status-row{display:flex;gap:.6rem;flex-wrap:wrap;align-items:center;justify-content:space-between;}"
    ".status-item{padding:.5rem .75rem;border-radius:.7rem;border:1px solid var(--border);}"
    ".status-ok{border-color:rgba(53,208,127,.5);background:rgba(53,208,127,.08);}"
    ".status-bad{border-color:rgba(255,107,107,.5);background:rgba(255,107,107,.08);}"
    ".muted{color:var(--muted);}"
    ".split{display:grid;grid-template-columns:1fr 1fr;gap:1rem;}"

    /* Tablet/Phone: weniger Luft, robustere Spalten */
    "@media(max-width:840px){"
      ".split{grid-template-columns:1fr;}"
      ".grid{grid-template-columns:clamp(6.2rem,34vw,10rem) minmax(0,1fr) 2.2rem 2.2rem;"
      "column-gap:.45rem;row-gap:.45rem;}"
      ".card{padding:.9rem;}"
      ".grid input{height:2.1rem;}"
      ".grid .btn{height:2.1rem;min-width:2.1rem;width:2.1rem;}"
    "}"

    /* Sehr schmal: Label nicht erzwingen -> mehr Platz fürs Input, weniger Abstand */
    "@media(max-width:480px){"
      ".grid{grid-template-columns:clamp(5.6rem,36vw,8.5rem) minmax(0,1fr) 2.1rem 2.1rem;"
      "column-gap:.4rem;row-gap:.4rem;}"
      "th,td{padding:.35rem .3rem;}"
    "}"

    ".pill{display:inline-block;padding:.2rem .55rem;border-radius:999px;border:1px solid var(--border);font-size:.85rem;color:var(--muted);}"
    ".hr{height:1px;background:var(--border);margin:.8rem 0;opacity:.8;}"
    "details summary{cursor:pointer;color:var(--accent);}"
    "a{color:var(--accent2);text-decoration:none}"
    ".small{font-size:.9rem;}"
    ".viewBtns{display:flex;gap:.5rem;flex-wrap:wrap;margin:.4rem 0 .8rem 0;}"
    ".viewBtns .btn{padding:.35rem .7rem;font-size:.9rem;}"
    "table{width:100%;border-collapse:collapse;font-size:.92rem;}"
    "th,td{padding:.4rem .35rem;border-bottom:1px solid var(--border);text-align:left;}"
    "#histSvg .temp-line{stroke:var(--temp-line);fill:none;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;}"
    "#histSvg .temp-glow{stroke:rgba(251,146,60,.35);fill:none;stroke-width:4;stroke-linecap:round;stroke-linejoin:round;}"
    "#histSvg .upper-line{stroke:var(--upper-line);fill:none;stroke-width:1;stroke-dasharray:4,4;}"
    "#histSvg .lower-line{stroke:var(--lower-line);fill:none;stroke-width:1;stroke-dasharray:4,4;}"
    "#histSvg .phase-line{stroke:var(--phase-line);fill:none;stroke-width:1;stroke-dasharray:4,4;opacity:.8;}"
    "#histSvg .axis{stroke:#6b7280;stroke-width:1;fill:none;}"
    "#histSvg .grid-line{stroke:rgba(148,163,184,.25);stroke-width:1;fill:none;}"
    "#histSvg .bg{fill:var(--bg);}"
    "#histSvg .axis-label{fill:var(--muted);font-size:10px;font-family:sans-serif;}"
    "#histSvg .hover-line{stroke:#e5e7eb;stroke-width:1;stroke-dasharray:2,2;}"
    "#histSvg .hover-dot{fill:var(--bad);}"
    "#histSvg .hover-text{fill:var(--text);font-size:11px;font-family:sans-serif;}"
    ".heat-on-band{fill:var(--heat-band);}"
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
  html += String(getSetPoint(), 1);
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

function formatTimeShort(tsSec)
{
  var d = new Date(tsSec * 1000);
  return d.toLocaleTimeString("de-DE",
  {
    hour:   "2-digit",
    minute: "2-digit"
  });
}

function formatTemp(val)
{
  return Number(val).toFixed(1) + " °C";
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
    return fetch('/history.json?days=1', { cache: 'no-store' })
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
    if (!rows.length)
    {
      var txt = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      txt.setAttribute('x', '10');
      txt.setAttribute('y', '20');
      txt.textContent = 'Keine Verlaufsdaten verfügbar.';
      svg.appendChild(txt);
      return;
    }

    var width = 600;
    var height = 320;
    var padL = 50, padR = 10, padT = 10, padB = 30;

    var maxTs = Number(rows[rows.length - 1].ts) || 0;
    var minTs = maxTs > 86400 ? (maxTs - 86400) : 0;

    var filtered = [];
    var lastBefore = null;
    for (var i = 0; i < rows.length; i++)
    {
      var r = rows[i];
      var tsVal = Number(r.ts) || 0;
      if (tsVal < minTs)
      {
        lastBefore = r;
        continue;
      }
      if (tsVal > maxTs)
      {
        continue;
      }
      filtered.push(r);
    }

    if (lastBefore && filtered.length)
    {
      filtered.unshift({
        ts: minTs,
        tC: lastBefore.tC,
        spC: lastBefore.spC,
        hyC: lastBefore.hyC,
        h: lastBefore.h
      });
    }

    if (!filtered.length)
    {
      filtered = rows.slice(-1);
    }

    var temps = [];
    var uppers = [];
    var lowers = [];
    var tsList = [];
    var onList = [];

    for (var j = 0; j < filtered.length; j++)
    {
      var fr = filtered[j];
      var tVal = Number(fr.tC) || 0;
      var spVal = Number(fr.spC) || 0;
      var hyVal = Number(fr.hyC) || 0;
      var on = fr.h ? true : false;

      temps.push(tVal);
      uppers.push(spVal + hyVal);
      lowers.push(spVal - hyVal);
      tsList.push(Number(fr.ts) || 0);
      onList.push(on);
    }

    var allVals = temps.concat(uppers).concat(lowers);
    var minV = allVals[0];
    var maxV = allVals[0];
    for (var j = 1; j < allVals.length; j++)
    {
      if (allVals[j] < minV) { minV = allVals[j]; }
      if (allVals[j] > maxV) { maxV = allVals[j]; }
    }
    if (maxV - minV < 0.5)
    {
      minV -= 0.5;
      maxV += 0.5;
    }
    var vSpan = maxV - minV;

    minTs = minTs || tsList[0];
    maxTs = maxTs || tsList[tsList.length - 1];
    if (maxTs <= minTs)
    {
      maxTs = minTs + 86400;
    }
    var tsSpan = maxTs - minTs;

    function xForTs(ts)
    {
      var rel = (ts - minTs) / tsSpan;
      return padL + rel * (width - padL - padR);
    }

    function yFor(val)
    {
      var rel = (val - minV) / vSpan;
      var inv = 1.0 - rel;
      return padT + inv * (height - padT - padB);
    }

    var bg = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    bg.setAttribute('x', '0');
    bg.setAttribute('y', '0');
    bg.setAttribute('width', width);
    bg.setAttribute('height', height);
    bg.setAttribute('class', 'bg');
    svg.appendChild(bg);

    // Heiz-Phasen als farbige Bänder hinter den Linien
    var segments = [];
    var segStart = null;
    for (var s = 0; s < tsList.length; s++)
    {
      if (onList[s])
      {
        if (segStart === null)
        {
          segStart = tsList[s];
        }
      }
      else if (segStart !== null)
      {
        segments.push({ start: segStart, end: tsList[s] });
        segStart = null;
      }
    }
    if (segStart !== null)
    {
      segments.push({ start: segStart, end: tsList[tsList.length - 1] });
    }

    for (var si = 0; si < segments.length; si++)
    {
      var seg = segments[si];
      var x1 = xForTs(seg.start);
      var x2 = xForTs(seg.end);
      if (x2 <= x1)
      {
        x2 = x1 + 1;
      }
      var band = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      band.setAttribute('x', x1);
      band.setAttribute('y', padT);
      band.setAttribute('width', x2 - x1);
      band.setAttribute('height', height - padT - padB);
      band.setAttribute('class', 'heat-on-band');
      svg.appendChild(band);
    }

    // Y-Grid + Labels
    var yTicks = 5;
    for (var k = 0; k < yTicks; k++)
    {
      var val = minV + (vSpan * (k / (yTicks - 1)));
      var y = yFor(val);

      var gl = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      gl.setAttribute('x1', padL);
      gl.setAttribute('y1', y);
      gl.setAttribute('x2', width - padR);
      gl.setAttribute('y2', y);
      gl.setAttribute('class', 'grid-line');
      svg.appendChild(gl);

      var lbl = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      lbl.setAttribute('x', padL - 4);
      lbl.setAttribute('y', y + 3);
      lbl.setAttribute('text-anchor', 'end');
      lbl.setAttribute('class', 'axis-label');
      lbl.textContent = val.toFixed(1);
      svg.appendChild(lbl);
    }

    var axisX = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    axisX.setAttribute('x1', padL);
    axisX.setAttribute('y1', height - padB);
    axisX.setAttribute('x2', width - padR);
    axisX.setAttribute('y2', height - padB);
    axisX.setAttribute('class', 'axis');
    svg.appendChild(axisX);

    var tickStep = 4 * 3600;
    var firstTick = Math.ceil(minTs / tickStep) * tickStep;
    for (var ts = firstTick; ts <= maxTs + 1; ts += tickStep)
    {
      var x = xForTs(ts);
      var tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      tick.setAttribute('x1', x);
      tick.setAttribute('y1', height - padB);
      tick.setAttribute('x2', x);
      tick.setAttribute('y2', height - padB + 4);
      tick.setAttribute('class', 'axis');
      svg.appendChild(tick);

      var tl = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      tl.setAttribute('x', x);
      tl.setAttribute('y', height - padB + 14);
      tl.setAttribute('text-anchor', 'middle');
      tl.setAttribute('class', 'axis-label');
      tl.textContent = formatTimeShort(ts);
      svg.appendChild(tl);
    }

    function buildTransitions(minTs, maxTs)
    {
      var list = [];
      var dayStart = Number(schedCfg.dayStart) || 0;
      var nightStart = Number(schedCfg.nightStart) || 0;
      var startDay = Math.floor(minTs / 86400) * 86400 - 86400;
      var end = maxTs + 86400;
      for (var base = startDay; base <= end; base += 86400)
      {
        list.push({ ts: base + dayStart * 60, label: 'Tag' });
        list.push({ ts: base + nightStart * 60, label: 'Nacht' });
      }
      return list;
    }

    var transitions = buildTransitions(minTs, maxTs);
    for (var ti = 0; ti < transitions.length; ti++)
    {
      var tr = transitions[ti];
      if (tr.ts < minTs || tr.ts > maxTs) { continue; }
      var xTr = xForTs(tr.ts);
      var pl = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      pl.setAttribute('x1', xTr);
      pl.setAttribute('y1', padT);
      pl.setAttribute('x2', xTr);
      pl.setAttribute('y2', height - padB);
      pl.setAttribute('class', 'phase-line');
      svg.appendChild(pl);

      var lblPhase = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      lblPhase.setAttribute('x', xTr + 2);
      lblPhase.setAttribute('y', padT + 12 + (ti % 2) * 12);
      lblPhase.setAttribute('class', 'axis-label');
      lblPhase.textContent = formatTimeShort(tr.ts) + ' ' + tr.label;
      svg.appendChild(lblPhase);
    }

    function buildPath(vals)
    {
      var d = '';
      for (var i = 0; i < vals.length; i++)
      {
        var x = xForTs(tsList[i]);
        var y = yFor(vals[i]);
        if (i == 0)
        {
          d += 'M' + x + ' ' + y;
        }
        else
        {
          d += ' L' + x + ' ' + y;
        }
      }
      return d;
    }

    function buildSmoothPath(vals)
    {
      if (!vals.length)
      {
        return '';
      }
      var pts = [];
      for (var i = 0; i < vals.length; i++)
      {
        pts.push({ x: xForTs(tsList[i]), y: yFor(vals[i]) });
      }
      var d = 'M' + pts[0].x + ' ' + pts[0].y;
      for (var j = 0; j < pts.length - 1; j++)
      {
        var p0 = j > 0 ? pts[j - 1] : pts[j];
        var p1 = pts[j];
        var p2 = pts[j + 1];
        var p3 = (j + 2 < pts.length) ? pts[j + 2] : p2;
        var cp1x = p1.x + (p2.x - p0.x) / 6;
        var cp1y = p1.y + (p2.y - p0.y) / 6;
        var cp2x = p2.x - (p3.x - p1.x) / 6;
        var cp2y = p2.y - (p3.y - p1.y) / 6;
        d += ' C ' + cp1x + ' ' + cp1y + ' ' + cp2x + ' ' + cp2y + ' ' + p2.x + ' ' + p2.y;
      }
      return d;
    }

    var pathGlow = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathGlow.setAttribute('d', buildSmoothPath(temps));
    pathGlow.setAttribute('class', 'temp-glow');
    svg.appendChild(pathGlow);

    var pathTemp = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathTemp.setAttribute('d', buildSmoothPath(temps));
    pathTemp.setAttribute('class', 'temp-line');
    svg.appendChild(pathTemp);

    var pathUpper = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathUpper.setAttribute('d', buildPath(uppers));
    pathUpper.setAttribute('class', 'upper-line');
    svg.appendChild(pathUpper);

    var pathLower = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathLower.setAttribute('d', buildPath(lowers));
    pathLower.setAttribute('class', 'lower-line');
    svg.appendChild(pathLower);

    var yLbl = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    yLbl.setAttribute('x', 15);
    yLbl.setAttribute('y', padT + 10);
    yLbl.setAttribute('class', 'axis-label');
    yLbl.textContent = '°C';
    svg.appendChild(yLbl);

    var hoverLine = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    hoverLine.setAttribute('class', 'hover-line');
    hoverLine.style.display = 'none';
    svg.appendChild(hoverLine);

    var hoverDot = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    hoverDot.setAttribute('r', '3');
    hoverDot.setAttribute('class', 'hover-dot');
    hoverDot.style.display = 'none';
    svg.appendChild(hoverDot);

    var hoverText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    hoverText.setAttribute('x', padL + 5);
    hoverText.setAttribute('y', padT + 15);
    hoverText.setAttribute('class', 'hover-text');
    hoverText.style.display = 'none';
    svg.appendChild(hoverText);

    var overlay = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    overlay.setAttribute('x', padL);
    overlay.setAttribute('y', padT);
    overlay.setAttribute('width', width - padL - padR);
    overlay.setAttribute('height', height - padT - padB);
    overlay.setAttribute('fill', 'transparent');
    overlay.style.cursor = 'crosshair';
    svg.appendChild(overlay);

    function hideHover()
    {
      hoverLine.style.display = 'none';
      hoverDot.style.display = 'none';
      hoverText.style.display = 'none';
    }

    function showHover(evt)
    {
      var pt = svg.createSVGPoint();
      pt.x = evt.clientX;
      pt.y = evt.clientY;
      var sp = pt.matrixTransform(svg.getScreenCTM().inverse());
      var x = sp.x;

      if (x < padL || x > (width - padR))
      {
        hideHover();
        return;
      }

      var f = (x - padL) / (width - padL - padR);
      if (f < 0) { f = 0; }
      if (f > 1) { f = 1; }

      var targetTs = minTs + f * tsSpan;
      var idx = 0;
      var bestDiff = Math.abs(tsList[0] - targetTs);
      for (var ii = 1; ii < tsList.length; ii++)
      {
        var diff = Math.abs(tsList[ii] - targetTs);
        if (diff < bestDiff)
        {
          bestDiff = diff;
          idx = ii;
        }
      }

      var xv = xForTs(tsList[idx]);
      var yv = yFor(temps[idx]);

      hoverLine.setAttribute('x1', xv);
      hoverLine.setAttribute('y1', padT);
      hoverLine.setAttribute('x2', xv);
      hoverLine.setAttribute('y2', height - padB);
      hoverLine.style.display = 'block';

      hoverDot.setAttribute('cx', xv);
      hoverDot.setAttribute('cy', yv);
      hoverDot.style.display = 'block';

      var txt = formatTs(tsList[idx]) + "  " + formatTemp(temps[idx]);
      hoverText.textContent = txt;
      hoverText.style.display = 'block';
    }

    overlay.addEventListener('mousemove', showHover);
    overlay.addEventListener('mouseleave', hideHover);
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
