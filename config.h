#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>
#include "control.h"

/***************** BaseTopic Selection ******************************************
 * Select ONE room by uncommenting it. Used to generate MQTT base topic.
 ******************************************************************************/
#define ROOM_WOHNZIMMER
// #define ROOM_SCHLAFZIMMER
// #define ROOM_KUECHE
// #define ROOM_KL_KINDERZIMMER
// #define ROOM_GR_KINDERZIMMER

#ifdef ROOM_WOHNZIMMER
  #define BASE_TOPIC "Wohnzimmer"
#elif defined(ROOM_SCHLAFZIMMER)
  #define BASE_TOPIC "Schlafzimmer"
#elif defined(ROOM_KUECHE)
  #define BASE_TOPIC "Kueche"
#elif defined(ROOM_KL_KINDERZIMMER)
  #define BASE_TOPIC "KlKinderzimmer"
#elif defined(ROOM_GR_KINDERZIMMER)
  #define BASE_TOPIC "GrKinderzimmer"
#else
  #define BASE_TOPIC "UnknownRoom"
#endif

/***************** Hardware Pins ************************************************/
#define RELAY_PIN D6
#define I2C_SDA   D2
#define I2C_SCL   D1

/***************** EEPROM Address ***********************************************/
#define EEPROM_SIZE 64
#define EEPROM_ADDR 0

/***************** Data Structure ***********************************************/
struct Config
{
  float       setPoint;      // Desired temperature
  float       hysteresis;    // Hysteresis value
  int         boostMinutes;  // Duration of boost mode
  ControlMode mode;          // Operation mode
};

/***************** Global Variable **********************************************/
extern Config config;

/***************** Persistence **************************************************/
void loadConfig();
void saveConfig();

/***************** Config Accessors *********************************************/
float getSetPoint();
void  setSetPoint(float v);

float getHysteresis();
void  setHysteresis(float v);

int   getBoostMinutes();
void  setBoostMinutes(int v);

#endif
