#include "led.h"

/***************** Internal State **********************************************/
static bool baseHeaterOn   = false;

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
 * Initializes LED GPIO and sets default (base OFF).
 ******************************************************************************/
void initLed()
{
  pinMode(LED_PIN, OUTPUT);
  baseHeaterOn = false;
  writeLedLogical(false);
}

/***************** ledSetBaseFromHeater *****************************************
 * params: heaterOn
 * return: void
 * Description:
 * Mirrors relay state directly to the LED.
 ******************************************************************************/
void ledSetBaseFromHeater(bool heaterOn)
{
  baseHeaterOn = heaterOn;
  writeLedLogical(baseHeaterOn);
}

/***************** handleLed ****************************************************/
void handleLed()
{
  writeLedLogical(baseHeaterOn);
}
