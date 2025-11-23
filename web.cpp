#include "web.h"
#include <ESP8266WebServer.h>
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

/***************** handleHistoryJson ********************************************
 * params: none
 * return: void
 * Description:
 * Returns history data in compact JSON for chart/table rendering.
 ******************************************************************************/
static void handleHistoryJson();

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
  html.reserve(8192);

  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F("</title>");
  html += F("<style>"
            "body{font-family:sans-serif;margin:1rem}"
            ".grid{display:grid;grid-template-columns:12rem 1fr auto auto;gap:.5rem;align-items:center}"
            ".btn{padding:.4rem .8rem;border:1px solid #ccc;border-radius:.5rem;text-decoration:none}"
            "input{padding:.4rem}"
            ".select-lg{font-size:1.1rem;padding:.55rem .9rem;min-width:12rem;height:2.3rem}"
            ".chart-card{margin-top:1.5rem}"
            "#histWrap{position:relative;max-width:100%;height:320px}"
            "#histWrap svg{width:100%;height:100%}"
            ".badge{background:#1a73e8;color:#fff;border-radius:.4rem;padding:.15rem .45rem;font-size:.85rem;margin-left:.5rem}"
            ".hist-head{display:flex;justify-content:space-between;align-items:center;gap:.5rem;flex-wrap:wrap}"
            ".view-toggle{display:flex;gap:.5rem}"
            ".tab-btn{padding:.4rem .8rem;border:1px solid #ccc;border-radius:.5rem;background:#f6f6f6;cursor:pointer}"
            ".tab-btn.active{background:#1a73e8;color:#fff;border-color:#1a73e8}"
            ".hist-table{width:100%;border-collapse:collapse;margin-top:.5rem}"
            ".hist-table th,.hist-table td{border:1px solid #ddd;padding:.4rem;text-align:left;font-size:.95rem}"
            ".hist-table th{background:#f6f6f6}"
            "#histSvg .temp-line{stroke:#d93025;fill:none;stroke-width:1.5}"
            "#histSvg .upper-line{stroke:#0b8043;fill:none;stroke-width:1;stroke-dasharray:4,4}"
            "#histSvg .lower-line{stroke:#1a73e8;fill:none;stroke-width:1;stroke-dasharray:4,4}"
            "#histSvg .axis{stroke:#999;stroke-width:1;fill:none}"
            "#histSvg .grid-line{stroke:#e0e0e0;stroke-width:1;fill:none}"
            "#histSvg .bg{fill:#fafafa}"
            "#histSvg .axis-label{fill:#333;font-size:10px;font-family:sans-serif}"
            "#histSvg .hover-line{stroke:#555;stroke-width:1;stroke-dasharray:2,2}"
            "#histSvg .hover-dot{fill:#d93025}"
            "#histSvg .hover-text{fill:#000;font-size:11px;font-family:sans-serif}"
            "</style>");
  html += F("</head><body>");

  // Header
  html += F("<h2>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F(" <span class='badge'>v");
  html += APP_VERSION;
  html += F("</span></h2>");
  html += F("<div>Host: ");
  html += getHostLabel();
  html += F(".local</div>");

  // Statuszeile
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
  html += F("<option value='0'");
  if (m == MODE_AUTO)
  {
    html += F(" selected");
  }
  html += F(">AUTO</option>");
  html += F("<option value='1'");
  if (m == MODE_OFF)
  {
    html += F(" selected");
  }
  html += F(">OFF</option>");
  html += F("<option value='2'");
  if (m == MODE_BOOST)
  {
    html += F(" selected");
  }
  html += F(">BOOST</option>");
  html += F("</select><span></span><span></span></div>");

  html += F("<div class='grid'><span></span>"
            "<button class='btn' type='submit'>Save</button>"
            "<span></span><span></span></div>");
  html += F("</form>");

  // Boost control
  html += F("<h3>Boost</h3>");
  if (getControlMode() == MODE_BOOST)
  {
    unsigned long remaining = 0;
    unsigned long end = getBoostEndTime();
    if (end > millis())
    {
      remaining = (end - millis()) / 60000UL;
    }
    html += F("<p>Boost active ~ ");
    html += String((int)remaining);
    html += F(" min remaining</p>");
  }
  html += F("<form method='POST' action='/boost'>");
  html += F("<div class='grid'><label>Start Boost</label>"
            "<button class='btn' type='submit'>Start</button>"
            "<span></span><span></span></div>");
  html += F("</form>");

  // History-Card (24h)
  html += F("<div class='chart-card'>"
            "<div class='hist-head'>"
            "<h3>History (letzte 24 Stunden)</h3>"
            "<div class='view-toggle'>"
            "<button type='button' class='tab-btn active' data-view='chart'>Diagramm</button>"
            "<button type='button' class='tab-btn' data-view='table'>Tabelle</button>"
            "</div></div>"
            "<div id='histTableWrap' class='hist-view' style='display:none'></div>"
            "<div id='histChartWrap' class='hist-view'>"
            "<div id='histWrap'>"
            "<svg id='histSvg' viewBox='0 0 600 320' preserveAspectRatio='none'></svg>"
            "</div>"
            "</div></div>");

  // JS als Raw-String, damit kein Quote-Chaos
  html += F("<script>");
  html += R"JS(
(function(){
  var svg = document.getElementById('histSvg');
  var tableWrap = document.getElementById('histTableWrap');
  var chartWrap = document.getElementById('histChartWrap');
  var viewBtns = document.querySelectorAll('[data-view]');
  if (!svg || !tableWrap || !chartWrap) { return; }

  function setActive(view)
  {
    for (var i = 0; i < viewBtns.length; i++)
    {
      var btn = viewBtns[i];
      var isActive = (btn.getAttribute('data-view') === view);
      if (isActive)
      {
        btn.classList.add('active');
      }
      else
      {
        btn.classList.remove('active');
      }
    }
    chartWrap.style.display = (view === 'chart' ? 'block' : 'none');
    tableWrap.style.display = (view === 'table' ? 'block' : 'none');
  }

  var activeBtn = document.querySelector('.tab-btn.active');
  var initialView = activeBtn ? activeBtn.getAttribute('data-view') : 'chart';
  setActive(initialView);
  for (var i = 0; i < viewBtns.length; i++)
  {
    viewBtns[i].addEventListener('click', function()
    {
      var v = this.getAttribute('data-view');
      setActive(v);
    });
  }

  function formatTs(ts)
  {
    var d = new Date(ts * 1000);
    var day = d.getDate();
    var mon = d.getMonth() + 1;
    var h = d.getHours();
    var m = d.getMinutes();
    function pad(v)
    {
      v = String(v);
      return v.length < 2 ? '0' + v : v;
    }
    return pad(day) + '.' + pad(mon) + ' ' + pad(h) + ':' + pad(m);
  }

  function formatTimeShort(ts)
  {
    var d = new Date(ts * 1000);
    var h = d.getHours();
    var m = d.getMinutes();
    function pad(v)
    {
      v = String(v);
      return v.length < 2 ? '0' + v : v;
    }
    return pad(h) + ':' + pad(m);
  }

  function formatTemp(v)
  {
    return (typeof v === 'number' && isFinite(v)) ? v.toFixed(1) + ' °C' : '–';
  }

  function clearSvg()
  {
    while (svg.firstChild)
    {
      svg.removeChild(svg.firstChild);
    }
  }

  function drawChart(rows)
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

    var temps = [];
    var uppers = [];
    var lowers = [];
    var tsList = [];

    for (var i = 0; i < rows.length; i++)
    {
      var r = rows[i];
      var tVal = Number(r.t) || 0;
      var spVal = Number(r.sp) || 0;
      var hyVal = Number(r.hy) || 0;

      temps.push(tVal / 100.0);
      uppers.push((spVal + hyVal) / 100.0);
      lowers.push((spVal - hyVal) / 100.0);
      tsList.push(Number(r.ts) || 0);
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

    function xFor(idx)
    {
      if (rows.length === 1)
      {
        return padL + (width - padL - padR) / 2;
      }
      var f = idx / (rows.length - 1);
      return padL + f * (width - padL - padR);
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

    // X-Achse
    var axisX = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    axisX.setAttribute('x1', padL);
    axisX.setAttribute('y1', height - padB);
    axisX.setAttribute('x2', width - padR);
    axisX.setAttribute('y2', height - padB);
    axisX.setAttribute('class', 'axis');
    svg.appendChild(axisX);

    // X-Ticks max 6
    var maxXTicks = 6;
    var stepIdx = 1;
    if (rows.length > 1)
    {
      stepIdx = Math.floor((rows.length - 1) / (maxXTicks - 1));
      if (stepIdx < 1) { stepIdx = 1; }
    }
    var used = 0;
    for (var idx = 0; idx < rows.length && used < maxXTicks; idx += stepIdx)
    {
      var x = xFor(idx);
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
      tl.textContent = formatTimeShort(tsList[idx]);
      svg.appendChild(tl);
      used++;
    }

    function buildPath(vals)
    {
      var d = '';
      for (var i = 0; i < vals.length; i++)
      {
        var x = xFor(i);
        var y = yFor(vals[i]);
        if (i === 0)
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

    var pathTemp = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    pathTemp.setAttribute('d', buildPath(temps));
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

    // Y-Achsentitel
    var yLbl = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    yLbl.setAttribute('x', 15);
    yLbl.setAttribute('y', padT + 10);
    yLbl.setAttribute('class', 'axis-label');
    yLbl.textContent = '°C';
    svg.appendChild(yLbl);

    // Hover-Elemente
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

      var idx = Math.round(f * (rows.length - 1));
      if (idx < 0) { idx = 0; }
      if (idx >= rows.length) { idx = rows.length - 1; }

      var xv = xFor(idx);
      var yv = yFor(temps[idx]);

      hoverLine.setAttribute('x1', xv);
      hoverLine.setAttribute('y1', padT);
      hoverLine.setAttribute('x2', xv);
      hoverLine.setAttribute('y2', height - padB);
      hoverLine.style.display = 'block';

      hoverDot.setAttribute('cx', xv);
      hoverDot.setAttribute('cy', yv);
      hoverDot.style.display = 'block';

      var txt = formatTs(tsList[idx]) + '  ' + formatTemp(temps[idx]);
      hoverText.textContent = txt;
      hoverText.style.display = 'block';
    }

    overlay.addEventListener('mousemove', showHover);
    overlay.addEventListener('mouseleave', hideHover);
  }

  function renderTableFromEvents(events)
  {
    if (!events.length)
    {
      tableWrap.innerHTML = '<p>Keine Heizphasen gefunden.</p>';
      return;
    }
    var html = '<table class=\"hist-table\"><thead><tr>'
             + '<th>An (Zeit)</th><th>Aus (Zeit)</th><th>Temp bei An</th><th>Temp bei Aus</th><th>Ein/Aus-Schwellen</th>'
             + '</tr></thead><tbody>';
    for (var j = 0; j < events.length; j++)
    {
      var ev = events[j];
      var endLabel = ev.end ? formatTs(ev.end) : 'läuft';
      var endTempLabel = (typeof ev.endTemp === 'number') ? formatTemp(ev.endTemp) : '–';
      html += '<tr>'
           +  '<td>' + formatTs(ev.start) + '</td>'
           +  '<td>' + endLabel + '</td>'
           +  '<td>' + formatTemp(ev.startTemp) + '</td>'
           +  '<td>' + endTempLabel + '</td>'
           +  '<td>' + formatTemp(ev.lower) + ' / ' + formatTemp(ev.upper) + '</td>'
           +  '</tr>';
    }
    html += '</tbody></table>';
    tableWrap.innerHTML = html;
  }

    function tryParseHistory(text)
  {
    // 1. Normaler JSON-Parse-Versuch
    try
    {
      return JSON.parse(text);
    }
    catch (e)
    {
      // 2. Reparatur-Versuch für abgeschnittene Arrays
      //    Beispiel: [{"ts":...,"h":0},{"ts":...,"h":0
      var start = text.indexOf('[');
      var endObj = text.lastIndexOf('}');

      if (start === -1 || endObj === -1 || endObj <= start)
      {
        return null;
      }

      // Nur den Teil von der ersten '[' bis zur letzten '}' nehmen
      var trimmed = text.slice(start, endObj + 1);

      // Wenn kein '[' am Anfang -> einklammern
      if (trimmed.charAt(0) !== '[')
      {
        trimmed = '[' + trimmed + ']';
      }
      else
      {
        // Array war nur hinten offen -> einfach ']' anhängen
        trimmed = trimmed + ']';
      }

      try
      {
        return JSON.parse(trimmed);
      }
      catch (e2)
      {
        return null;
      }
    }
  }

  fetch('/history.json?days=1')
    .then(function(r)
    {
      // Rohtext holen, nicht direkt r.json()
      return r.text();
    })
    .then(function(text)
    {
      var rows = tryParseHistory(text);

      if (!rows || !rows.length)
      {
        clearSvg();
        var txt = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        txt.setAttribute('x', '10');
        txt.setAttribute('y', '20');
        txt.textContent = 'Keine Verlaufsdaten verfügbar.';
        svg.appendChild(txt);
        tableWrap.innerHTML = '<p>Keine Verlaufsdaten verfügbar.</p>';
        return;
      }

      // Ab hier alles wie vorher: drawChart(rows), Events bauen, Tabelle rendern
      drawChart(rows);

      var events = [];
      var current = null;
      for (var i = 0; i < rows.length; i++)
      {
        var r = rows[i];
        var ts = Number(r.ts) || 0;
        var tVal = Number(r.t) || 0;
        var spVal = Number(r.sp) || 0;
        var hyVal = Number(r.hy) || 0;
        var on = r.h ? true : false;

        if (on && !current)
        {
          current = {
            start: ts,
            startTemp: tVal / 100.0,
            lower: (spVal - hyVal) / 100.0,
            upper: (spVal + hyVal) / 100.0
          };
        }
        if (!on && current)
        {
          current.end = ts;
          current.endTemp = tVal / 100.0;
          events.push(current);
          current = null;
        }
      }
      if (current)
      {
        events.push(current);
      }

      renderTableFromEvents(events);
    })
    .catch(function(err)
    {
      clearSvg();
      var txt = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      txt.setAttribute('x', '10');
      txt.setAttribute('y', '20');
      txt.textContent = 'Fehler beim Laden der Verlaufsdaten.';
      svg.appendChild(txt);
      tableWrap.innerHTML = '<p>Fehler beim Laden der Verlaufsdaten.</p>';
    });
})();
)JS";
  html += F("</script>");

  html += F("</body></html>");

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
  webServer.on("/history.json", HTTP_GET, handleHistoryJson);
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

  const unsigned long intervalMin = (unsigned long)LOG_INTERVAL_MINUTES;
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

  LogSample *buffer = (LogSample *)malloc(sizeof(LogSample) * maxRecords);
  if (!buffer)
  {
    webServer.send(500, "application/json", "[]");
    return;
  }

  size_t count = 0;
  readHistoryTail(maxRecords, buffer, &count);

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
  json.reserve(count * 40 + 16);
  json += '[';

  for (size_t i = 0; i < count; ++i)
  {
    if (i > 0)
    {
      json += ',';
    }

    const LogSample &s = buffer[i];
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
