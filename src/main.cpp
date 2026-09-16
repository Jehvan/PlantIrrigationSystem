#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// ---------------- Config ----------------
const char* ssid     = "NULL";
const char* password = "NULL";

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

// --- Feature switches ---
// 0 = light sensor disabled, 1 = enabled
#define ENABLE_LIGHT_SENSOR 1

// --- Pinout ---
// Board: ESP32-S3
#define ADC_SOIL_PIN 4      // analog soil moisture
#define LDR_PIN      6      // DIGITAL LDR module DO pin
#define PUMP_PIN     5

// --- Relay polarity ---
// 0 = active-HIGH relay (HIGH = pump ON)   <-- yours
// 1 = active-LOW  relay (LOW  = pump ON)
#define PUMP_ACTIVE_LOW 0

// --- Digital LDR polarity ---
// 1 = DO goes HIGH when BRIGHT (typical LM393 module)
// 0 = DO goes LOW  when BRIGHT (some modules are inverted)
#define LDR_BRIGHT_IS_HIGH 0

// If your module has an open-collector DO output and reads unstable,
// set this to 1 to enable the internal pull-up on the pin.
#define LDR_USE_PULLUP 0

// --- Timing ---
#define SENSOR_READ_INTERVAL     1000UL
#define PUMP_RUN_TIME            5000UL
#define PUMP_COOLDOWN            30000UL
#define PUMP_MAX_ON_TIME         60000UL

// --- Thresholds ---
#define SOIL_MOISTURE_THRESHOLD  28        // %

WebServer server(80);

// ---------------- State ----------------
bool autoMode    = true;
bool pumpState   = false;
bool pumpTimed   = false;

unsigned long pumpStartTime  = 0;
unsigned long lastPumpStop   = 0;
unsigned long lastSensorRead = 0;
unsigned long pumpOnSince    = 0;

int  moistureValue = 0;
int  lightDigital  = 0;     // raw digital reading 0 or 1
bool lightIsBright = false; // interpreted state

// ---------------- Pump helpers ----------------
void setPump(bool on) {
    pumpState = on;
    bool level = PUMP_ACTIVE_LOW ? !on : on;
    digitalWrite(PUMP_PIN, level ? HIGH : LOW);
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
    Serial.printf("Pump -> %s\n", on ? "ON" : "OFF");
}

void startPumpCycle() {
    setPump(true);
    pumpTimed     = true;
    pumpStartTime = millis();
    pumpOnSince   = millis();
}

void stopPump(const char* reason) {
    if (!pumpState) return;
    pumpTimed    = false;
    lastPumpStop = millis();
    setPump(false);
    if (reason) Serial.printf("Pump stopped: %s\n", reason);
}

// ---------------- WiFi ----------------
void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid, password);

    Serial.printf("Connecting to \"%s\" ...\n", ssid);
    unsigned long start = millis();
    int lastStatus = -1;
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(500);
        Serial.print(".");
        int s = WiFi.status();
        if (s != lastStatus) {
            Serial.printf(" [status=%d]", s);
            lastStatus = s;
        }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected! IP: ");
        Serial.println(WiFi.localIP());
        return;
    }

    Serial.printf("FAILED after 20s. Last status = %d\n", WiFi.status());
    switch (WiFi.status()) {
        case WL_NO_SSID_AVAIL:  Serial.println("-> SSID not found."); break;
        case WL_CONNECT_FAILED: Serial.println("-> Wrong password, OR WPA3-only router."); break;
        case WL_DISCONNECTED:   Serial.println("-> Assoc dropped. Wrong password / WPA3 / MAC filter."); break;
        default:                Serial.println("-> Unknown. Check router.");
    }
    Serial.print("ESP32 MAC: ");
    Serial.println(WiFi.macAddress());
}

// ---------------- File serving ----------------
void listAllFiles() {
    Serial.println("\n--- Listing all files in LittleFS ---");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.print("FILE: ");
        Serial.println(file.name());
        file = root.openNextFile();
    }
    Serial.println("-----------------------------------\n");
}

void handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        server.send(404, "text/plain", "index.html not found in LittleFS");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

// ---------------- JSON API ----------------
void handleSensorData() {
    String json = "{";
    json += "\"moisture\":" + String(moistureValue) + ",";

#if ENABLE_LIGHT_SENSOR
    // Send 100 or 0 so the dashboard's % display still works unchanged.
    json += "\"light\":"      + String(lightIsBright ? 100 : 0) + ",";
    json += "\"lightState\":\"" + String(lightIsBright ? "BRIGHT" : "DARK") + "\",";
    json += "\"lightRaw\":"   + String(lightDigital) + ",";
#else
    json += "\"light\":\"--\",";
    json += "\"lightState\":\"--\",";
    json += "\"lightRaw\":\"--\",";
#endif

    json += "\"pump\":\""   + String(pumpState ? "ON" : "OFF") + "\",";
    json += "\"mode\":\""   + String(autoMode  ? "AUTO" : "MANUAL") + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleTogglePump() {
    autoMode = false;

    if (pumpState) {
        stopPump("manual toggle");
    } else {
        pumpTimed   = false;
        pumpOnSince = millis();
        setPump(true);
    }

    String json = "{\"status\":\"" + String(pumpState ? "ON" : "OFF") +
                  "\",\"mode\":\"MANUAL\"}";
    server.send(200, "application/json", json);
}

void handleAutoMode() {
    autoMode = true;
    if (pumpTimed) stopPump("return to auto");
    lastPumpStop = millis();
    server.send(200, "application/json", "{\"mode\":\"AUTO\"}");
}

// ---------------- Sensors ----------------
void readSensors() {
    int rawSoil = analogRead(ADC_SOIL_PIN);
    moistureValue = constrain(map(rawSoil, 4095, 0, 0, 100), 0, 100);

#if ENABLE_LIGHT_SENSOR
    lightDigital = digitalRead(LDR_PIN);
    lightIsBright = LDR_BRIGHT_IS_HIGH ? (lightDigital == HIGH)
                                       : (lightDigital == LOW);

    Serial.printf("Soil raw=%4d  moisture=%3d%%  |  LDR DO=%d (%s)  |  pump=%s  mode=%s\n",
                  rawSoil, moistureValue,
                  lightDigital, lightIsBright ? "BRIGHT" : "DARK",
                  pumpState ? "ON" : "OFF", autoMode ? "AUTO" : "MANUAL");
#else
    Serial.printf("Soil raw=%4d  moisture=%3d%%  |  LDR disabled  |  pump=%s  mode=%s\n",
                  rawSoil, moistureValue,
                  pumpState ? "ON" : "OFF", autoMode ? "AUTO" : "MANUAL");
#endif
}

// ---------------- Setup ----------------
void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(ADC_SOIL_PIN, INPUT);
    pinMode(PUMP_PIN, OUTPUT);

#if ENABLE_LIGHT_SENSOR
    pinMode(LDR_PIN, LDR_USE_PULLUP ? INPUT_PULLUP : INPUT);
#endif

    pumpState = false;
    digitalWrite(PUMP_PIN, PUMP_ACTIVE_LOW ? HIGH : LOW);
    digitalWrite(LED_BUILTIN, LOW);

    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    delay(1000);
    listAllFiles();

    connectWiFi();

    server.on("/",            HTTP_GET,  handleRoot);
    server.on("/sensor-data", HTTP_GET,  handleSensorData);
    server.on("/toggle-pump", HTTP_POST, handleTogglePump);
    server.on("/auto-mode",   HTTP_POST, handleAutoMode);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

    server.begin();
    Serial.println("HTTP server started");
}

// ---------------- Loop ----------------
void loop() {
    server.handleClient();
    unsigned long now = millis();

    if (pumpState && pumpOnSince && (now - pumpOnSince >= PUMP_MAX_ON_TIME)) {
        Serial.println("!! SAFETY CUTOFF -- pump ran too long");
        stopPump("safety cutoff");
        pumpOnSince = 0;
    }

    if (pumpTimed && (now - pumpStartTime >= PUMP_RUN_TIME)) {
        stopPump("auto cycle complete");
        pumpOnSince = 0;
    }

    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;
        readSensors();

        if (autoMode && !pumpState &&
            (lastPumpStop == 0 || now - lastPumpStop >= PUMP_COOLDOWN) &&
            moistureValue < SOIL_MOISTURE_THRESHOLD) {
            Serial.println("Auto: soil dry -> starting pump");
            startPumpCycle();
        }
    }
}