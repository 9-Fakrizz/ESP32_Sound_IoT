#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Configuration ---
const char* ssid = "Ok";
const char* password = "q12345678";
const char* mqtt_server = "demo.thingsboard.io";
const char* token = "VgpSbXAQ5RWAlCkDfJxn";

// Hardware pins
const int soundSensorPin = 34;
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;
PubSubClient client(espClient);

// Timer and Averaging Variables
unsigned long lastSampleTime = 0;
unsigned long lastUpdateTime = 0;
const int sampleInterval = 50;   // Sample every 50ms
const int updateInterval = 500;  // Update every 500ms (10 samples)
long dbSum = 0;
int sampleCount = 0;

void updateStatus(String status, int dbValue = -1) {
  Serial.print("Status: ");
  Serial.print(status);
  if(dbValue != -1) {
    Serial.print(" | Avg dB: ");
    Serial.print(dbValue);
  }
  Serial.println();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Status: ");
  display.println(status);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  if(dbValue != -1) {
    display.setCursor(0, 25);
    display.setTextSize(2);
    display.print(dbValue);
    display.print(" dB");
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;); 
  }
  updateStatus("Starting...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    updateStatus("WiFi Connecting...");
    delay(500);
  }
  updateStatus("WiFi OK");
  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    updateStatus("MQTT Connecting...");
    if (client.connect("ESP32Client", token, NULL)) {
      updateStatus("MQTT OK");
    } else {
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  // 1. SAMPLE EVERY 50ms
  if (currentMillis - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentMillis;

    int rawValue = analogRead(soundSensorPin);
    int flippedValue = 4095 - rawValue; 
    
    // Using your specific calculation formula
    int currentDb = (flippedValue / 10.0) - 87;
    
    dbSum += currentDb; // Add to running total
    sampleCount++;
  }

  // 2. SEND & UPDATE EVERY 500ms (after 10 samples)
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;

    if (sampleCount > 0) {
      int avgDb = dbSum / sampleCount; // Calculate average

      // Update OLED and Serial
      updateStatus("Online", avgDb);

      // Send to ThingsBoard
      String payload = "{\"dB\":" + String(avgDb) + "}";
      if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
        Serial.println("Telemetry sent to TB");
      }

      // Reset accumulators for next batch
      dbSum = 0;
      sampleCount = 0;
    }
  }
}