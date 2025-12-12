#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>

/***************** Enums ********************************************************
 * params: none
 * return: n/a
 * Description:
 * ControlMode: requested operation mode of controller
 * ControlState: physical heating state
 ******************************************************************************/
enum ControlMode
{
  MODE_AUTO = 0,
  MODE_OFF,
  MODE_BOOST
};

enum ControlState
{
  STATE_IDLE = 0,
  STATE_HEATING,
  STATE_ERROR
};

/***************** API **********************************************************/
void initControl();
void handleControl();
bool isHeaterOn();
void setHeater(bool on);

/***************** requestHeaterOffNow ******************************************
 * params: none
 * return: void
 * Description:
 * Forces the heater output OFF immediately while keeping the control logic active.
 ******************************************************************************/
void requestHeaterOffNow();

ControlState getControlState();
ControlMode getControlMode();
void setControlMode(ControlMode m);

const char* modeToStr(ControlMode m);
const char* stateToStr(ControlState s);

#endif // CONTROL_H
