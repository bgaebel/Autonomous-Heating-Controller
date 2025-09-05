# Autonomous Heating Controller (ESP8266 / Wemos D1 mini)

## Overview

-   Runs **standalone**: heating logic works without WiFi/MQTT.\
-   Hardware: Wemos D1 mini (ESP8266), GY-21 (SHT21/Si7021) via I²C,
    relay on D6 (HIGH=ON).\
-   Features: hysteresis controller, modes **auto/off/boost/summer**,
    Web UI, MQTT (telemetry + config), LittleFS config, OTA (Arduino
    IDE).\
-   Multi-room: unique BaseTopics per device via ChipID→room mapping.

------------------------------------------------------------------------

## Wiring

-   GY-21 → **D1=SCL**, **D2=SDA**, **3V3**, **GND**\
-   Relay/heating → **D6** (GPIO12), HIGH=ON, LOW=OFF\
-   I²C pullups: usually present on GY-21 module.

------------------------------------------------------------------------

## Build & Dependencies

-   **Board:** ESP8266 (e.g. Wemos D1 mini), ESP8266 Arduino Core ≥
    3.1.x\
-   **Libraries:**
    -   ArduinoJson\
    -   PubSubClient\
    -   ESP8266WebServer (Core)\
    -   LittleFS (Core)\
    -   ESP8266mDNS (Core)\
    -   ArduinoOTA (Core)\
-   **Filesystem:** LittleFS (initialized automatically; no upload
    required)\
-   **OTA:** Arduino IDE → select network port
    (`heatctrl-<chipid>.local`)

------------------------------------------------------------------------

## First Startup

1.  **Flash via USB**, open Serial Monitor → note down `ChipID (HEX)`\
2.  In the code, map **ChipID→room** inside `chooseBaseTopicByChip()`\
3.  **Change OTA password** (`setupOta()` → `setPassword`)\
4.  Flash again\
5.  Configure WiFi + MQTT via Web UI (`/config`)\
6.  Check MQTT broker → telemetry should appear under
    `<BaseTopic>/telemetry`

------------------------------------------------------------------------

## Web UI (HTTP)

-   `GET /` → status (Temp, Humidity, State, Mode, Heater, SetPoint,
    Hysteresis)\
-   `GET /config` → configuration form\
-   `POST /config` → form fields:
    -   `wifiSsid`, `wifiPass`
    -   `mqttServer`, `mqttPort`, `mqttUser`, `mqttPass`,
        `mqttBaseTopic`
    -   `setPoint` (°C), `hysteresis` (°C, min 0.2), `boostMinutes`
        (0--65535)
    -   **Summer Mode:** `summerAuto` (checkbox), `idleMargin` (°C),
        `summerTriggerMs`, `summerExitMs`

**AP fallback:** If STA fails, device starts AP `HeatCtrl-ESP8266`,
password `heat1234` (change!).

------------------------------------------------------------------------

## MQTT

### Topics (relative to `<BaseTopic>`)

-   Publish:
    -   `telemetry` (retained):

        ``` json
        {
          "temp": 21.3,       // or null
          "humidity": 48,     // or null
          "state": "idle|heating|safe|init",
          "heaterOn": false,
          "mode": "auto|off|boost|summer"
        }
        ```

    -   `state` (retained):

        ``` json
        {
          "mode": "auto|off|boost|summer",
          "setPoint": 21.0,
          "hysteresis": 0.6,
          "boostMinutes": 20,
          "summerAuto": true,
          "idleMargin": 0.8,
          "summerTriggerMs": 21600000,
          "summerExitMs": 600000
        }
        ```
-   Subscribe:
    -   `set`:

        ``` json
        { "setPoint": 22.0 }
        { "hysteresis": 0.8 }
        { "boostMinutes": 30 }
        { "mode": "off" }          // auto | off | boost | summer
        { "summerAuto": true }
        { "idleMargin": 0.8 }
        { "summerTriggerMs": 21600000 }
        { "summerExitMs": 600000 }
        ```

**Note:** UTF-8 topics with spaces/umlauts work, but some MQTT tools
don't like them. Use ASCII variants (`Kueche`, `GrKinderzimmer`, ...) if
needed.

------------------------------------------------------------------------

## Modes

-   **AUTO:** hysteresis control around `setPoint ± hysteresis/2`.\
-   **OFF:** heater forced OFF.\
-   **BOOST:** heater forced ON for `boostMinutes`, obeys min on/off
    times; reverts to previous mode afterwards.\
-   **SUMMER:** heater forced OFF; automatic exit after being below low
    threshold for `summerExitMs`.
    -   **Low threshold:** `setPoint − hysteresis/2`\
    -   **Summer Auto entry:** if above `setPoint + idleMargin` for
        `summerTriggerMs` while heater OFF.

------------------------------------------------------------------------

## State Machine (simplified)

               +---------+
               |  INIT   |
               +----+----+
                    |
          sensor ok v
               +----+-----+         temp>high & minOff?
       +------>|  IDLE    |---------------------------+
       |       +----+-----+                           |
       | temp<low & minOff?                           |
       |             v                                |
       |       +-----+-----+                          |
       +-------|  HEATING  |<-------------------------+
               +-----+-----+
                     |
             sensor error / invalid temp
                     v
               +-----+-----+
               |   SAFE    |  -> Heater OFF, auto return if sensor ok
               +-----------+

------------------------------------------------------------------------

## Persistence

-   **File:** `/config.json` (LittleFS)\
-   On first boot → defaults created (including BaseTopic from
    ChipID→room).

Example:

``` json
{
  "mqttBaseTopic": "Livingroom",
  "setPoint": 21.0,
  "hysteresis": 0.6,
  "boostMinutes": 20,
  "summerAuto": true,
  "idleMargin": 0.8,
  "summerTriggerMs": 21600000,
  "summerExitMs": 600000
}
```

------------------------------------------------------------------------

## OTA (Arduino IDE)

-   Hostname: `heatctrl-<chipid>.local`\
-   Port: 8266 (default)\
-   Password: **change it!** (`setupOta()` → `setPassword`)\
-   During OTA: relay set **OFF**, device reboots afterwards.

------------------------------------------------------------------------

## ChipID → BaseTopic Mapping

Inside `chooseBaseTopicByChip()`, replace placeholders with your ChipIDs
(HEX, 6 digits):

``` cpp
if (chip == 0xAAAAAA) return "Livingroom";
else if (chip == 0xBBBBBB) return "Bedroom";
// …
else return String("Unknown/") + String(ESP.getChipId(), HEX);
```

ChipID is printed to Serial at boot.

------------------------------------------------------------------------

## Safety & Robustness

-   Min on/off times protect the relay.\
-   `STATE_SAFE`: on sensor errors → heater OFF; auto return when sensor
    recovers.\
-   AP fallback has default password → **change it** if you use AP mode
    long-term.

------------------------------------------------------------------------

## Testing (without heater)

1.  Connect D6 to LED/test load.\
2.  Adjust setPoint just above/below ambient → check switching.\
3.  Send MQTT `{ "mode":"boost" }` → relay ON for boostMinutes.\
4.  Enable `summerAuto`, set `idleMargin` small, `summerTriggerMs` short
    → check auto-entry to Summer.\
5.  Set `summerExitMs` short → check auto-exit.

------------------------------------------------------------------------

## Troubleshooting

-   **Compile error `max(...)`:** types must match; fixed in code
    (explicit clamp/cast).\
-   **`nullptr` vs. float in JSON:** fixed (if/else instead of
    ternary).\
-   **OTA port missing:** check WiFi STA, AP fallback disables OTA, mDNS
    may be blocked → use IP upload.\
-   **No MQTT telemetry:** verify BaseTopic unique per device, broker
    creds correct.\
-   **Web UI unreachable:** check IP via Serial, try AP fallback.
