#include "control.h"
#include "config.h"
#include "sensor.h"
#include "led.h"

static ControlState currentState = STATE_IDLE;
static unsigned long boostEndTime = 0;

/***************** initControl *************************************************
 * Description:
 * Initializes heater pin and state machine.
 ******************************************************************************/
void initControl()
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  currentState = STATE_IDLE;
  Serial.println(F("[CONTROL] Initialized. Heater OFF."));
}

/***************** setHeater ****************************************************
 * Description:
 * Turns the heater ON/OFF physically.
 ******************************************************************************/
void setHeater(bool on)
{
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  Serial.printf("[CONTROL] Heater %s\n", on ? "ON" : "OFF");
}

/***************** getControlState **********************************************/
ControlState getControlState()
{
  return currentState;
}

/***************** setControlState **********************************************/
void setControlState(ControlState s)
{
  currentState = s;
}

/***************** getControlMode **********************************************/
ControlMode getControlMode()
{
  return config.mode;
}

/***************** setControlMode **********************************************/
void setControlMode(ControlMode m)
{
  config.mode = m;
  saveConfig();
}

/***************** handleControl **************************************************
 * Description:
 * Periodically evaluates temperature and updates heating state.
 * Works in AUTO, MANUAL, BOOST, OFF modes.
 ******************************************************************************/
void handleControl()
{
  static unsigned long lastEval = 0;
  unsigned long now = millis();

  if (now - lastEval < 2000)
    return; // evaluate every 2s
  lastEval = now;

  float temp = getLastTemperature();
  if (isnan(temp))
  {
    Serial.println(F("[CONTROL] Sensor invalid! Switching to ERROR."));
    setHeater(false);
    currentState = STATE_ERROR;
    setLedState(LED_ERROR);
    return;
  }

  switch (config.mode)
  {
    case MODE_OFF:
      setHeater(false);
      currentState = STATE_IDLE;
      break;

    case MODE_MANUAL:
      setHeater(true);
      currentState = STATE_HEATING;
      break;

    case MODE_BOOST:
      if (boostEndTime == 0)
      {
        boostEndTime = now + (unsigned long)config.boostMinutes * 60000UL;
        Serial.printf("[CONTROL] BOOST started for %d min\n", config.boostMinutes);
      }
      if (now < boostEndTime)
      {
        setHeater(true);
        currentState = STATE_HEATING;
      }
      else
      {
        config.mode = MODE_AUTO;
        boostEndTime = 0;
        Serial.println(F("[CONTROL] BOOST finished. Switching to AUTO."));
      }
      break;

    case MODE_AUTO:
    default:
      if (temp < config.setPoint - config.hysteresis)
      {
        setHeater(true);
        currentState = STATE_HEATING;
      }
      else if (temp > config.setPoint + config.hysteresis)
      {
        setHeater(false);
        currentState = STATE_IDLE;
      }
      break;
  }

  Serial.printf("[CONTROL] Mode: %s | Temp: %.2f | State: %s\n",
                modeToStr(config.mode), temp, stateToStr(currentState));
}

/***************** modeToStr ****************************************************/
const char* modeToStr(ControlMode m)
{
  switch (m)
  {
    case MODE_AUTO: return "AUTO";
    case MODE_MANUAL: return "MANUAL";
    case MODE_OFF: return "OFF";
    case MODE_BOOST: return "BOOST";
    default: return "UNKNOWN";
  }
}

/***************** stateToStr ***************************************************/
const char* stateToStr(ControlState s)
{
  switch (s)
  {
    case STATE_IDLE: return "IDLE";
    case STATE_HEATING: return "HEATING";
    case STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}
