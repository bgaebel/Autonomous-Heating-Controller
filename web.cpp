#include "web.h"
#include <ESP8266WebServer.h>   // Use WebServer for ESP32
#include "config.h"
#include "control.h"
#include "sensor.h"
#include "mqtt.h"
#include "history.h"
#include <stdlib.h>

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
static void handleHistoryJson();

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
  html += F("<style>body{font-family:sans-serif;margin:1rem} .grid{display:grid;grid-template-columns:12rem 1fr auto auto;gap:.5rem;align-items:center} .btn{padding:.4rem .8rem;border:1px solid #ccc;border-radius:.5rem;text-decoration:none} input{padding:.4rem} .select-lg{font-size:1.1rem;padding:.55rem .9rem;min-width:12rem;height:2.3rem} .chart-card{margin-top:1.5rem} #histWrap{position:relative;max-width:100%;height:320px} .badge{background:#1a73e8;color:#fff;border-radius:.4rem;padding:.15rem .45rem;font-size:.85rem;margin-left:.5rem} .hist-head{display:flex;justify-content:space-between;align-items:center;gap:.5rem;flex-wrap:wrap} .view-toggle{display:flex;gap:.5rem} .tab-btn{padding:.4rem .8rem;border:1px solid #ccc;border-radius:.5rem;background:#f6f6f6;cursor:pointer} .tab-btn.active{background:#1a73e8;color:#fff;border-color:#1a73e8} .hist-table{width:100%;border-collapse:collapse;margin-top:.5rem} .hist-table th,.hist-table td{border:1px solid #ddd;padding:.4rem;text-align:left;font-size:.95rem} .hist-table th{background:#f6f6f6}</style>");
  html += F("</head><body>");

  // Header mit Raum/Topic
  html += F("<h2>Heating Controller &ndash; ");
  html += getBaseTopic();
  html += F(" <span class='badge'>v");
  html += APP_VERSION;
  html += F("</span></h2>");
  html += F("<div>Host: ");
  html += getHostLabel();
  html += F(".local</div>");

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

  html += F("<div class='chart-card'><div class='hist-head'><h3>History (letzte 5 Tage)</h3><div class='view-toggle'><button type='button' class='tab-btn active' data-view='chart'>Diagramm</button><button type='button' class='tab-btn' data-view='table'>Tabelle</button></div></div><div id='histTableWrap' class='hist-view' style='display:none'></div><div id='histChartWrap' class='hist-view'><div id='histWrap'><canvas id='histChart'></canvas></div></div></div>");
  html += F("<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.4/dist/chart.umd.min.js' integrity='sha384-f4NBX7ZD0rGUNShUMpOWcZH/AsLiCFYI8a9kaI0s5momkGLumZ5qX6Ch12HaxJXJ' crossorigin='anonymous'></script>");
  html += F("<script>(function(){const canvas=document.getElementById('histChart');const tableWrap=document.getElementById('histTableWrap');const chartWrap=document.getElementById('histChartWrap');const viewBtns=document.querySelectorAll('[data-view]');if(!canvas||!tableWrap||!chartWrap){return;}const setActive=view=>{viewBtns.forEach(btn=>{const isActive=btn.getAttribute('data-view')===view;btn.classList.toggle('active',isActive);});chartWrap.style.display=view==='chart'?'block':'none';tableWrap.style.display=view==='table'?'block':'none';};const initialView=document.querySelector('.tab-btn.active')?.getAttribute('data-view')||'chart';setActive(initialView);viewBtns.forEach(btn=>btn.addEventListener('click',()=>setActive(btn.getAttribute('data-view'))));const formatTs=ts=>{const d=new Date(ts*1000);const pad=v=>String(v).padStart(2,'0');return pad(d.getDate())+'.'+pad(d.getMonth()+1)+' '+pad(d.getHours())+':'+pad(d.getMinutes());};const formatTemp=v=>Number.isFinite(v)?v.toFixed(1)+' °C':'–';fetch('/history.json?days=5').then(r=>r.json()).then(rows=>{if(!Array.isArray(rows)||!rows.length){const msg=document.createElement('p');msg.textContent='Keine Verlaufsdaten verfügbar.';chartWrap.replaceChildren(msg);tableWrap.replaceChildren(msg.cloneNode(true));return;}const labels=rows.map(r=>formatTs(r.ts));const temps=rows.map(r=>r.t/100);const uppers=rows.map(r=>(r.sp+r.hy)/100);const lowers=rows.map(r=>(r.sp-r.hy)/100);const heaterOnData=rows.map(r=>r.h ? r.t/100 : null);const events=[];let current=null;rows.forEach(r=>{const on=!!r.h;if(on && !current){current={start:r.ts,startTemp:r.t/100,lower:(r.sp-r.hy)/100,upper:(r.sp+r.hy)/100};}if(!on && current){current.end=r.ts;current.endTemp=r.t/100;events.push(current);current=null;}});if(current){events.push(current);}const renderTable=()=>{tableWrap.innerHTML='';if(!events.length){const msg=document.createElement('p');msg.textContent='Keine Heizphasen gefunden.';tableWrap.appendChild(msg);return;}const table=document.createElement('table');table.className='hist-table';const thead=document.createElement('thead');thead.innerHTML='<tr><th>An (Zeit)</th><th>Aus (Zeit)</th><th>Temp bei An</th><th>Temp bei Aus</th><th>Ein/Aus-Schwellen</th></tr>';table.appendChild(thead);const tbody=document.createElement('tbody');events.forEach(ev=>{const tr=document.createElement('tr');const endLabel=ev.end?formatTs(ev.end):'läuft';const endTempLabel=ev.endTemp!==undefined?formatTemp(ev.endTemp):'–';tr.innerHTML=`<td>${formatTs(ev.start)}</td><td>${endLabel}</td><td>${formatTemp(ev.startTemp)}</td><td>${endTempLabel}</td><td>${formatTemp(ev.lower)} / ${formatTemp(ev.upper)}</td>`;tbody.appendChild(tr);});table.appendChild(tbody);tableWrap.appendChild(table);};renderTable();new Chart(canvas.getContext('2d'),{type:'line',data:{labels:labels,datasets:[{label:'Temperatur',data:temps,borderColor:'#d93025',backgroundColor:'rgba(217,48,37,0)',pointRadius:0,tension:0.1,fill:false},{label:'Heizung AN (Fläche)',data:heaterOnData,borderColor:'rgba(0,0,0,0)',backgroundColor:'rgba(217,48,37,0.18)',pointRadius:0,tension:0.1,fill:true},{label:'Ausschalttemperatur',data:uppers,borderColor:'#0b8043',borderDash:[6,6],pointRadius:0,tension:0.1,fill:false},{label:'Einschalttemperatur',data:lowers,borderColor:'#1a73e8',borderDash:[6,6],pointRadius:0,tension:0.1,fill:false}]},options:{maintainAspectRatio:false,interaction:{mode:'nearest',intersect:false},scales:{y:{title:{display:true,text:'°C'}},x:{ticks:{maxRotation:0,autoSkip:true,maxTicksLimit:8}}},plugins:{legend:{position:'bottom'}}});}).catch(()=>{const msg=document.createElement('p');msg.textContent='Fehler beim Laden der Verlaufsdaten.';chartWrap.replaceChildren(msg);tableWrap.replaceChildren(msg.cloneNode(true));});})();</script>");
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

  LogSample* buffer = (LogSample*)malloc(sizeof(LogSample) * maxRecords);
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
