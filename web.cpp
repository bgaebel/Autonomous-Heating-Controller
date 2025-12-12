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

static void handleHeaterOffPost();

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
  const bool heaterIsOn = isHeaterOn();

  String html;
  html.reserve(8192);

  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F("</title>");
  html += F("<style>"
            ":root{--bg:#f7f8fa;--card:#ffffff;--text:#0f172a;--muted:#6b7280;--accent:#2563eb;"
            "--accent-soft:rgba(37,99,235,.08);--success:#16a34a;--danger:#ef4444;--border:#e5e7eb;"
            "--temp-line:#fb923c;--upper-line:#22c55e;--lower-line:#60a5fa;--phase-line:#a855f7;"
            "--heat-band:rgba(239,68,68,0.16);--shadow-soft:0 4px 12px rgba(15,23,42,.18);}"
            "[data-theme='dark']{--bg:#020617;--card:#020617;--text:#e5e7eb;--muted:#94a3b8;"
            "--accent:#60a5fa;--accent-soft:rgba(96,165,250,.1);--success:#22c55e;--danger:#f97373;"
            "--border:#1f2937;--temp-line:#fdba74;--upper-line:#4ade80;--lower-line:#93c5fd;"
            "--phase-line:#c4b5fd;--heat-band:rgba(248,113,113,0.18);--shadow-soft:0 8px 24px rgba(0,0,0,.65);}"
            "body{font-family:system-ui,sans-serif;margin:1rem;background:var(--bg);color:var(--text);"
            "transition:background .25s ease,color .25s ease;}"
            "h2{margin-bottom:.2rem;}"
            ".card{background:var(--card);border:1px solid var(--border);border-radius:.9rem;padding:1rem;"
            "box-shadow:var(--shadow-soft);margin-bottom:1rem;}"
            ".grid{display:grid;grid-template-columns:12rem 1fr auto auto;gap:.5rem;align-items:center;}"
            ".btn{padding:.45rem .9rem;border:1px solid var(--border);border-radius:.6rem;text-decoration:none;"
            "background:var(--card);color:var(--text);cursor:pointer;display:inline-flex;align-items:center;"
            "gap:.25rem;transition:background .16s ease,box-shadow .16s ease,border-color .16s ease;}"
            ".btn:hover{background:var(--accent);color:#fff;box-shadow:0 6px 16px rgba(0,0,0,.35);"
            "border-color:var(--accent);}"
            "input,select,button{font-family:inherit;background:var(--card);color:var(--text);}"
            "input{padding:.45rem;border:1px solid var(--border);border-radius:.5rem;}"
            "input:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 1px var(--accent-soft);}"
            "input[type='time']{padding:.45rem .6rem;min-height:2.4rem;font-size:1rem;}"
            ".select-lg{font-size:1.05rem;padding:.5rem .8rem;min-width:12rem;height:2.2rem;"
            "border:1px solid var(--border);border-radius:.6rem;}"
            ".badge{background:var(--accent);color:#fff;border-radius:.5rem;padding:.2rem .5rem;font-size:.8rem;"
            "margin-left:.4rem;}"
            ".split{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:1rem;}"
            ".label-small{color:var(--muted);font-size:.9rem;}"
            ".hist-table{width:100%;border-collapse:collapse;margin-top:.5rem;}"
            ".hist-table th,.hist-table td{border:1px solid var(--border);padding:.4rem;text-align:left;font-size:.95rem;}"
            ".hist-table th{background:var(--accent);color:#fff;}"
            ".hist-head{display:flex;justify-content:space-between;align-items:center;gap:.5rem;flex-wrap:wrap;}"
            ".view-toggle{display:flex;gap:.5rem;}"
            ".tab-btn{padding:.4rem .8rem;border-radius:.6rem;border:1px solid var(--border);background:var(--card);"
            "cursor:pointer;transition:background .16s ease,color .16s ease,border-color .16s ease;}"
            ".tab-btn.active{background:var(--accent);color:#fff;border-color:var(--accent);}"
            ".chart-card{margin-top:1.5rem;}"
            "#histWrap{position:relative;max-width:100%;height:320px;}"
            "#histWrap svg{width:100%;height:100%;}"
            "#histSvg .temp-line{stroke:var(--temp-line);fill:none;stroke-width:1.6;}"
            "#histSvg .upper-line{stroke:var(--upper-line);fill:none;stroke-width:1;stroke-dasharray:4,4;}"
            "#histSvg .lower-line{stroke:var(--lower-line);fill:none;stroke-width:1;stroke-dasharray:4,4;}"
            "#histSvg .phase-line{stroke:var(--phase-line);fill:none;stroke-width:1;stroke-dasharray:3,2;opacity:.9;}"
            "#histSvg .axis{stroke:#6b7280;stroke-width:1;fill:none;}"
            "#histSvg .grid-line{stroke:rgba(148,163,184,.45);stroke-width:1;fill:none;}"
            "#histSvg .bg{fill:var(--bg);}"
            "#histSvg .axis-label{fill:var(--muted);font-size:10px;font-family:sans-serif;}"
            "#histSvg .hover-line{stroke:#e5e7eb;stroke-width:1;stroke-dasharray:2,2;}"
            "#histSvg .hover-dot{fill:var(--danger);}"
            "#histSvg .hover-text{fill:var(--text);font-size:11px;font-family:sans-serif;}"
            ".heat-on-band{fill:var(--heat-band);}"
            ".status-bar{display:flex;gap:.75rem;align-items:center;flex-wrap:wrap;font-size:.95rem;}"
            ".status-item{display:flex;align-items:center;gap:.35rem;padding:.35rem .7rem;border-radius:.6rem;"
            "background:var(--accent-soft);border:1px solid var(--border);}"
            ".status-ok{background:rgba(34,197,94,.15);color:var(--success);border-color:rgba(34,197,94,.4);}"
            ".status-bad{background:rgba(239,68,68,.15);color:var(--danger);border-color:rgba(239,68,68,.4);}"
            "@media(max-width:680px){"
              "body{margin:.6rem;}"
              ".grid{grid-template-columns:1fr 1fr;}"
              ".grid label{font-size:.9rem;}"
              ".btn{justify-content:center;}"
            "}"
            "</style>");
  html += F("</head><body>");

  // Header
  html += F("<h2>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F(" <span class='badge'>v");
  html += APP_VERSION;
  html += F("</span></h2>");
  html += F("<div style='display:flex;gap:.5rem;align-items:center;flex-wrap:wrap;margin:.4rem 0 1rem 0;'>Host: ");
  html += getHostLabel();
  html += F(".local");
  html += F("<button class='btn' type='button' onclick='toggleTheme()'>🌙 Dark / Light</button>");
  html += F("</div>");

  // Statuszeile (modern, ohne Temperatur)
  html += F("<div class='card'>");
  html += F("<div class='status-bar'>");

  // Modus
  html += F("<div class='status-item'>⚙️ ");
  html += modeToStr(getControlMode());
  html += F("</div>");

  // Heater
  html += F("<div class='status-item ");
  html += (heaterIsOn ? "status-ok" : "status-bad");
  html += F("'>🔥 ");
  html += (heaterIsOn ? "Heater ON" : "Heater OFF");
  html += F("</div>");

  if (heaterIsOn)
  {
    html += F(
      "<form method='POST' action='/heaterOff' style='margin:0;'>"
      "<button class='btn' type='submit' "
      "onclick='return confirm(\"Heizung wirklich ausschalten?\")'>"
      "🛑 Heater OFF</button></form>"
    );
  }

  // MQTT
  html += F("<div class='status-item ");
  html += (mqttIsConnected() ? "status-ok" : "status-bad");
  html += F("'>📡 MQTT ");
  if (mqttIsConnected())
  {
    html += F("OK");
  }
  else
  {
    html += F("DOWN");
  }
  html += F("</div>");

  html += F("</div></div>");

  auto fmtTime = [](int minutes)
  {
    int h = minutes / 60;
    int m = minutes % 60;
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return String(buf);
  };

  html += F("<div class='card'>");
  html += F("<p><div class='label-small'>Aktuelle Temperatur</div>");
  html += F("<div class='label-lg'>");
  html += (isnan(t) ? String("NaN") : String(t, 1));
  html += F(" &deg;C</div></p>");

  html += F("<p><div class='label-small'>Heizung Einschalttemperatur</div>");
  html += F("<div class='label-lg'>");
  html += String(getSetPoint(), 1);
  html += F(" °C <span class='badge'>");
  html += (isDayScheduleActive() ? "Tag" : "Nacht");
  html += F("</span></div></p></div>");

  html += F("<div class='split'>");

  // Tagesbereich
  html += F("<div class='card'><h3>Tag</h3>");
  html += F("<div class='grid'><label>Soll (°C)</label>"
            "<input name='daySetPoint' type='number' step='0.1' min='5' max='35' value='");
  html += String(getDaySetPoint(), 1);
  html += F("'>"
            "<a class='btn' href='/nudge?field=daySetPoint&delta=-0.5'>-</a>"
            "<a class='btn' href='/nudge?field=daySetPoint&delta=0.5'>+</a></div>");
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
            "<a class='btn' href='/nudge?field=nightSetPoint&delta=-0.5'>-</a>"
            "<a class='btn' href='/nudge?field=nightSetPoint&delta=0.5'>+</a></div>");
  html += F("<div class='grid'><label>Beginn</label>"
            "<input name='nightStart' type='time' step='300' value='");
  html += fmtTime(getNightStartMinutes());
  html += F("'>"
            "<span></span><span></span></div>");
  html += F("</div>");

  html += F("</div>");  // close split-div

  // Hysteresis
  html += F("<div class='card'>");
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

  // Boost control
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
  html += F("<div class='grid'><label>Boost</label>"
            "<button class='btn' type='submit'>Start</button>"
            "<span></span><span></span></div>");
  html += F("</form>");
  html += F("</div>");  // close card

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
    var onList = [];

    for (var i = 0; i < rows.length; i++)
    {
      var r = rows[i];
      var tVal = Number(r.t) || 0;
      var spVal = Number(r.sp) || 0;
      var hyVal = Number(r.hy) || 0;
      var on = r.h ? true : false;

      temps.push(tVal / 100.0);
      uppers.push((spVal + hyVal) / 100.0);
      lowers.push((spVal - hyVal) / 100.0);
      tsList.push(Number(r.ts) || 0);
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

    var minTs = tsList[0];
    var maxTs = tsList[tsList.length - 1];
    if (maxTs === minTs)
    {
      maxTs = minTs + 60;
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

    var maxXTicks = 6;
    for (var t = 0; t < maxXTicks; t++)
    {
      var rel = (maxXTicks === 1) ? 0 : (t / (maxXTicks - 1));
      var ts = minTs + rel * tsSpan;
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
    try
    {
      return JSON.parse(text);
    }
    catch (e)
    {
      var start = text.indexOf('[');
      var endObj = text.lastIndexOf('}');

      if (start === -1 || endObj === -1 || endObj <= start)
      {
        return null;
      }

      var trimmed = text.slice(start, endObj + 1);

      if (trimmed.charAt(0) !== '[')
      {
        trimmed = '[' + trimmed + ']';
      }
      else
      {
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

(function themeInit()
{
  var saved = localStorage.getItem('theme');
  if (saved)
  {
    document.documentElement.setAttribute('data-theme', saved);
  }
})();

function toggleTheme()
{
  var cur = document.documentElement.getAttribute('data-theme');
  var next = (cur === 'dark') ? 'light' : 'dark';
  document.documentElement.setAttribute('data-theme', next);
  localStorage.setItem('theme', next);
}

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
  float daySp   = clampFloat(webServer.arg("daySetPoint").toFloat(), 5.0f, 35.0f);
  float nightSp = clampFloat(webServer.arg("nightSetPoint").toFloat(), 5.0f, 35.0f);
  float hy = clampFloat(webServer.arg("hysteresis").toFloat(), 0.1f, 5.0f);
  int   bm = clampInt(webServer.arg("boostMinutes").toInt(), 0, 240);
  int   md = webServer.arg("mode").toInt();
  int   dayStart = clampInt(parseTimeMinutes(webServer.arg("dayStart")), 0, 1439);
  int   nightStart = clampInt(parseTimeMinutes(webServer.arg("nightStart")), 0, 1439);

  setDaySetPoint(daySp);
  setNightSetPoint(nightSp);
  setDayStartMinutes(dayStart);
  setNightStartMinutes(nightStart);
  setHysteresis(hy);
  setBoostMinutes(bm);
  setControlMode((ControlMode)md);

  renderIndex();
}

/***************** handleHeaterOffPost ******************************************
 * params: none
 * return: void
 * Description:
 * Manual heater OFF: forces relay OFF but does not change the control mode.
 ******************************************************************************/
static void handleHeaterOffPost()
{
  requestHeaterOffNow();
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
  webServer.on("/heaterOff", HTTP_POST, handleHeaterOffPost);
  
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
