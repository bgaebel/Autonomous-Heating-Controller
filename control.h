#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>

/***************** Enums ********************************************************
 * ControlMode: operation mode of controller
 * ControlState: actual heating state
 ******************************************************************************/
enum ControlMode
{
  MODE_AUTO,
  MODE_MANUAL,
  MODE_OFF,
  MODE_BOOST
};

enum ControlState
{
  STATE_IDLE,
  STATE_HEATING,
  STATE_ERROR
};

/***************** Global Accessors ********************************************/
ControlState getControlState();
void setControlState(ControlState s);

ControlMode getControlMode();
void setControlMode(ControlMode m);

/***************** Function Declarations ***************************************/
void initControl();
void handleControl();
const char* modeToStr(ControlMode m);
const char* stateToStr(ControlState s);

#endif
