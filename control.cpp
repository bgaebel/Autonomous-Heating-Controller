#include "control.h"
#include "config.h"
#include "sensor.h"
#include "led.h"

/***************** Local State **************************************************/
static ControlMode  activeMode = MODE_AUTO;  // internal active mode
static ControlMode  prevMode   = (ControlMode)(-1); // to detect on-entry
static ControlState controlState = STATE_IDLE;
static bool         heaterOn   = false;      // physical output cache

/***************** setHeater ****************************************************
 * params: on
 * return: void
 * Description:
 * Writes the relay only if the requested state differs from the cached state.
 ******************************************************************************/
static void setHeater(bool on)
{
  if (heaterOn == on)
  {
    return;
  }
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  heaterOn = on;

  // >>> LED-Basis aktualisieren:
  ledSetBaseFromHeater(on);
}

/***************** isHeaterOn ***************************************************
 * params: none
 * return: bool
 * Description:
 * Returns current physical heater output state.
 ******************************************************************************/
bool isHeaterOn()
{
  return heaterOn;
}

/***************** initControl **************************************************
 * params: none
 * return: void
 * Description:
 * Initializes heater output and internal state machine data.
 ******************************************************************************/
void initControl()
{
  pinMode(RELAY_PIN, OUTPUT);
  setHeater(false);
  controlState = STATE_IDLE;

  // Pick up the persisted request as initial active mode.
  activeMode = getControlMode();
  prevMode   = (ControlMode)(-1); // force on-entry in first handleControl()
}

/***************** handleControl ************************************************
 * params: none
 * return: void
 * Description:
 * Non-blocking control state machine. All mode behavior (AUTO/OFF/BOOST)
 * runs INSIDE the switch-case. Transitions toggle outputs only once (on-entry),
 * and AUTO uses hysteresis with edge detection (no periodic re-writes).
 ******************************************************************************/
void handleControl()
{
  static unsigned long lastEval = 0;
  const unsigned long now = millis();

  // Evaluate at ~2s cadence to limit IO and jitter
  if (now - lastEval < 2000)
  {
    return;
  }
  lastEval = now;

  // The requested (persisted) mode can change at any time (web/UI)
  const ControlMode requestedMode = getControlMode();
  const bool modeChangeRequested  = (requestedMode != activeMode);

  // Read sensor once per evaluation; use same value across cases
  const float temp = getLastTemperature();
  const bool  sensorOk = !isnan(temp);

  // Keep setpoint/hysteresis stable within one cycle
  const float setp = getSetPoint();
  const float hyst = getHysteresis();

  // ===== Single State Machine: everything handled per-mode inside switch =====
  switch (activeMode)
  {
    case MODE_OFF:
    {
      // --- On-Entry ---
      if (prevMode != MODE_OFF)
      {
        setHeater(false);
        controlState = STATE_IDLE;
        prevMode = MODE_OFF;
      }

      // --- Error Handling (in-mode) ---
      if (!sensorOk)
      {
        // Fail-safe: even in OFF we reflect the error state (heater remains off)
        setHeater(false);
        controlState = STATE_ERROR;
        break;
      }
      if (controlState == STATE_ERROR)
      {
        // Recover diagnostic state (heater is already OFF)
        controlState = STATE_IDLE;
      }

      // --- Transitions ---
      if (modeChangeRequested)
      {
        // Leave OFF → enter requested
        activeMode = requestedMode;
        prevMode   = (ControlMode)(-1);
      }
      // No cyclic output writes in steady state.
      break;
    }

    case MODE_BOOST:
    {
      // --- On-Entry ---
      if (prevMode != MODE_BOOST)
      {
        // Set boost end only once on entry
        unsigned long end = getBoostEndTime();
        if (end == 0)
        {
          end = now + (unsigned long)getBoostMinutes() * 60000UL;
          setBoostEndTime(end);
        }
        setHeater(true);
        controlState = STATE_HEATING;
        prevMode = MODE_BOOST;
      }

      // --- Error Handling (in-mode) ---
      if (!sensorOk)
      {
        // Fail-safe OFF, aber BOOST-Ende bleibt gespeichert
        setHeater(false);
        controlState = STATE_ERROR;
        break;
      }
      if (controlState == STATE_ERROR)
      {
        // Sensor wieder ok → BOOST erzwingt HEATING, aber nur auf Transition:
        setHeater(true);
        controlState = STATE_HEATING;
      }

      // --- BOOST Timeout / Transitions ---
      const unsigned long end = getBoostEndTime();
      if (end != 0 && now >= end)
      {
        // Endzeit erreicht → zurück nach AUTO (persistiert)
        setBoostEndTime(0);
        setControlMode(MODE_AUTO);
        activeMode = MODE_AUTO;
        prevMode   = (ControlMode)(-1);
        break;
      }

      if (modeChangeRequested)
      {
        // User bricht BOOST ab → sauberes Verlassen
        setBoostEndTime(0);
        activeMode = requestedMode;
        prevMode   = (ControlMode)(-1);
      }
      // Steady: nichts weiter; kein periodisches setHeater(true).
      break;
    }

    case MODE_AUTO:
    {
      // --- On-Entry ---
      if (prevMode != MODE_AUTO)
      {
        // kein sofortiger Output-Wechsel; Hysterese entscheidet
        // (aktueller controlState bleibt bestehen)
        prevMode = MODE_AUTO;
      }

      // --- Error Handling (in-mode) ---
      if (!sensorOk)
      {
        setHeater(false);
        controlState = STATE_ERROR;
        break;
      }
      if (controlState == STATE_ERROR)
      {
        // Nach Fehler in den bisherigen physischen Zustand zurückkehren
        controlState = heaterOn ? STATE_HEATING : STATE_IDLE;
      }

      // --- Transitions ---
      if (modeChangeRequested)
      {
        activeMode = requestedMode;
        prevMode   = (ControlMode)(-1);
        break;
      }

      // --- AUTO Regelung (nur auf Flankenwechsel) ---
      if (controlState != STATE_HEATING && temp <= setp - hyst)
      {
        setHeater(true);
        controlState = STATE_HEATING;
      }
      else if (controlState != STATE_IDLE && temp >= setp + hyst)
      {
        setHeater(false);
        controlState = STATE_IDLE;
      }
      break;
    }
    default:
      activeMode = MODE_OFF;
      break;
  }
}

/***************** getControlState **********************************************/
ControlState getControlState()
{
  return controlState;
}

/***************** getControlMode ***********************************************/
ControlMode getControlMode()
{
  return activeMode;
}

/***************** setControlMode ***********************************************
 * params: m
 * return: void
 * Description:
 * Sets control mode and persists.
 ******************************************************************************/
void setControlMode(ControlMode m)
{
  activeMode = m;
  saveConfig();
}

/***************** setControlMode ***********************************************
 * params: m
 * return: void
 * Description:
 * Sets control mode and persists.
 ******************************************************************************/
void setControlMode(ControlState s)
{
  controlState = s;
}

/***************** modeToStr ****************************************************
 * params: m
 * return: const char*
 * Description:
 * Convert mode to readable string.
 ******************************************************************************/
const char* modeToStr(ControlMode m)
{
  switch (m)
  {
    case MODE_AUTO:   return "AUTO";
    case MODE_OFF:    return "OFF";
    case MODE_BOOST:  return "BOOST";
    default:          return "?";
  }
}

/***************** stateToStr ***************************************************
 * params: s
 * return: const char*
 * Description:
 * Convert internal control state to readable string.
 ******************************************************************************/
const char* stateToStr(ControlState s)
{
  switch (s)
  {
    case STATE_IDLE:    return "IDLE";
    case STATE_HEATING: return "HEATING";
    case STATE_ERROR:   return "ERROR";
    default:            return "?";
  }
}
