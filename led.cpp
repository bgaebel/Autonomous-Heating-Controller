#include "led.h"

/***************** Internal State **********************************************/
static bool baseHeaterOn   = false;

static bool wifiOkFlag     = false;
static bool mqttOkFlag     = false;
static bool sensorOkFlag   = false;

static bool anyFaultCached = false;     // caches if at least one subsystem fails

// signature state machine
enum SigState
{
  SIG_IDLE,         // no faults -> no group
  SIG_WAIT,         // waiting LED_GROUP_PERIOD_MS to emit next group
  SIG_P1_ON,        // WiFi pulse ON (short/long)
  SIG_P1_GAP,       // inter-pulse gap
  SIG_P2_ON,        // MQTT pulse ON
  SIG_P2_GAP,
  SIG_P3_ON,        // Sensor pulse ON
  SIG_DONE          // group finished -> back to WAIT
};

static SigState      sigState     = SIG_IDLE;
static unsigned long tRef         = 0;

/***************** Helpers ******************************************************/
static inline void writeLedLogical(bool logicalOn)
{
#if LED_ACTIVE_LOW
  digitalWrite(LED_PIN, logicalOn ? LOW : HIGH);
#else
  digitalWrite(LED_PIN, logicalOn ? HIGH : LOW);
#endif
}

static inline bool anyFault()
{
  return !wifiOkFlag || !mqttOkFlag || !sensorOkFlag;
}

/***************** restartSignature *********************************************
 * params: none
 * return: void
 * Description:
 * Re-evaluates whether we should be idle or waiting for next group.
 ******************************************************************************/
static void restartSignature()
{
  const bool fault = anyFault();
  anyFaultCached = fault;

  if (!fault)
  {
    sigState = SIG_IDLE;
  }
  else
  {
    sigState = SIG_WAIT;
    tRef = millis();   // start waiting full period
  }
}

/***************** initLed ******************************************************
 * params: none
 * return: void
 * Description:
 * Initializes LED GPIO and sets default (base OFF, subsystems unknown=false).
 ******************************************************************************/
void initLed()
{
  pinMode(LED_PIN, OUTPUT);
  baseHeaterOn = false;

  // assume all unknown -> treated as NOT OK until modules report their status
  wifiOkFlag = false;
  mqttOkFlag = false;
  sensorOkFlag = false;

  restartSignature();
  writeLedLogical(false);
}

/***************** ledSetBaseFromHeater *****************************************
 * params: heaterOn
 * return: void
 * Description:
 * Mirrors relay state as LED base. The overlay pulses will invert it briefly.
 ******************************************************************************/
void ledSetBaseFromHeater(bool heaterOn)
{
  baseHeaterOn = heaterOn;
}

/***************** ledSetSubsystemStatus ****************************************/
void ledSetSubsystemStatus(bool wifiOk, bool mqttOk, bool sensorOk)
{
  const bool prevFault = anyFaultCached;

  wifiOkFlag   = wifiOk;
  mqttOkFlag   = mqttOk;
  sensorOkFlag = sensorOk;

  // If fault/ok changed, recompute signature scheduling.
  if (prevFault != anyFault())
  {
    restartSignature();
  }
}

/***************** Convenience setters ******************************************/
void ledSetWifiOk(bool ok)
{
  ledSetSubsystemStatus(ok, mqttOkFlag, sensorOkFlag);
}

void ledSetMqttOk(bool ok)
{
  ledSetSubsystemStatus(wifiOkFlag, ok, sensorOkFlag);
}

void ledSetSensorOk(bool ok)
{
  ledSetSubsystemStatus(wifiOkFlag, mqttOkFlag, ok);
}

/***************** handleLed ****************************************************/
void handleLed()
{
  const unsigned long now = millis();

  // recompute if fault presence toggled (safety net if someone changed flags rapidly)
  const bool faultNow = anyFault();
  if (faultNow != anyFaultCached)
  {
    restartSignature();
  }

  // default: no overlay
  bool overlayPulse = false;

  switch (sigState)
  {
    case SIG_IDLE:
    {
      // no faults -> nothing to show
      break;
    }

    case SIG_WAIT:
    {
      if ((now - tRef) >= LED_GROUP_PERIOD_MS)
      {
        // time to emit a new group (only if still faulty)
        if (anyFault())
        {
          sigState = SIG_P1_ON;
          tRef = now;
          overlayPulse = true; // start WiFi pulse ON now
        }
        else
        {
          // fault cleared while waiting
          sigState = SIG_IDLE;
        }
      }
      break;
    }

    case SIG_P1_ON:
    {
      // WiFi pulse duration: long if wifi FAIL, short if OK
      const unsigned long dur = wifiOkFlag ? LED_PULSE_SHORT_MS : LED_PULSE_LONG_MS;
      overlayPulse = true;

      if ((now - tRef) >= dur)
      {
        sigState = SIG_P1_GAP;
        tRef = now;
      }
      break;
    }

    case SIG_P1_GAP:
    {
      if ((now - tRef) >= LED_INTER_PULSE_GAP_MS)
      {
        sigState = SIG_P2_ON;
        tRef = now;
        overlayPulse = true; // MQTT starts
      }
      break;
    }

    case SIG_P2_ON:
    {
      const unsigned long dur = mqttOkFlag ? LED_PULSE_SHORT_MS : LED_PULSE_LONG_MS;
      overlayPulse = true;

      if ((now - tRef) >= dur)
      {
        sigState = SIG_P2_GAP;
        tRef = now;
      }
      break;
    }

    case SIG_P2_GAP:
    {
      if ((now - tRef) >= LED_INTER_PULSE_GAP_MS)
      {
        sigState = SIG_P3_ON;
        tRef = now;
        overlayPulse = true; // Sensor starts
      }
      break;
    }

    case SIG_P3_ON:
    {
      const unsigned long dur = sensorOkFlag ? LED_PULSE_SHORT_MS : LED_PULSE_LONG_MS;
      overlayPulse = true;

      if ((now - tRef) >= dur)
      {
        sigState = SIG_DONE;
        tRef = now;
      }
      break;
    }

    case SIG_DONE:
    {
      // group finished -> wait full period again
      sigState = SIG_WAIT;
      tRef = now;
      break;
    }
  }

  // Merge base (Heater) with overlay (invert during pulse)
  const bool finalLogicalOn = (baseHeaterOn ^ overlayPulse);
  writeLedLogical(finalLogicalOn);
}
