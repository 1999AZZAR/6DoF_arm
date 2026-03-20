/*
 * ESP32 Bridge Configuration
 * REST API gateway between WiFi clients and the Arduino 6DOF arm controller.
 *
 * Wiring (ESP32 <-> Arduino):
 *   ESP32 GPIO16 (RX2)  -->  Arduino TX (pin 1)
 *   ESP32 GPIO17 (TX2)  -->  Arduino RX (pin 0)
 *   ESP32 GND           -->  Arduino GND
 *
 * Note: disconnect Arduino USB before powering via ESP32 UART,
 * since both share the same Serial port on Uno.
 * Arduino Mega users can use Serial1 instead to keep USB available.
 */

#ifndef BRIDGE_CONFIG_H
#define BRIDGE_CONFIG_H

// UART to Arduino
#define ARM_SERIAL        Serial2
#define ARM_BAUD          115200
#define ARM_RX_PIN        16
#define ARM_TX_PIN        17

// HTTP server
#define HTTP_PORT         80

// mDNS hostname (reachable as http://robot-arm.local)
#define MDNS_HOSTNAME     "robot-arm"

// Serial command timeout (ms) - how long to wait for Arduino response
#define CMD_TIMEOUT_MS    5000

// Long command timeout for blocking moves (HOME, WAVE, sequences)
#define CMD_LONG_TIMEOUT_MS 15000

// Maximum response lines to collect from Arduino per command
#define MAX_RESPONSE_LINES 10

// WiFi reconnect interval (ms)
#define WIFI_RECONNECT_MS 5000

// Watchdog timeout (seconds)
#define WDT_TIMEOUT_SEC   30

// Joint configuration (must match Arduino config.h)
#define NUM_JOINTS        6

#endif // BRIDGE_CONFIG_H
