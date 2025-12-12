#include "led.h"

/***************** Helpers ******************************************************/
static inline void writeLedLogical(bool logicalOn)
{
#if LED_ACTIVE_LOW
  digitalWrite(LED_PIN, logicalOn ? LOW : HIGH);
#else
  digitalWrite(LED_PIN, logicalOn ? HIGH : LOW);
#endif
}

/***************** initLed ******************************************************
 * params: none
 * return: void
 * Description:
 * Initializes LED GPIO and sets default (OFF).
 ******************************************************************************/
void initLed()
{
  pinMode(LED_PIN, OUTPUT);
  writeLedLogical(false);
}

/***************** ledSetBaseFromHeater *****************************************
 * params: heaterOn
 * return: void
 * Description:
 * Mirrors heater state directly to the LED.
 ******************************************************************************/
void ledSetBaseFromHeater(bool heaterOn)
{
  writeLedLogical(heaterOn);
}

/***************** handleLed ****************************************************
 * params: none
 * return: void
 * Description:
 * Non-blocking LED handler. No internal state needed.
 ******************************************************************************/
void handleLed()
{
  // no-op
}
