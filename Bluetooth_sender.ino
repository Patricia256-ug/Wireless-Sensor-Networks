#include <ArduinoBLE.h>

// ---------- Pin definitions ----------
#define LM35_PIN A1
#define SOIL_PIN A2
#define PUMP_PIN 7
#define LED_PIN 6

// ---------- BLE service & characteristics ----------
BLEService wsnService("180A");                 // custom service
BLEStringCharacteristic dataChar("2A6E", BLERead | BLENotify, 40); 
// send all sensor data + timestamp in one packet

// ---------- Sensor helpers ----------
float readTemp() {
  int raw = analogRead(LM35_PIN);
  float v = raw * 3.3 / 1023.0;
  return v * 100.0;        // LM35 → 10 mV/°C
}
int readSoil() { return analogRead(SOIL_PIN); }

// ===================================================
void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println("🔌 Serial connected");

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  Serial.print("🔧 Starting BLE...");
  if (!BLE.begin()) {
    Serial.println("❌ BLE start failed!");
    while (1);
  }
  Serial.println("✅ ok");

  BLE.setLocalName("NodeA_MKR1010");
  BLE.setAdvertisedService(wsnService);
  wsnService.addCharacteristic(dataChar);
  BLE.addService(wsnService);
  BLE.advertise();
  Serial.println("🌿 Node A — BLE Peripheral advertising...");
}

// ===================================================
void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("👋 Connected to ");
    Serial.println(central.address());

    while (central.connected()) {
      float t = readTemp();
      int s   = readSoil();
      bool pump = (s <= 300) || (t >= 36.0);

      digitalWrite(PUMP_PIN, pump);
      digitalWrite(LED_PIN, pump);

      // --- include timestamp for metrics ---
      unsigned long tSend = millis();
      String msg = "TIME:" + String(tSend) +
                   ",TEMP:" + String(t,1) +
                   ",SOIL:" + String(s) +
                   ",PUMP:" + (pump ? "ON" : "OFF");

      dataChar.writeValue(msg);

      Serial.println("📤 BLE Sent → " + msg);
      delay(2000);
    }
    Serial.println("❌ Disconnected");
  }
}