#include "config.h"
#include <string.h>
#include <time.h>

Config config;

static float clampf(float v, float lo, float hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/***************** loadConfig ***************************************************
 * params: none
 * return: void
 * Description:
 * Loads configuration from EEPROM. If invalid, uses defaults and writes them.
 ******************************************************************************/
void loadConfig()
{
  EEPROM.begin(EEPROM_SIZE);

  Config tmp;
  EEPROM.get(EEPROM_ADDR, tmp);

  bool ok = (tmp.magic == CONFIG_MAGIC) && (tmp.version == CONFIG_VERSION);

  if (!ok)
  {
    config = Config{};
    saveConfig();
    Serial.println(F("[CONFIG] Defaults stored (invalid header)."));
    return;
  }

  // Range-sanitize to avoid nonsense values
  tmp.daySetPoint   = clampf(tmp.daySetPoint, 5.0f, 35.0f);
  tmp.nightSetPoint = clampf(tmp.nightSetPoint, 5.0f, 35.0f);
  tmp.dayStartMin   = (uint16_t)max(0, min(1439, (int)tmp.dayStartMin));
  tmp.nightStartMin = (uint16_t)max(0, min(1439, (int)tmp.nightStartMin));
  tmp.hysteresis    = clampf(tmp.hysteresis, 0.1f, 5.0f);
  if (tmp.boostMinutes < 0)   tmp.boostMinutes = 0;
  if (tmp.boostMinutes > 240) tmp.boostMinutes = 240;

  config = tmp;
  Serial.println(F("[CONFIG] Loaded from EEPROM."));
}

/***************** saveConfig ***************************************************
 * params: none
 * return: void
 * Description:
 * Writes config to EEPROM. Keep the number of writes conservative.
 ******************************************************************************/
void saveConfig()
{
  EEPROM.put(EEPROM_ADDR, config);
  EEPROM.commit();
}

/***************** getSetPoint **************************************************/
float getSetPoint()
{
  return isDayScheduleActive() ? config.daySetPoint : config.nightSetPoint;
}

/***************** getDaySetPoint **********************************************/
float getDaySetPoint()
{
  return config.daySetPoint;
}

/***************** getNightSetPoint ********************************************/
float getNightSetPoint()
{
  return config.nightSetPoint;
}

/***************** setDaySetPoint **********************************************/
void setDaySetPoint(float v)
{
  config.daySetPoint = clampf(v, 5.0f, 35.0f);
  saveConfig();
}

/***************** setNightSetPoint ********************************************/
void setNightSetPoint(float v)
{
  config.nightSetPoint = clampf(v, 5.0f, 35.0f);
  saveConfig();
}

/***************** schedule helpers ********************************************/
static int clampMinutesOfDay(int v)
{
  if (v < 0) return 0;
  if (v > 1439) return 1439;
  return v;
}

static int minutesOfDay()
{
  time_t now;
  time(&now);
  struct tm *t = localtime(&now);
  if (!t)
  {
    return 0;
  }
  return t->tm_hour * 60 + t->tm_min;
}

bool isDayScheduleActive()
{
  const int dStart = config.dayStartMin;
  const int nStart = config.nightStartMin;
  const int nowMin = minutesOfDay();

  if (dStart == nStart)
  {
    return true; // Degenerates to "immer Tag"
  }

  if (dStart < nStart)
  {
    return (nowMin >= dStart) && (nowMin < nStart);
  }

  // Tag beginnt z.B. 22:00 und läuft über Mitternacht
  return (nowMin >= dStart) || (nowMin < nStart);
}

int getDayStartMinutes()
{
  return config.dayStartMin;
}

int getNightStartMinutes()
{
  return config.nightStartMin;
}

void setDayStartMinutes(int v)
{
  config.dayStartMin = (uint16_t)clampMinutesOfDay(v);
  saveConfig();
}

void setNightStartMinutes(int v)
{
  config.nightStartMin = (uint16_t)clampMinutesOfDay(v);
  saveConfig();
}

/***************** getHysteresis ************************************************/
float getHysteresis()
{
  return config.hysteresis;
}

/***************** setHysteresis ************************************************/
void setHysteresis(float v)
{
  config.hysteresis = clampf(v, 0.1f, 5.0f);
  saveConfig();
}

/***************** getBoostMinutes **********************************************/
int getBoostMinutes()
{
  return config.boostMinutes;
}

/***************** setBoostMinutes **********************************************
 * params: v
 * return: void
 * Description:
 * Sets BOOST duration (minutes) and persists.
 ******************************************************************************/
void setBoostMinutes(int v)
{
  if (v < 0) v = 0;
  if (v > 240) v = 240;
  config.boostMinutes = v;
  saveConfig();
}

/***************** getBoostEndTime **********************************************/
unsigned long getBoostEndTime()
{
  return config.boostEndMillis;
}

/***************** setBoostEndTime **********************************************
 * params: t
 * return: void
 * Description:
 * Sets BOOST end timestamp (millis, 0 = disabled) and persists.
 ******************************************************************************/
void setBoostEndTime(unsigned long t)
{
  config.boostEndMillis = t;
  saveConfig();
}

/***************** getBaseTopic *************************************************
 * params: none
 * return: const char*
 * Description:
 * Returns the compile-time selected base topic / room label (e.g. "Wohnzimmer").
 ******************************************************************************/
const char* getBaseTopic()
{
  return BASE_TOPIC;
}

/***************** getHostLabel *************************************************
 * params: none
 * return: const char*
 * Description:
 * Returns the DNS-safe host label derived from the room (e.g. "wohnzimmer").
 ******************************************************************************/
const char* getHostLabel()
{
  return HOST_LABEL;
}

