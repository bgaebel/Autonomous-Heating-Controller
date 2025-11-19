#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>
#include "control.h"

#define APP_VERSION "1.2"

/***************** BaseTopic Selection ******************************************
 * Select ONE room by uncommenting it. Used to generate MQTT base topic.
 ******************************************************************************/
//#define ROOM_WOHNZIMMER
// #define ROOM_SCHLAFZIMMER
 #define ROOM_KUECHE
// #define ROOM_KL_KINDERZIMMER
// #define ROOM_GR_KINDERZIMMER

#ifdef ROOM_WOHNZIMMER
  #define BASE_TOPIC "Wohnzimmer"
  #define HOST_LABEL "wohnzimmer-heizung"
#elif defined(ROOM_SCHLAFZIMMER)
  #define BASE_TOPIC "Schlafzimmer"
  #define HOST_LABEL "schlafzimmer-heizung"
#elif defined(ROOM_KUECHE)
  #define BASE_TOPIC "Kueche"
  #define HOST_LABEL "kueche-heizung"
#elif defined(ROOM_KL_KINDERZIMMER)
  #define BASE_TOPIC "KlKinderzimmer"
  #define HOST_LABEL "kl-kinderzimmer-heizung"
#elif defined(ROOM_GR_KINDERZIMMER)
  #define BASE_TOPIC "GrKinderzimmer"
  #define HOST_LABEL "gr-kinderzimmer-heizung"
#else
  #define BASE_TOPIC "UnknownRoom"
  #define HOST_LABEL "unknown-heizung"
#endif

/***************** Hardware Pins ************************************************/
#define RELAY_PIN D6
#define I2C_SDA   D2
#define I2C_SCL   D1

/***************** EEPROM Layout ***********************************************
 * params: none
 * return: n/a
 * Description:
 * Single source of truth for all persistent settings.
 * MAGIC + VERSION validate structure. No CRC (Variant 2).
 ******************************************************************************/
#define CONFIG_MAGIC     0x43464721UL   // "CFG!"
#define CONFIG_VERSION   2
#define EEPROM_ADDR      0
#define EEPROM_SIZE      128

/***************** Struct: Config **********************************************
 * params: none
 * return: n/a
 * Description:
 * Persistent configuration. Keep POD-compatible for EEPROM.get/put.
 ******************************************************************************/
struct Config
{
  uint32_t magic = CONFIG_MAGIC;
  uint16_t version = CONFIG_VERSION;
  uint16_t reserved = 0;

  // Control parameters
  float       setPoint     = 21.0f;     // °C
  float       hysteresis   = 0.6f;      // °C
  int         boostMinutes = 20;        // min

  // BOOST timing (persist to survive reset)
  unsigned long boostEndMillis = 0;
};

extern Config config;

// ---------- NTP ----------
#ifndef NTP_TZ_STRING
#define NTP_TZ_STRING "CET-1CEST,M3.5.0,M10.5.0/3"  // Europe/Berlin
#endif

#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.cloudflare.com"
#endif

// ---------- History Logging ----------
#ifndef LOG_INTERVAL_MINUTES
#define LOG_INTERVAL_MINUTES 5   // <- hier Stellrad (X Minuten)
#endif

#ifndef HISTORY_FILE_PATH
#define HISTORY_FILE_PATH "/hist.bin"
#endif

#ifndef HISTORY_CAPACITY_RECORDS
#define HISTORY_CAPACITY_RECORDS 6000  // 6000*8B ≈ 48 KB auf FS
#endif


/***************** loadConfig ***************************************************
 * params: none
 * return: void
 * Description:
 * Loads configuration from EEPROM. If invalid, writes defaults immediately.
 ******************************************************************************/
void loadConfig();

/***************** saveConfig ***************************************************
 * params: none
 * return: void
 * Description:
 * Persists configuration to EEPROM. Call after mutating fields.
 ******************************************************************************/
void saveConfig();

/***************** Accessors ****************************************************/
float getSetPoint();
void  setSetPoint(float v);

float getHysteresis();
void  setHysteresis(float v);

int   getBoostMinutes();
void  setBoostMinutes(int v);

unsigned long getBoostEndTime();
void  setBoostEndTime(unsigned long t);

const char* getBaseTopic();
const char* getHostLabel();

#endif // CONFIG_H
