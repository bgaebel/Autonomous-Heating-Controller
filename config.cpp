#include "config.h"
#include <string.h>

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
  tmp.setPoint    = clampf(tmp.setPoint, 5.0f, 35.0f);
  tmp.hysteresis  = clampf(tmp.hysteresis, 0.1f, 5.0f);
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
  return config.setPoint;
}

/***************** setSetPoint **************************************************
 * params: v
 * return: void
 * Description:
 * Sets temperature setpoint (°C) and persists.
 ******************************************************************************/
void setSetPoint(float v)
{
  config.setPoint = clampf(v, 5.0f, 35.0f);
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

/***************** getFirmwareVersion *******************************************/
const char* getFirmwareVersion()
{
  return APP_VERSION;
}

