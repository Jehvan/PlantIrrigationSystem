#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// ================================================================
//  DEBUG AP MODE
//  1 = ESP32 creates its own WiFi network (no router needed)
//  0 = normal mode, connects to your home WiFi below
// ================================================================
#define DEBUG_AP_MODE 1

// --- Home WiFi (used when DEBUG_AP_MODE = 0) ---
const char* ssid     = "NULL";
const char* password = "NULL";

// --- Debug AP credentials (used when DEBUG_AP_MODE = 1) ---
const char* ap_ssid     = "ESP32-Irrigation";
const char* ap_password = "irrigation123";   // min 8 chars, or "" for open

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

// --- Feature switches ---
#define ENABLE_LIGHT_SENSOR 1

// --- Pinout ---
#define ADC_SOIL_PIN 4
#define LDR_PIN      6
#define PUMP_PIN     5

// --- Relay polarity ---
#define PUMP_ACTIVE_LOW 0

// --- Digital LDR polarity ---
#define LDR_BRIGHT_IS_HIGH 0
#define LDR_USE_PULLUP 0

// --- Timing ---
#define SENSOR_READ_INTERVAL     1000UL
#define PUMP_RUN_TIME            5000UL
#define PUMP_COOLDOWN            30000UL
#define PUMP_MAX_ON_TIME         60000UL

// --- Thresholds ---
// Test value: set above your current reading to force auto to fire.
// Set to 28 for "normal" operation.
#define SOIL_MOISTURE_THRESHOLD  28

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
int  lightDigital  = 0;
bool lightIsBright = false;

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
#if DEBUG_AP_MODE
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);

    bool ok = (strlen(ap_password) >= 8)
        ? WiFi.softAP(ap_ssid, ap_password)
        : WiFi.softAP(ap_ssid);

    if (!ok) {
        Serial.println("AP start FAILED");
        return;
    }
    Serial.println("=== DEBUG AP MODE ===");
    Serial.printf("SSID    : %s\n", ap_ssid);
    Serial.printf("Password: %s\n", strlen(ap_password) >= 8 ? ap_password : "(open)");
    Serial.print ("Open    : http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("=====================");
#else
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
        Serial.print("Connected! IP: http://");
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
#endif
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
    json += "\"light\":"       + String(lightIsBright ? 100 : 0) + ",";
    json += "\"lightState\":\"" + String(lightIsBright ? "BRIGHT" : "DARK") + "\",";
    json += "\"lightRaw\":"    + String(lightDigital) + ",";
#else
    json += "\"light\":\"--\",";
    json += "\"lightState\":\"--\",";
    json += "\"lightRaw\":\"--\",";
#endif

    json += "\"pump\":\"" + String(pumpState ? "ON" : "OFF") + "\",";
    json += "\"mode\":\"" + String(autoMode  ? "AUTO" : "MANUAL") + "\",";
    json += "\"wifi\":\"" + String(DEBUG_AP_MODE ? "AP" : "STA") + "\"";
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

    Serial.println("Mode -> MANUAL");

    String json = "{\"status\":\"" + String(pumpState ? "ON" : "OFF") +
                  "\",\"mode\":\"MANUAL\"}";
    server.send(200, "application/json", json);
}

void handleAutoMode() {
    autoMode = true;

    // Stop the pump if it's running (manual OR timed) -- this is the fix
    if (pumpState) {
        stopPump("return to auto");
    }

    Serial.println("Mode -> AUTO");

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

// ---------------- Auto decision with reason logging ----------------
void runAutoDecision() {
    if (!autoMode) return;   // silent when in manual

    unsigned long now = millis();

    if (pumpState) {
        // pump already running -- nothing to report, it's fine
        return;
    }

    if (lastPumpStop != 0 && (now - lastPumpStop) < PUMP_COOLDOWN) {
        static unsigned long lastReport = 0;
        if (now - lastReport >= 5000) {
            lastReport = now;
            Serial.printf("Auto: waiting for cooldown (%lus left)\n",
                          (PUMP_COOLDOWN - (now - lastPumpStop)) / 1000);
        }
        return;
    }

    if (moistureValue >= SOIL_MOISTURE_THRESHOLD) {
        static unsigned long lastReport = 0;
        if (now - lastReport >= 5000) {
            lastReport = now;
            Serial.printf("Auto: soil OK (%d%% >= %d%%), no water needed\n",
                          moistureValue, SOIL_MOISTURE_THRESHOLD);
        }
        return;
    }

    Serial.printf("Auto: FIRE (soil %d%% < %d%%)\n",
                  moistureValue, SOIL_MOISTURE_THRESHOLD);
    startPumpCycle();
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
        runAutoDecision();
    }
}