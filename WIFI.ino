#include <WiFi.h>
#include <WiFiUdp.h>

// ---------- Wi-Fi credentials ----------
const char* ssid     = "Desire";
const char* password = "Desire12";

// ---------- UDP setup ----------
WiFiUDP udp;
const unsigned int localPort = 4210;

// ---------- Metric variables ----------
unsigned long totalPackets = 0;
unsigned long successPackets = 0;
unsigned long totalLatency = 0;
unsigned long startTime;
int msgLength = 0;

// ===================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n🌐 Node B — ESP32 UDP Receiver + Metric Collector");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wi-Fi");
  Serial.print("📡 Local IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(localPort);
  Serial.printf("📥 Listening for UDP packets on port %u ...\n", localPort);

  startTime = millis();
}

// ===================================================
void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buffer[250];
    int len = udp.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = 0;

    String msg = String(buffer);
    msg.trim();
    msgLength = msg.length();
    if (msgLength == 0) return;

    totalPackets++;
    unsigned long now = millis();

    // ---- Parse TIME for latency ----
    int idx = msg.indexOf("TIME:");
    if (idx >= 0) {
      int comma = msg.indexOf(',', idx);
      unsigned long tSend = msg.substring(idx + 5, comma).toInt();
      unsigned long latency = now - tSend;
      totalLatency += latency;
      successPackets++;

      Serial.print("📦 UDP Received → ");
      Serial.println(msg);
      Serial.print("   ⏱ Latency: ");
      Serial.print(latency);
      Serial.println(" ms");
    } else {
      Serial.println("⚠ No TIME field found!");
    }
  }

  // ---- After 5 min, print summary ----
  if (millis() - startTime >= 300000) {   // 5 min = 300 000 ms
    float avgLatency  = successPackets ? (float)totalLatency / successPackets : 0;
    float successRate = totalPackets ? (100.0 * successPackets / totalPackets) : 0;
    float durationSec = (millis() - startTime) / 1000.0;
    float dataRate    = (successPackets * msgLength * 8) / durationSec;  // bits / s

    Serial.println("\n===== 📊 METRIC SUMMARY (Wi-Fi UDP) =====");
    Serial.print("Total packets: ");    Serial.println(totalPackets);
    Serial.print("Successful: ");       Serial.println(successPackets);
    Serial.print("Success rate: ");     Serial.print(successRate); Serial.println(" %");
    Serial.print("Average latency: ");  Serial.print(avgLatency);  Serial.println(" ms");
    Serial.print("Estimated data rate: "); Serial.print(dataRate); Serial.println(" bps");
    Serial.println("=========================================");
    while (true); // stop after report
  }

  delay(100);
}