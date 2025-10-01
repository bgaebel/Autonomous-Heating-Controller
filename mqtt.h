#ifndef MQTT_H
#define MQTT_H

/***************** initMqtt *****************************************************
 * Initializes MQTT client and sets up callback handler.
 ******************************************************************************/
void initMqtt();

/***************** mqttReconnectIfNeeded ****************************************
 * Ensures MQTT connection is alive, reconnects every 10s if disconnected.
 ******************************************************************************/
void mqttReconnectIfNeeded();

/***************** publishTelemetry ********************************************
 * Publishes current sensor data (temp, humidity, state) to MQTT broker.
 ******************************************************************************/
void publishTelemetry();

/***************** publishState *************************************************
 * Publishes current configuration and control state (setPoint, mode, etc.).
 ******************************************************************************/
void publishState();

#endif
