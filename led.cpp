#include <Arduino.h>
#include "led.h"

// Adjust this to your relay/LED pin
const int LED_PIN = LED_BUILTIN;

// Internal state
static uint8_t currentLedState = LED_OFF;
static unsigned long lastBlinkTime = 0;
static uint8_t blinkCount = 0;
static bool ledOn = false;

/***************** initLed ******************************************************
 * Description:
 * Initializes the LED pin and sets initial state to OFF.
 ******************************************************************************/
void initLed()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.println(F("[LED] Initialized"));
}

/***************** setLedState **************************************************
 * params:
 *   state: one of the LED_* constants
 * Description:
 * Sets the blink mode based on system status.
 ******************************************************************************/
void setLedState(uint8_t state)
{
  currentLedState = state;
  blinkCount = 0;
  ledOn = false;
  lastBlinkTime = millis();

  Serial.print(F("[LED] State changed to "));
  Serial.println(state);
}

/***************** handleLed ****************************************************
 * Description:
 * Non-blocking LED blink handler.
 * Called frequently in loop().
 * LED pattern:
 *   WIFI_CONNECTING   -> 2 short blinks
 *   MQTT_CONNECTING   -> 3 short blinks
 *   RUNNING           -> 1 short blink
 *   ERROR             -> 5 short blinks
 ******************************************************************************/
void handleLed()
{
  unsigned long now = millis();
  static unsigned long blinkInterval = 150;   // short blink
  static unsigned long pauseInterval = 2000;  // long pause

  uint8_t targetBlinkCount = 0;

  switch (currentLedState)
  {
    case LED_OFF: 
      digitalWrite(LED_PIN, HIGH);
      return;
    case LED_RUNNING:
      targetBlinkCount = 1;
      break;
    case LED_WIFI_CONNECTING:
      targetBlinkCount = 2;
      break;
    case LED_MQTT_CONNECTING:
      targetBlinkCount = 3;
      break;
    case LED_ERROR:
      targetBlinkCount = 5;
      break;
  }

  // Blink logic
  if (blinkCount < targetBlinkCount)
  {
    if (!ledOn && now - lastBlinkTime >= pauseInterval)
    {
      ledOn = true;
      digitalWrite(LED_PIN, LOW);
      lastBlinkTime = now;
    }
    else if (ledOn && now - lastBlinkTime >= blinkInterval)
    {
      ledOn = false;
      digitalWrite(LED_PIN, HIGH);
      lastBlinkTime = now;
      blinkCount++;
    }
  }
  else
  {
    // Wait for next sequence
    if (now - lastBlinkTime >= pauseInterval)
    {
      blinkCount = 0;
    }
  }
}
