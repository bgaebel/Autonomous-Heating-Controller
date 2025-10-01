#include "sensor.h"
#include "led.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_Si7021.h>

static Adafruit_Si7021 sensor = Adafruit_Si7021();
static float lastTemp = NAN;
static float lastHumidity = NAN;
static unsigned long lastRead = 0;

/***************** initSensor **************************************************/
bool initSensor()
{
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  if (!sensor.begin())
  {
    Serial.println(F("[SENSOR] GY-21 (Si7021) not found!"));
    return false;
  }

  Serial.println(F("[SENSOR] GY-21 (Si7021) initialized."));
  return true;
}

/***************** sensorLoop ***************************************************/
void handleSensor()
{
  unsigned long now = millis();
  if (now - lastRead < 5000)
    return;

  lastRead = now;
  lastTemp = sensor.readTemperature();
  lastHumidity = sensor.readHumidity();

  Serial.printf("[SENSOR] T=%.2f°C | RH=%.2f%%\n", lastTemp, lastHumidity);
}

/***************** getLastTemperature *******************************************/
float getLastTemperature()
{
  return lastTemp;
}

/***************** getLastHumidity **********************************************/
float getLastHumidity()
{
  return lastHumidity;
}
