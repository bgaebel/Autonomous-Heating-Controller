#include "config.h"

Config config;

/***************** loadConfig ***************************************************
 * Description:
 * Loads configuration from EEPROM. If invalid, uses default values.
 ******************************************************************************/
void loadConfig()
{
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, config);

  // Validate values
  if (isnan(config.setPoint) || config.setPoint < 5 || config.setPoint > 35)
  {
    Serial.println(F("[CONFIG] Invalid EEPROM data. Loading defaults."));
    config.setPoint = 21.0;
    config.hysteresis = 0.5;
    config.boostMinutes = 15;
    config.mode = MODE_AUTO;
    saveConfig();
  }
  else
  {
    Serial.println(F("[CONFIG] Loaded from EEPROM:"));
  }

  Serial.printf("  SetPoint: %.2f°C\n", config.setPoint);
  Serial.printf("  Hysteresis: %.2f°C\n", config.hysteresis);
  Serial.printf("  Boost: %d min\n", config.boostMinutes);
  Serial.printf("  Mode: %d\n", config.mode);
}

/***************** saveConfig ***************************************************
 * Description:
 * Writes current configuration to EEPROM.
 ******************************************************************************/
void saveConfig()
{
  EEPROM.put(EEPROM_ADDR, config);
  EEPROM.commit();
  Serial.println(F("[CONFIG] Saved to EEPROM."));
}

/***************** getSetPoint **************************************************
 * Description:
 * Returns the current configured temperature setpoint in °C.
 ******************************************************************************/
float getSetPoint()
{
  return config.setPoint;
}

/***************** setSetPoint **************************************************
 * params:
 *   v: new temperature setpoint in °C
 * Description:
 * Updates the temperature setpoint and saves the configuration to EEPROM.
 ******************************************************************************/
void setSetPoint(float v)
{
  config.setPoint = v;
  saveConfig();
}

/***************** getHysteresis ************************************************
 * Description:
 * Returns the current configured hysteresis in °C.
 ******************************************************************************/
float getHysteresis()
{
  return config.hysteresis;
}

/***************** setHysteresis ************************************************
 * params:
 *   v: new hysteresis value in °C
 * Description:
 * Updates the hysteresis value and saves the configuration to EEPROM.
 ******************************************************************************/
void setHysteresis(float v)
{
  config.hysteresis = v;
  saveConfig();
}

/***************** getBoostMinutes *********************************************
 * Description:
 * Returns the currently configured boost duration in minutes.
 ******************************************************************************/
int getBoostMinutes()
{
  return config.boostMinutes;
}

/***************** setBoostMinutes *********************************************
 * params:
 *   v: new boost duration in minutes
 * Description:
 * Updates the boost duration and saves the configuration to EEPROM.
 ******************************************************************************/
void setBoostMinutes(int v)
{
  config.boostMinutes = v;
  saveConfig();
}
