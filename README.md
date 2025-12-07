# Autonomous Heating Controller (ESP8266 / Wemos D1 mini)

A standalone heating controller with hysteresis control, web UI, MQTT integration,
OTA updates, mDNS hostname, NTP time, and a LittleFS-backed history ring buffer.

> **TL;DR**  
> - Hardware: Wemos D1 mini (ESP8266), GY-21 (SHT21/Si7021), Relay on D6 (**HIGH = ON**)  
> - Modes: `AUTO`, `OFF`, `BOOST`  
> - Web UI: config + live status + **24h SVG history chart** + **heating-phase table**  
> - MQTT: telemetry, state, commands  
> - OTA + mDNS (`http://<host>.local`)  
> - NTP epoch time, fallback to uptime  
> - History: compact 8-byte samples, ring buffer in LittleFS  

---

## Hardware / Pinout

- **Board:** Wemos D1 mini (ESP8266)
- **Sensor:** GY-21 (SHT21/Si7021) via I²C → `D1=SCL`, `D2=SDA`, `3V3`, `GND`  
- **Relay (heater):** `D6 / GPIO12` (**HIGH = ON**)  
- **Onboard LED:** active-low  
- GY‑21 boards usually include I²C pull-ups.

---

## Build & Dependencies

- **ESP8266 Arduino Core ≥ 3.1.x**
- **Libraries**  
  - Core: `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`, `LittleFS`, `time.h`, `ArduinoOTA`  
  - External: `ArduinoJson`, `PubSubClient`  
- **Flash layout:** Recommended: **4M (FS:2M, OTA:~1M)**  

---

## Configuration (compile-time)

```cpp
#define BASE_TOPIC "Wohnzimmer"
#define HOST_LABEL "wohnzimmer-heizung"
```

NTP + history settings:

```cpp
#define LOG_INTERVAL_MINUTES     5
#define HISTORY_FILE_PATH        "/hist.bin"
#define HISTORY_CAPACITY_RECORDS 6000
```

---

## Networking, mDNS, OTA

Correct initialization order for reliable OTA and `.local` resolution.

Example:

```cpp
void setup() {
  initWifi();
  initNtp();
  initHistory();
}

void loop() {
  if (otaIsActive()) {
    handleMdns();
    handleOta();
    delay(0);
    return;
  }
  handleSensor();
  handleControl();
  handleWebServer();
  handleMdns();
  handleOta();
  ntpTick();
  handleHistory();
}
```

**Tip:**  
`WiFi.setSleepMode(WIFI_NONE_SLEEP);` on ESP8266.

---

## Web UI

<img width="778" height="796" alt="image" src="https://github.com/user-attachments/assets/54541f14-a06c-4b66-af47-933e44f2ba65" />

Shows live state + config:

- Room (`BASE_TOPIC`)
- Host (`HOST_LABEL`)
- Temperature
- Heater state
- MQTT status
- Setpoint, hysteresis, boost minutes
- Mode selector

Endpoints:

| Endpoint | Method | Description |
|---------|--------|-------------|
| `/` | GET | Main UI |
| `/config` | POST | Apply config |
| `/boost` | POST | Start BOOST |
| `/history.json?days=1` | GET | 24h JSON history |

---

## History Visualization (SVG Chart + Table)

### Fully local (no CDN)

The UI renders **all graphics via inline SVG**, no external JS.

### Features

- Temperature curve  
- Setpoint upper/lower bounds  
- Gridlines  
- Y-axis labels  
- X-axis time ticks  
- **Mouseover tooltip + vertical hover line**  
- **24h history** (`days=1`)

### Table view

Condensed heating phases (`h:1` intervals):

| On | Off | Temp(on) | Temp(off) | Thresholds |

---

## Modes & Logic

- **AUTO:** hysteresis around setPoint  
- **OFF:** forced off  
- **BOOST:** forced on for `boostMinutes`  

State machine toggles relay only on transitions.

---

## LED

- Mirrors heater state (active-low)  
- Occasional 3-pulse diagnostic burst (WiFi / MQTT / Sensor)

---

## Time & History

### NTP / epoch fallback

`getEpochOrUptimeSec()` switches automatically from uptime-based to epoch once NTP sync lands.

### History record format (8 bytes)

```c
uint32 tsSec;
int16  tempCenti;
int16  setPointCenti;
int16  hysteresisCenti;
uint8  flags; // bit0 = heaterOn
uint8  rsv;
```

Logged every `LOG_INTERVAL_MINUTES`.

---

## Troubleshooting

- `.local` not resolving → mDNS blocked, Bonjour missing, wrong SSID  
- OTA unreliable → ensure `handleOta()` runs constantly, no blocking loops  
- Empty history in UI → backend only supports `days=1` (24h) currently  

---

## Notes

- Runs fully standalone; WiFi/MQTT only enhance usability  
- For multi-room: compile once per room  

