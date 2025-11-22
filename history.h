#ifndef HISTORY_H
#define HISTORY_H

#include <Arduino.h>

/***************** Configuration ************************************************
 * params: none
 * return: n/a
 * Description:
 * Tuning knobs for sampling and storage. Adjust to your FS size.
 ******************************************************************************/
#ifndef HISTORY_SAMPLE_PERIOD_MIN
#define HISTORY_SAMPLE_PERIOD_MIN 5          // <- alle X Minuten loggen
#endif

#ifndef HISTORY_FILE_BYTES
#define HISTORY_FILE_BYTES (256UL * 1024UL)  // 256 KB Ringpuffer
#endif

#ifndef HISTORY_FILE_PATH
#define HISTORY_FILE_PATH "/hist.bin"
#endif

/***************** LogSample ****************************************************
 * params: n/a
 * return: n/a
 * Description:
 * Compact, fixed-size record: 12 bytes total.
 * - tsSec           : uint32_t epoch seconds (UTC)
 * - tempCenti       : int16_t  temperature * 100 (°C * 100)
 * - setPointCenti   : int16_t  set point  * 100 (°C * 100)
 * - hysteresisCenti : int16_t  hysteresis * 100 (°C * 100)
 * - flags           : bit0 = heaterOn (1=ON), bit1..7 reserved
 * - rsv             : reserved for alignment/future use
 ******************************************************************************/
struct LogSample
{
  uint32_t tsSec;
  int16_t  tempCenti;
  int16_t  setPointCenti;
  int16_t  hysteresisCenti;
  uint8_t  flags;
  uint8_t  rsv;
};

/***************** initHistory **************************************************
 * params: none
 * return: bool
 * Description:
 * Mounts LittleFS and (re)creates/validates a fixed-size ring buffer file.
 ******************************************************************************/
bool initHistory();

/***************** handleHistory ************************************************
 * params: none
 * return: void
 * Description:
 * Periodic sampler. Every HISTORY_SAMPLE_PERIOD_MIN minutes appends one record
 * using getUnixTimeSec() (0 -> skip until NTP is ready).
 ******************************************************************************/
void handleHistory();

/***************** appendHistory ************************************************
 * params: tsSec, tempCenti, heaterOn
 * return: bool
 * Description:
 * Appends a single LogSample into the ring buffer immediately using the
 * current set point and hysteresis values for threshold reconstruction.
 ******************************************************************************/
bool appendHistory(uint32_t tsSec, int16_t tempCenti, bool heaterOn);

/***************** readHistoryTail *********************************************
 * params: maxOut, outBuf, outCount
 * return: size_t
 * Description:
 * Reads up to maxOut most-recent samples into outBuf. Returns number read.
 ******************************************************************************/
size_t readHistoryTail(size_t maxOut, LogSample* outBuf, size_t* outCount);

#endif
