#include <WiFiNINA.h>
#include <WiFiUdp.h>

// ---------- Wi-Fi credentials ----------
char ssid[] = "Desire";
char pass[] = "Desire12";

// ---------- UDP setup ----------
WiFiUDP udp;
IPAddress nodeB_IP(192, 168, 137, 216);   // Node B’s IP
unsigned int port = 4210;                // destination port

// ---------- Pin definitions ----------
#define LM35_PIN A1
#define SOIL_PIN A2
#define PUMP_PIN 7
#define LED_PIN 6

// ---------- Helper functions ----------
float readTemp() {
  int raw = analogRead(LM35_PIN);
  float v = raw * 3.3 / 1023.0;
  return v * 100.0;  // LM35 = 10 mV/°C
}

int readSoil() { return analogRead(SOIL_PIN); }

// ===================================================
void setup() {
  Serial.begin(9600);
  Serial.println("🌿 Node A — MKR1010 Wi-Fi UDP Transmitter");
  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Connected to Wi-Fi");
  Serial.print("📡 Local IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(4211);  // local port
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  Serial.println("🚀 Ready to transmit data via UDP\n");
}

// ===================================================
void loop() {
  float t = readTemp();
  int s   = readSoil();
  bool pump = (s <= 300) || (t >= 36.0);

  digitalWrite(PUMP_PIN, pump);
  digitalWrite(LED_PIN, pump);

  // ---- Add timestamp for latency measurement ----
  unsigned long tSend = millis();

  // ---- Labeled, timestamped message ----
  String msg = "TIME:" + String(tSend) +
               ",TEMP:" + String(t, 1) +
               ",SOIL:" + String(s) +
               ",PUMP:" + (pump ? "ON" : "OFF");

  // ---- Transmit via UDP ----
  udp.beginPacket(nodeB_IP, port);
  udp.print(msg);
  udp.endPacket();

  Serial.println("📤 Sent UDP → " + msg);
  delay(2000);
}