#include "ntp.h"
#include "config.h"
#include <time.h>

static uint32_t bootEpochBase = 0;

/***************** validEpoch ***************************************************
 * params: t
 * return: bool
 * Description:
 * Heuristics: consider time valid if after 2021-01-01.
 ******************************************************************************/
static bool validEpoch(time_t t)
{
  return (t >= 1609459200UL);
}

/***************** initNtp ******************************************************
 * params: none
 * return: void
 * Description:
 * Starts SNTP with TZ + two servers. Idempotent.
 ******************************************************************************/
void initNtp()
{
  // Defaults come from config.h; see defines below.
#ifdef NTP_TZ_STRING
  const char* tz = NTP_TZ_STRING;
#else
  const char* tz = "CET-1CEST,M3.5.0,M10.5.0/3"; // Europe/Berlin
#endif

#ifdef ARDUINO_ARCH_ESP8266
  configTime(tz,
             NTP_SERVER_1,  // from config.h (defaults below)
             NTP_SERVER_2);
#else
  // ESP32 also supports configTzTime
  configTzTime(tz,
               NTP_SERVER_1,
               NTP_SERVER_2);
#endif
}

/***************** ntpTick ******************************************************
 * params: none
 * return: void
 * Description:
 * Maintains bootEpochBase once valid time is available.
 ******************************************************************************/
void ntpTick()
{
  time_t now;
  time(&now);

  if (bootEpochBase == 0 && validEpoch(now))
  {
    // Establish base: epoch - uptime
    bootEpochBase = (uint32_t)now - (millis() / 1000UL);
  }
}

/***************** isTimeSynced *************************************************/
bool isTimeSynced()
{
  time_t now;
  time(&now);
  return validEpoch(now);
}

/***************** getEpochNow **************************************************/
uint32_t getEpochNow()
{
  time_t now;
  time(&now);
  return validEpoch(now) ? (uint32_t)now : 0UL;
}

/***************** getEpochOrUptimeSec ******************************************/
uint32_t getEpochOrUptimeSec()
{
  time_t now;
  time(&now);

  if (validEpoch(now))
  {
    // If we didn’t establish base yet (e.g. very early), create it now
    if (bootEpochBase == 0)
    {
      bootEpochBase = (uint32_t)now - (millis() / 1000UL);
    }
    return (uint32_t)now;
  }

  if (bootEpochBase != 0)
  {
    return bootEpochBase + (millis() / 1000UL);
  }

  // Fallback: uptime-based epoch (will automatically jump to absolute later)
  return (millis() / 1000UL);
}
