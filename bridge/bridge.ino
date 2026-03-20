/*
 * ESP32 WiFi Bridge for 6DOF Robot Arm
 *
 * Provides a REST API over WiFi that forwards commands to the Arduino
 * controller via UART. No changes required to the Arduino firmware.
 *
 * Dependencies (install via Arduino Library Manager):
 *   - ArduinoJson (v6 or v7)
 *
 * Board: ESP32 Dev Module (or your variant)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "secrets.h"

// --------------- Globals ---------------

WebServer server(HTTP_PORT);

int jointPositions[NUM_JOINTS] = {92, 85, 45, 108, 80, 152};
bool arduinoOnline = false;
unsigned long lastStatusMs = 0;
unsigned long bootMs = 0;

// --------------- Forward declarations ---------------

void setupWiFi();
void setupRoutes();
bool authenticate();
String sendArm(const char* cmd, unsigned long timeoutMs = CMD_TIMEOUT_MS);
void collectArmResponse(String lines[], int& count, unsigned long timeoutMs);
bool parsePositions(const String& line);
void jsonPositions(JsonObject& obj);
void sendJson(int code, JsonDocument& doc);
void sendError(int code, const char* msg);
void sendOk(const char* msg);

// --------------- Setup / Loop ---------------

void setup() {
  Serial.begin(115200);
  ARM_SERIAL.begin(ARM_BAUD, SERIAL_8N1, ARM_RX_PIN, ARM_TX_PIN);

  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
  esp_task_wdt_add(NULL);

  setupWiFi();

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("mDNS: http://%s.local\n", MDNS_HOSTNAME);
  }

  setupRoutes();
  server.begin();
  bootMs = millis();

  // Wait for Arduino boot message
  delay(2000);
  while (ARM_SERIAL.available()) ARM_SERIAL.read();

  // Initial status probe
  String resp = sendArm("STATUS");
  arduinoOnline = resp.startsWith("OK:");
  if (arduinoOnline) parsePositions(resp);

  Serial.println("Bridge ready");
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();

  // Periodic health check every 5 seconds
  if (millis() - lastStatusMs > 5000) {
    lastStatusMs = millis();
    String resp = sendArm("STATUS", 1000);
    arduinoOnline = resp.startsWith("OK:");
    if (arduinoOnline) parsePositions(resp);
  }

  // WiFi reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    setupWiFi();
  }
}

// --------------- WiFi ---------------

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s", WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi connection failed");
  }
}

// --------------- Authentication ---------------

bool authenticate() {
  if (strlen(API_KEY) == 0) return true;
  if (!server.hasHeader("X-API-Key")) {
    sendError(401, "Missing X-API-Key header");
    return false;
  }
  if (server.header("X-API-Key") != String(API_KEY)) {
    sendError(403, "Invalid API key");
    return false;
  }
  return true;
}

// --------------- ARM Serial Communication ---------------

String sendArm(const char* cmd, unsigned long timeoutMs) {
  // Drain any pending data
  while (ARM_SERIAL.available()) ARM_SERIAL.read();

  ARM_SERIAL.println(cmd);

  unsigned long start = millis();
  String line = "";
  while (millis() - start < timeoutMs) {
    if (ARM_SERIAL.available()) {
      char c = ARM_SERIAL.read();
      if (c == '\n' || c == '\r') {
        line.trim();
        if (line.length() > 0) return line;
        line = "";
      } else {
        line += c;
      }
    }
    esp_task_wdt_reset();
    yield();
  }
  line.trim();
  return line;
}

void collectArmResponse(String lines[], int& count, unsigned long timeoutMs) {
  count = 0;
  unsigned long start = millis();
  unsigned long lastChar = start;
  String line = "";

  while (millis() - start < timeoutMs && count < MAX_RESPONSE_LINES) {
    if (ARM_SERIAL.available()) {
      char c = ARM_SERIAL.read();
      lastChar = millis();
      if (c == '\n' || c == '\r') {
        line.trim();
        if (line.length() > 0) {
          lines[count++] = line;
          line = "";
        }
      } else {
        line += c;
      }
    } else {
      // If we already got at least one line and no data for 200ms, stop
      if (count > 0 && millis() - lastChar > 200) break;
    }
    esp_task_wdt_reset();
    yield();
  }
  line.trim();
  if (line.length() > 0 && count < MAX_RESPONSE_LINES) {
    lines[count++] = line;
  }
}

bool parsePositions(const String& line) {
  if (!line.startsWith("OK:")) return false;
  String payload = line.substring(3);
  int start = 0;
  while (start < (int)payload.length()) {
    int comma = payload.indexOf(',', start);
    if (comma == -1) comma = payload.length();
    String pair = payload.substring(start, comma);
    int colon = pair.indexOf(':');
    if (colon > 0 && pair.charAt(0) == 'J') {
      int idx = pair.substring(1, colon).toInt() - 1;
      int angle = pair.substring(colon + 1).toInt();
      if (idx >= 0 && idx < NUM_JOINTS) {
        jointPositions[idx] = angle;
      }
    }
    start = comma + 1;
  }
  return true;
}

// --------------- JSON Helpers ---------------

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
}

void sendJson(int code, JsonDocument& doc) {
  addCorsHeaders();
  String body;
  serializeJson(doc, body);
  server.send(code, "application/json", body);
}

void sendError(int code, const char* msg) {
  JsonDocument doc;
  doc["error"] = msg;
  sendJson(code, doc);
}

void sendOk(const char* msg) {
  JsonDocument doc;
  doc["status"] = "ok";
  doc["message"] = msg;
  sendJson(200, doc);
}

void jsonPositions(JsonObject obj) {
  for (int i = 0; i < NUM_JOINTS; i++) {
    String key = "J" + String(i + 1);
    obj[key] = jointPositions[i];
  }
}

// --------------- API Routes ---------------

void setupRoutes() {
  server.collectHeaders("X-API-Key");

  // CORS preflight
  server.on("/api/*", HTTP_OPTIONS, []() {
    addCorsHeaders();
    server.send(204);
  });

  // ---- Info & Health ----

  server.on("/api/health", HTTP_GET, []() {
    JsonDocument doc;
    doc["arduino"] = arduinoOnline ? "online" : "offline";
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["uptime_sec"] = (millis() - bootMs) / 1000;
    sendJson(200, doc);
  });

  server.on("/api/info", HTTP_GET, []() {
    JsonDocument doc;
    doc["firmware"] = "6DOF-Bridge";
    doc["version"] = "1.0.0";
    doc["ip"] = WiFi.localIP().toString();
    doc["hostname"] = MDNS_HOSTNAME;
    doc["arduino"] = arduinoOnline ? "online" : "offline";
    doc["uptime_sec"] = (millis() - bootMs) / 1000;
    doc["wifi_ssid"] = WIFI_SSID;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap();
    sendJson(200, doc);
  });

  // ---- Status ----

  server.on("/api/status", HTTP_GET, []() {
    if (!authenticate()) return;

    String resp = sendArm("STATUS", 2000);
    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonDocument doc;
      doc["status"] = "ok";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else {
      sendError(502, "Arduino not responding");
    }
  });

  // ---- Joint Control (Jog) ----

  server.on("/api/joint", HTTP_POST, []() {
    if (!authenticate()) return;

    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      sendError(400, "Invalid JSON");
      return;
    }

    int joint = body["joint"] | 0;
    int angle = body["angle"] | -1;
    if (joint < 1 || joint > NUM_JOINTS || angle < 0) {
      sendError(400, "Required: joint (1-6), angle (0-180)");
      return;
    }

    char cmd[16];
    snprintf(cmd, sizeof(cmd), "J%d:%d", joint, angle);
    String resp = sendArm(cmd, CMD_LONG_TIMEOUT_MS);

    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonDocument doc;
      doc["status"] = "ok";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else if (resp.startsWith("ERROR:")) {
      sendError(400, resp.substring(6).c_str());
    } else {
      sendError(504, "Timeout waiting for arm");
    }
  });

  server.on("/api/joints", HTTP_POST, []() {
    if (!authenticate()) return;

    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      sendError(400, "Invalid JSON");
      return;
    }

    JsonObject joints = body["joints"];
    if (joints.isNull() || joints.size() == 0) {
      sendError(400, "Required: joints object {\"J1\": 90, ...}");
      return;
    }

    String lastResp;
    for (JsonPair kv : joints) {
      const char* key = kv.key().c_str();
      int angle = kv.value().as<int>();
      char cmd[16];
      snprintf(cmd, sizeof(cmd), "%s:%d", key, angle);
      lastResp = sendArm(cmd, CMD_LONG_TIMEOUT_MS);
      if (lastResp.startsWith("ERROR:")) {
        sendError(400, lastResp.substring(6).c_str());
        return;
      }
      if (!lastResp.startsWith("OK:")) {
        sendError(504, "Timeout waiting for arm");
        return;
      }
    }

    parsePositions(lastResp);

    JsonDocument doc;
    doc["status"] = "ok";
    JsonObject pos = doc["positions"].to<JsonObject>();
    jsonPositions(pos);
    sendJson(200, doc);
  });

  // ---- Presets ----

  server.on("/api/home", HTTP_POST, []() {
    if (!authenticate()) return;
    String resp = sendArm("HOME", CMD_LONG_TIMEOUT_MS);
    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonDocument doc;
      doc["status"] = "ok";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else {
      sendError(502, "Failed");
    }
  });

  server.on("/api/stop", HTTP_POST, []() {
    // Emergency stop: skip auth for safety
    String resp = sendArm("STOP", 2000);
    JsonDocument doc;
    doc["status"] = "stopped";
    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
    }
    sendJson(200, doc);
  });

  server.on("/api/preset/min", HTTP_POST, []() {
    if (!authenticate()) return;
    String resp = sendArm("MIN", CMD_LONG_TIMEOUT_MS);
    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonDocument doc;
      doc["status"] = "ok";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else {
      sendError(502, "Failed");
    }
  });

  server.on("/api/preset/max", HTTP_POST, []() {
    if (!authenticate()) return;
    String resp = sendArm("MAX", CMD_LONG_TIMEOUT_MS);
    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonDocument doc;
      doc["status"] = "ok";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else {
      sendError(502, "Failed");
    }
  });

  server.on("/api/preset/wave", HTTP_POST, []() {
    if (!authenticate()) return;
    String resp = sendArm("WAVE", CMD_LONG_TIMEOUT_MS);
    if (resp.startsWith("OK:")) {
      parsePositions(resp);
      JsonDocument doc;
      doc["status"] = "ok";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else {
      sendError(502, "Failed");
    }
  });

  // ---- Speed ----

  server.on("/api/speed", HTTP_POST, []() {
    if (!authenticate()) return;

    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      sendError(400, "Invalid JSON");
      return;
    }

    int speed = body["speed_ms"] | -1;
    if (speed < 5 || speed > 200) {
      sendError(400, "speed_ms must be 5-200");
      return;
    }

    char cmd[24];
    snprintf(cmd, sizeof(cmd), "SET_SPEED:%d", speed);
    String resp = sendArm(cmd);

    if (resp.startsWith("OK:")) {
      JsonDocument doc;
      doc["status"] = "ok";
      doc["speed_ms"] = speed;
      sendJson(200, doc);
    } else if (resp.startsWith("ERROR:")) {
      sendError(400, resp.substring(6).c_str());
    } else {
      sendError(504, "Timeout");
    }
  });

  // ---- Program (Sequence) Management ----

  server.on("/api/program/list", HTTP_GET, []() {
    if (!authenticate()) return;

    while (ARM_SERIAL.available()) ARM_SERIAL.read();
    ARM_SERIAL.println("LIST_SEQUENCES");

    String lines[MAX_RESPONSE_LINES];
    int count = 0;
    collectArmResponse(lines, count, 2000);

    JsonDocument doc;
    doc["status"] = "ok";
    JsonArray programs = doc["programs"].to<JsonArray>();

    for (int i = 0; i < count; i++) {
      // Skip header line
      if (lines[i].startsWith("SEQUENCE:")) continue;

      int colon = lines[i].indexOf(':');
      if (colon > 0 && lines[i].substring(0, colon).toInt() >= 0) {
        JsonObject p = programs.add<JsonObject>();
        p["slot"] = lines[i].substring(0, colon).toInt();
        p["name"] = lines[i].substring(colon + 1);
      }
    }

    sendJson(200, doc);
  });

  server.on("/api/program/record", HTTP_POST, []() {
    if (!authenticate()) return;

    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      sendError(400, "Invalid JSON");
      return;
    }

    int slot = body["slot"] | -1;
    const char* name = body["name"] | (const char*)nullptr;
    if (slot < 0 || !name || strlen(name) == 0) {
      sendError(400, "Required: slot (0-1), name (string)");
      return;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "RECORD_START:%d:%s", slot, name);
    String resp = sendArm(cmd);

    if (resp.startsWith("OK:")) {
      sendOk("Recording started");
    } else if (resp.startsWith("ERROR:")) {
      sendError(400, resp.substring(6).c_str());
    } else {
      sendError(504, "Timeout");
    }
  });

  server.on("/api/program/stop", HTTP_POST, []() {
    if (!authenticate()) return;
    String resp = sendArm("RECORD_STOP");
    if (resp.startsWith("OK:")) {
      sendOk("Recording stopped");
    } else if (resp.startsWith("ERROR:")) {
      sendError(400, resp.substring(6).c_str());
    } else {
      sendError(504, "Timeout");
    }
  });

  server.on("/api/program/run", HTTP_POST, []() {
    if (!authenticate()) return;

    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      sendError(400, "Invalid JSON");
      return;
    }

    int slot = body["slot"] | -1;
    if (slot < 0) {
      sendError(400, "Required: slot (0-1)");
      return;
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "PLAY_SEQUENCE:%d", slot);
    String resp = sendArm(cmd, CMD_LONG_TIMEOUT_MS);

    if (resp.startsWith("OK:")) {
      // After sequence, get final positions
      String statusResp = sendArm("STATUS", 2000);
      if (statusResp.startsWith("OK:")) parsePositions(statusResp);

      JsonDocument doc;
      doc["status"] = "ok";
      doc["message"] = "Sequence completed";
      JsonObject pos = doc["positions"].to<JsonObject>();
      jsonPositions(pos);
      sendJson(200, doc);
    } else if (resp.startsWith("ERROR:")) {
      sendError(400, resp.substring(6).c_str());
    } else {
      sendError(504, "Timeout during playback");
    }
  });

  server.on("/api/program/delete", HTTP_POST, []() {
    if (!authenticate()) return;

    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      sendError(400, "Invalid JSON");
      return;
    }

    int slot = body["slot"] | -1;
    if (slot < 0) {
      sendError(400, "Required: slot (0-1)");
      return;
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "DELETE_SEQUENCE:%d", slot);
    String resp = sendArm(cmd);

    if (resp.startsWith("OK:")) {
      sendOk("Program deleted");
    } else if (resp.startsWith("ERROR:")) {
      sendError(400, resp.substring(6).c_str());
    } else {
      sendError(504, "Timeout");
    }
  });

  // ---- Catch-all ----

  server.onNotFound([]() {
    addCorsHeaders();
    if (server.method() == HTTP_OPTIONS) {
      server.send(204);
      return;
    }
    sendError(404, "Not found");
  });
}
