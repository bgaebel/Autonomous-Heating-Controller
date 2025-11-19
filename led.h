#ifndef LED_H
#define LED_H

#include <Arduino.h>

// LED zeigt nur den Heizungsstatus an (AN/AUS)

#ifndef LED_PIN
#define LED_PIN LED_BUILTIN
#endif

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 1   // 1 = active-low (ESP Onboard-LED), 0 = active-high
#endif

/***************** API **********************************************************/

// Initializes LED GPIO and sets default (OFF)
void initLed();

// Mirrors the heater state directly to the LED
void ledSetBaseFromHeater(bool heaterOn);

// Non-blocking LED driver. Call frequently in loop().
void handleLed();

#endif
