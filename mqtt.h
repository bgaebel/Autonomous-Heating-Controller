#ifndef MQTT_H
#define MQTT_H

/***************** initMqtt *****************************************************
 * Initializes MQTT client and sets up callback handler.
 ******************************************************************************/
void initMqtt();

/***************** ensureMQTT *************************************************
 * Ensures MQTT connection is alive, reconnects every 10s if disconnected.
 ******************************************************************************/
void ensureMQTT();

/***************** publishTelemetry ********************************************
 * Publishes current sensor data (temp, humidity, state) to MQTT broker.
 ******************************************************************************/
void publishTelemetry();

/***************** publishState *************************************************
 * Publishes current configuration and control state (setPoint, mode, etc.).
 ******************************************************************************/
void publishState();

/***************** mqttIsConnected *********************************************
 * params: none
 * return: bool
 * Description:
 * Returns whether the MQTT client is currently connected.
 ******************************************************************************/
bool mqttIsConnected();

/***************** mqttConnStateText ********************************************
 * params: none
 * return: const char*
 * Description:
 * Returns a short text for the last MQTT connection state
 * (e.g. "connected", "timeout", "unauthorized").
 ******************************************************************************/
const char* mqttConnStateText();


#endif
