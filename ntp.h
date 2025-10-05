#ifndef NTP_TIME_H
#define NTP_TIME_H

#include <Arduino.h>

/***************** initNtp ******************************************************
 * params: none
 * return: void
 * Description:
 * Starts SNTP using TZ + NTP servers. Safe to call multiple times.
 * Works even if WiFi connects later; SNTP updates when online.
 ******************************************************************************/
void initNtp();

/***************** ntpTick ******************************************************
 * params: none
 * return: void
 * Description:
 * Periodic maintenance. Establishes a bootEpochBase once real time is known.
 ******************************************************************************/
void ntpTick();

/***************** isTimeSynced *************************************************
 * params: none
 * return: bool
 * Description:
 * Returns true once the device has a valid epoch time from NTP.
 ******************************************************************************/
bool isTimeSynced();

/***************** getEpochNow **************************************************
 * params: none
 * return: uint32_t
 * Description:
 * Returns current epoch seconds (UTC). If not synced yet, returns 0.
 ******************************************************************************/
uint32_t getEpochNow();

/***************** getEpochOrUptimeSec ******************************************
 * params: none
 * return: uint32_t
 * Description:
 * Returns absolute epoch seconds if synced; otherwise returns a synthetic epoch
 * derived from bootEpochBase + uptime. Becomes absolute automatically once NTP
 * is available.
 ******************************************************************************/
uint32_t getEpochOrUptimeSec();

#endif // NTP_TIME_H
