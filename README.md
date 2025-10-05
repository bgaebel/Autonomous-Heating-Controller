# Autonomous Heating Controller (ESP8266 / Wemos D1 mini)

A standalone heating controller with hysteresis control, web UI, MQTT integration, OTA updates, mDNS hostname, NTP time, and a LittleFS-backed history ring buffer.

> **TL;DR**  
> - Hardware: Wemos D1 mini (ESP8266), GY‑21 (SHT21/Si7021) via I²C, relay on D6 (HIGH = ON).  
> - Modes: `AUTO`, `OFF`, `BOOST`.  
> - Web UI: status + config (setpoint, hysteresis, boost, mode), shows **room (Base‑Topic)**, **host**, **MQTT status**, **heater state**, **temperature**.  
> - MQTT: telemetry + config/state.  
> - OTA (Arduino IDE) + mDNS (`http://<host>.local`).  
> - Time: NTP-backed epoch time.  
> - Logging: compact 8‑byte records (epoch, temperature, heater flag) in a LittleFS ring buffer, every X minutes.

---

## Hardware / Pinout

- **Board:** Wemos D1 mini (ESP8266)
- **Sensor:** GY‑21 (SHT21/Si7021) over I²C → **D1 = SCL**, **D2 = SDA**, **3V3**, **GND**
- **Relay (heater):** **D6 / GPIO12** (logic: **HIGH = ON**, **LOW = OFF**)
- **Onboard LED:** active‑low (used as heater base + rare status signatures)
- The GY‑21 module usually includes I²C pull‑ups.

---

## Build & Dependencies

- **ESP8266 Arduino Core:** ≥ 3.1.x
- **Libraries (from core unless noted):**
  - `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`
  - `ArduinoOTA`
  - `LittleFS`
  - `time.h` (NTP/SNTP via `configTime`)
  - `ArduinoJson` (external)
  - `PubSubClient` (external)
- **Flash/Partitioning:** Recommended: **4M (FS:2M, OTA:~1M)** to give the history enough room.
- **IDE Port:** For OTA, select the **Network Port** shown as `<host>.local` in the Arduino IDE.

---

## Configuration (compile‑time)

Room and host naming are set at compile time in `config.h`. Example:

```cpp
// Select room (example)
#define ROOM_WOHNZIMMER
#ifdef ROOM_WOHNZIMMER
  #define BASE_TOPIC "Wohnzimmer"
  #define HOST_LABEL "wohnzimmer-heizung"   // DNS/mDNS-safe, lowercase, [a-z0-9-]
#endif
```

NTP and history logging settings (defaults can be overridden):
```cpp
// NTP
#define NTP_TZ_STRING "CET-1CEST,M3.5.0,M10.5.0/3"  // Europe/Berlin
#define NTP_SERVER_1  "pool.ntp.org"
#define NTP_SERVER_2  "time.cloudflare.com"

// History logging
#define LOG_INTERVAL_MINUTES     5           // log every X minutes
#define HISTORY_FILE_PATH        "/hist.bin" // LittleFS path
#define HISTORY_CAPACITY_RECORDS 6000        // 6000 * 8B ≈ 48 KB
```

MQTT credentials (usually in `secrets.h`), and WiFi SSID/PASS must be configured accordingly.

---

## Networking, mDNS & OTA (order matters)

The Arduino IDE discovers the OTA port via mDNS (`_arduino._tcp`). To keep both the OTA port **and** the hostname `http://<host>.local` stable:

1) **Before `WiFi.begin(...)`:** set the **DHCP hostname** (so `http://<host>.fritz.box` may work on FRITZ!Box).  
2) **After `WL_CONNECTED`:** start **mDNS** (`startMdns()`), then immediately **OTA** (`initOta()`).  
3) In the main loop, **always call** `handleMdns()` and `handleOta()`.  
4) On reconnect, run steps **2)** again (mDNS then OTA).

Example (sketch‑level):
```cpp
void setup()
{
  // ...
  initWifi();     // internally: applyDhcpHostname → WiFi.begin → startMdns → initOta
  // ...
  initNtp();      // SNTP start (idempotent)
  initHistory();  // LittleFS + ring buffer
}

void loop()
{
  if (otaIsActive())
  {
    handleMdns();  // keep responder alive during OTA
    handleOta();   // fast-path while uploading
    delay(0);      // feed WDT / cooperative yield
    return;
  }

  handleSensor();
  handleControl();
  handleWebServer();

  handleMdns();
  handleOta();
  ntpTick();       // maintains base epoch once time is valid
  handleHistory(); // logs every LOG_INTERVAL_MINUTES
}
```

> On ESP8266, disable WiFi sleep for better mDNS/OTA stability:  
> `WiFi.setSleepMode(WIFI_NONE_SLEEP);` (before `WiFi.begin`).  
> On ESP32: `WiFi.setSleep(false);`

---

## Web UI

- **GET `/`** – status and configuration:
  - Room (`BASE_TOPIC`) and **Host** (`HOST_LABEL`), shown in header/title
  - **MQTT status** (`OK` / `DOWN (reason)`)
  - Mode, Heater ON/OFF, Temperature (°C)
  - Inputs: Setpoint, Hysteresis, Boost minutes, **Mode dropdown (AUTO/OFF/BOOST)**
- **POST `/config`** – apply `setPoint`, `hysteresis`, `boostMinutes`, `mode`
- **POST `/boost`** – start BOOST for the configured minutes

> The page is served as **UTF‑8** (`text/html; charset=utf‑8`) and includes `<meta charset="utf-8">` (no garbled “â€“”).

---

## MQTT

**BaseTopic:** from `BASE_TOPIC`, e.g. `Wohnzimmer`.

### Publish
- `BaseTopic/telemetry` (periodic):
  ```json
  { "temp": 21.3, "humidity": 48, "state": "IDLE|HEATING|ERROR", "mode": "AUTO|OFF|BOOST" }
  ```
- `BaseTopic/state` (on change):
  ```json
  { "setPoint": 21.0, "hysteresis": 0.6, "boostMinutes": 20, "mode": "AUTO|OFF|BOOST" }
  ```

### Subscribe
- `BaseTopic/cmd` – JSON commands:
  ```json
  { "setPoint": 22.0 }
  { "hysteresis": 0.8 }
  { "boostMinutes": 30 }
  { "mode": 0 }   // 0=AUTO, 1=OFF, 2=BOOST
  ```

---

## Modes & Control Logic

- **AUTO:** hysteresis control around `setPoint ± hysteresis`. Transition‑based actuator toggling (no spam).  
- **OFF:** heater forced OFF.  
- **BOOST:** heater forced ON until the computed `boostEndMillis`; switching is done **once** on mode entry.

The state machine handles mode behavior **inside the switch‑case**, transitions trigger the relay once. Heater LED base is updated on relay **edges** only.

---

## LED behavior (concise)

- **Base:** LED mirrors **heater ON/OFF** (active‑low if using onboard LED).  
- **Rare status signature, every X minutes:** a **3‑pulse group** (short = OK, long = FAIL)
  1. **WiFi**  
  2. **MQTT**  
  3. **Sensor**  
- Pulses invert the base briefly (XOR). No continuous blinking.

Intervals and pulse lengths are configurable via `led.h` defines.

---

## Time & History Logging

### NTP
- `initNtp()` starts SNTP with time zone (`NTP_TZ_STRING`) and servers (`NTP_SERVER_1/2`).  
- `ntpTick()` establishes a **bootEpochBase** once valid time is available.  
- `getEpochOrUptimeSec()` always returns seconds: uptime‑derived first, then absolute epoch once synced (automatic transition).

### History (LittleFS ring buffer)
- Record (8 bytes):  
  `uint32 tsSec` (epoch), `int16 tempCenti` (°C ×100), `uint8 flags` (bit0=heaterOn), `uint8 rsv`  
- File path: `HISTORY_FILE_PATH` (e.g. `/hist.bin`)  
- Capacity: `HISTORY_CAPACITY_RECORDS` (e.g. 6000 → ~48 KB)  
- Interval: `LOG_INTERVAL_MINUTES` (e.g. 5)  
- API:
  - `initHistory()`
  - `handleHistory()`
  - `readHistoryTail(size_t maxOut, LogSample* outBuf, size_t* outCount)`

> With 2 MB LittleFS, you can store **~250k** samples at 8 bytes each (~173 days at 60 s cadence). Adjust capacity to your FS size.

---

## First Run

1. Flash via USB, open **Serial Monitor**.  
2. Set **room/host** in `config.h` (`BASE_TOPIC`, `HOST_LABEL`).  
3. Configure WiFi/MQTT credentials (e.g., in `secrets.h`).  
4. Flash the sketch.  
5. Open web UI: `http://<HOST_LABEL>.local/` (e.g., `http://wohnzimmer-heizung.local/`).  
6. Verify MQTT: telemetry under `<BaseTopic>/telemetry`.

---

## Troubleshooting

- **`.local` not resolving:** same SSID/L2? `handleMdns()` called in loop? On Windows, install **Bonjour** (Apple Bonjour Print Services). Guest networks often block mDNS.  
- **OTA “No response from device”:** ensure `handleOta()` is called in the loop; use the **OTA fast‑path** (skip other work while uploading). Avoid blocking loops (`while` + `delay(100)`); add `delay(0)` on ESP8266.  
- **MQTT DOWN:** broker address/ports/credentials; web UI shows `MQTT=DOWN (reason)`.  
- **History empty:** LittleFS partition too small? Adjust `HISTORY_CAPACITY_RECORDS` accordingly.  
- **Encoding issues (â€“):** page uses UTF‑8 (`meta + Content‑Type`) and HTML entity `&ndash;` in titles.

---

## Quick Reference (endpoints & formats)

- **Web:** `/` (status & config), `/boost` (POST)  
- **MQTT Pub:** `<BaseTopic>/telemetry`, `<BaseTopic>/state`  
- **MQTT Sub:** `<BaseTopic>/cmd` (JSON)  
- **Mode Codes:** `0 = AUTO`, `1 = OFF`, `2 = BOOST`  
- **History Record (binary):** `uint32 tsSec`, `int16 tempCenti`, `uint8 flags`, `uint8 rsv`

---

## Notes

- The controller runs independently; WiFi/MQTT enhance visibility and remote control but are not required for basic regulation.
- For multi‑room setups, compile one firmware per room with distinct `HOST_LABEL` and `BASE_TOPIC`.
