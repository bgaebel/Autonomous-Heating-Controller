#include "history.h"
#include "config.h"
#include "ntp.h"
#include "sensor.h"
#include "control.h"
#include <LittleFS.h>

struct HistoryHeader
{
  uint32_t magic;
  uint16_t version;
  uint16_t recSize;
  uint32_t capacity;   // Anzahl Records
  uint32_t head;       // nächste Schreibposition (0..capacity-1)
  uint32_t count;      // gültige Anzahl
  uint32_t reserved;
};

static const uint32_t HIST_MAGIC   = 0x48495354UL; // "HIST"
static const uint16_t HIST_VERSION = 4;

static File           histFile;
static HistoryHeader  hdr;

/***************** fileOpenOrCreate ********************************************
 * params: none
 * return: bool
 * Description:
 * Öffnet vorhandene History-Datei oder erstellt sie inkl. Header und Größe.
 ******************************************************************************/
static bool fileOpenOrCreate()
{
  if (!LittleFS.begin())
  {
    Serial.println(F("[HIST] LittleFS mount failed"));
    return false;
  }

  if (!LittleFS.exists(HISTORY_FILE_PATH))
  {
    // Neu anlegen
    histFile = LittleFS.open(HISTORY_FILE_PATH, "w+");
    if (!histFile)
    {
      Serial.println(F("[HIST] Create file failed"));
      return false;
    }

    hdr.magic    = HIST_MAGIC;
    hdr.version  = HIST_VERSION;
    hdr.recSize  = sizeof(LogSample);
    hdr.capacity = HISTORY_CAPACITY_RECORDS;
    hdr.head     = 0;
    hdr.count    = 0;
    hdr.reserved = 0;

    // Header schreiben
    histFile.seek(0, SeekSet);
    histFile.write((const uint8_t*)&hdr, sizeof(hdr));

    // Datei auf Zielgröße bringen (Header + capacity*recSize)
    const size_t total = sizeof(hdr) + (size_t)hdr.capacity * hdr.recSize;
    histFile.seek(total - 1, SeekSet);
    histFile.write((const uint8_t*)"\0", 1);
    histFile.flush();

    Serial.print(F("[HIST] Created "));
    Serial.print(HISTORY_FILE_PATH);
    Serial.print(F(" capacity="));
    Serial.println(hdr.capacity);
    return true;
  }

  // Bestehende Datei öffnen
  histFile = LittleFS.open(HISTORY_FILE_PATH, "r+");
  if (!histFile)
  {
    Serial.println(F("[HIST] Open file failed"));
    return false;
  }

  // Header lesen/prüfen
  histFile.seek(0, SeekSet);
  if (histFile.read((uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr))
  {
    Serial.println(F("[HIST] Header read failed"));
    LittleFS.remove(HISTORY_FILE_PATH);
    return fileOpenOrCreate();
  }

  if (hdr.magic != HIST_MAGIC || hdr.version != HIST_VERSION || hdr.recSize != sizeof(LogSample))
  {
    Serial.println(F("[HIST] Header invalid → recreate"));
    LittleFS.remove(HISTORY_FILE_PATH);
    return fileOpenOrCreate();
  }

  if (hdr.capacity != HISTORY_CAPACITY_RECORDS)
  {
    Serial.println(F("[HIST] Capacity mismatch → recreate"));
    LittleFS.remove(HISTORY_FILE_PATH);
    return fileOpenOrCreate();
  }

  Serial.print(F("[HIST] Opened "));
  Serial.print(HISTORY_FILE_PATH);
  Serial.print(F(" capacity="));
  Serial.print(hdr.capacity);
  Serial.print(F(" head="));
  Serial.print(hdr.head);
  Serial.print(F(" count="));
  Serial.println(hdr.count);

  return true;
}

/***************** recOffset ****************************************************
 * params: idx
 * return: size_t
 * Description:
 * Byte-Offset eines Records in der Datei.
 ******************************************************************************/
static inline size_t recOffset(uint32_t idx)
{
  return sizeof(hdr) + (size_t)idx * hdr.recSize;
}

/***************** initHistory **************************************************
 * params: none
 * return: bool
 * Description:
 * Mountet FS und initialisiert/öffnet den Ringpuffer.
 ******************************************************************************/
bool initHistory()
{
  return fileOpenOrCreate();
}

/***************** appendHistory ************************************************
 * params: sample
 * return: bool
 * Description:
 * Schreibt einen Datensatz an hdr.head, inkrementiert Head/Count und persistiert.
 ******************************************************************************/
bool appendHistory(const LogSample& s)
{
  if (!histFile)
  {
    return false;
  }

  const size_t off = recOffset(hdr.head);
  histFile.seek(off, SeekSet);
  const size_t w = histFile.write((const uint8_t*)&s, sizeof(s));
  if (w != sizeof(s))
  {
    Serial.println(F("[HIST] Write sample failed"));
    return false;
  }

  // Head vorrücken
  hdr.head = (hdr.head + 1) % hdr.capacity;
  if (hdr.count < hdr.capacity)
  {
    hdr.count++;
  }

  // Header persistieren
  histFile.seek(0, SeekSet);
  histFile.write((const uint8_t*)&hdr, sizeof(hdr));
  histFile.flush();
  return true;
}

bool appendHistory(uint32_t tsSec, int16_t tempCenti, bool heaterOn)
{
  LogSample s;
  s.tsSec           = tsSec;
  s.tempCenti       = tempCenti;
  s.setPointCenti   = (int16_t)roundf(getSetPoint() * 100.0f);
  s.hysteresisCenti = (int16_t)roundf(getHysteresis() * 100.0f);
  s.flags           = heaterOn ? 0x01 : 0x00;
  s.rsv             = 0;
  return appendHistory(s);
}

/***************** readHistoryTail *********************************************
 * params: maxOut, outBuf, outCount
 * return: size_t
 * Description:
 * Liest die letzten maxOut Datensätze (neueste zuerst im Buffer-Index aufsteigend).
 ******************************************************************************/
size_t readHistoryTail(size_t maxOut, LogSample* outBuf, size_t* outCount)
{
  if (!histFile || outBuf == nullptr || maxOut == 0)
  {
    if (outCount) *outCount = 0;
    return 0;
  }

  const size_t n = (hdr.count < maxOut) ? hdr.count : maxOut;

  for (size_t i = 0; i < n; i++)
  {
    // Ältester Index innerhalb der letzten n Elemente:
    const uint32_t idx = (hdr.head + hdr.capacity - n + i) % hdr.capacity;
    histFile.seek(recOffset(idx), SeekSet);
    histFile.read((uint8_t*)&outBuf[i], sizeof(LogSample));
  }

  if (outCount) *outCount = n;
  return n;
}

/***************** handleHistory ************************************************
 * params: none
 * return: void
 * Description:
 * Scheduler für periodisches Logging. Nutzt LOG_INTERVAL_MINUTES aus config.h.
 ******************************************************************************/
void handleHistory()
{
  static unsigned long nextDue = 0;
  const unsigned long nowMs = millis();
  const unsigned long intervalMs = (unsigned long)LOG_INTERVAL_MINUTES * 60000UL;

  if (intervalMs == 0)
  {
    return; // Logging deaktiviert
  }

  if (nextDue == 0)
  {
    // Erste Planung (ab jetzt in festen Takten)
    nextDue = nowMs + intervalMs;
    return;
  }

  if ((long)(nowMs - nextDue) < 0)
  {
    return; // noch nicht fällig
  }

  // Sensor lesen und Sample bauen
  const float tC = getLastTemperature();
  if (!isnan(tC))
  {
    LogSample s;
    s.tsSec     = getEpochOrUptimeSec();
    s.tempCenti = (int16_t)roundf(tC * 100.0f);
    s.setPointCenti   = (int16_t)roundf(getSetPoint() * 100.0f);
    s.hysteresisCenti = (int16_t)roundf(getHysteresis() * 100.0f);
    s.flags           = (isHeaterOn() ? 0x01 : 0x00);
    s.rsv             = 0;

    if (!appendHistory(s))
    {
      Serial.println(F("[HIST] append failed"));
    }
    else
    {
      Serial.println(F("[HIST] append succeded"));
    }
  }

  // Nächsten Termin setzen (bei Verzögerung ggf. nachziehen)
  do
  {
    nextDue += intervalMs;
  }
  while ((long)(nowMs - nextDue) >= 0);
}
