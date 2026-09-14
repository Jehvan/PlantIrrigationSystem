#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

const char* ssid = "Telekom-334548";
const char* password = "vd9pf696n55r"; // lowkey me mrzi da go stavam vo secret file

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

#define ADC_SOIL_PIN 4
#define SENSOR_READ_INTERVAL 1000 // milliseconds

#define PUMP_PIN 5
#define PUMP_RUN_TIME 5000 // milliseconds

#define SOIL_MOISTURE_THRESHOLD 28 //Vo %

WebServer server(80);

void listAllFiles() {
  Serial.println("\n--- Listing all files in LittleFS ---");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
  Serial.println("-----------------------------------\n");
}

void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  // Stream the file directly to the browser client
  server.streamFile(file, "text/html");
  file.close();
}

void setup() {
    Serial.begin(115200);
	pinMode(LED_BUILTIN, OUTPUT);
    pinMode(ADC_SOIL_PIN, INPUT);
    pinMode(PUMP_PIN, OUTPUT);

    if (!LittleFS.begin(true)) { // 'true' forces a format if LittleFS is corrupted
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    delay(1000); // Wait for a second to ensure LittleFS is ready
    listAllFiles();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);

    server.begin();
}

void loop() {
    int soilMoistureValue = analogRead(ADC_SOIL_PIN);
    int normMoistureValue = map(soilMoistureValue, 4095, 0, 0, 100);
    Serial.print("Soil Moisture Raw Value: ");
    Serial.println(soilMoistureValue);
    Serial.print("Soil Moisture Normalized Value: ");
    Serial.print(normMoistureValue);
    Serial.println("%");

    if (normMoistureValue < SOIL_MOISTURE_THRESHOLD) {
        digitalWrite(LED_BUILTIN, HIGH);
        digitalWrite(PUMP_PIN, HIGH);
        delay(PUMP_RUN_TIME);
        digitalWrite(PUMP_PIN, LOW);
    } else {
        digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(PUMP_PIN, LOW);
    }

    server.handleClient();
    delay(SENSOR_READ_INTERVAL);
}
