#include <Arduino.h>
#include "LoRa_E32.h"

// ---------- Pin Definitions ----------
#define LM35_PIN   A1
#define SOIL_PIN   A2
#define PUMP_PIN   7
#define LED_PIN    6
#define LORA_AUX   3
#define LORA_SET   4

// ---------- Thresholds ----------
const int   soilThreshold = 300;
const float tempThreshold = 36.0;

// ---------- LoRa Setup ----------
LoRa_E32 e32ttl(&Serial1);

// ---------- Helper Functions ----------
float readTemperatureC() {
  int raw = analogRead(LM35_PIN);
  float voltage = (raw * 3.3) / 1023.0;
  return voltage * 100.0;    // LM35 → 10 mV / °C
}

int readSoilMoisture() {
  return analogRead(SOIL_PIN);
}

// ===================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  e32ttl.begin();

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LORA_AUX, INPUT);
  pinMode(LORA_SET, OUTPUT);
  digitalWrite(LORA_SET, HIGH);   // Normal mode

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  Serial.println("🌿 Node A — LoRa Sensor Transmitter Ready");
  delay(1000);
}

// ===================================================
void loop() {
  // ---- Sensor readings ----
  int   soilRaw = readSoilMoisture();
  float temperatureC = readTemperatureC();

  // ---- Determine Pump Status ----
  bool soilDry  = (soilRaw <= soilThreshold);
  bool tempHigh = (temperatureC >= tempThreshold);
  bool pumpOn   = soilDry || tempHigh;

  digitalWrite(PUMP_PIN, pumpOn ? HIGH : LOW);
  digitalWrite(LED_PIN,  pumpOn ? HIGH : LOW);

  // ---- Add timestamp for metrics ----
  unsigned long tSend = millis();

  // ---- Prepare labeled message ----
  String message = "TIME:" + String(tSend) +
                   ",TEMPERATURE:" + String(temperatureC, 1) +
                   ",SOIL MOISTURE:" + String(soilRaw) +
                   ",PUMP STATUS:" + (pumpOn ? "ON" : "OFF");

  // ---- Send via LoRa ----
  e32ttl.sendMessage(message);
  Serial.println("📤 Sent via LoRa → " + message);

  // ---- Wait for module idle ----
  unsigned long auxWaitStart = millis();
  while (digitalRead(LORA_AUX) == LOW && (millis() - auxWaitStart < 2000)) {
    delay(10);
  }

  // ---- Wait for ACK from Node B ----
  unsigned long startTime = millis();
  bool ackReceived = false;

  while (millis() - startTime < 3000) {
    if (Serial1.available()) {
      String reply = Serial1.readStringUntil('\n');
      reply.trim();
      if (reply == "ACK") {
        ackReceived = true;
        break;
      }
    }
  }

  if (ackReceived) {
    Serial.println("✅ ACK received from Node B");
  } else {
    Serial.println("⚠ No ACK received");
  }

  Serial.println("--------------------------------");
  delay(2000);
}