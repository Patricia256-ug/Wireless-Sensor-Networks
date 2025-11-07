/*********************************************************************
  Node B — ESP32 Nano + LoRa E32 + Metrics Collector + ACK
*********************************************************************/
#include <Arduino.h>
#include <time.h>

// ------------------- LoRa Pins -------------------
#define LORA_RX 2     // ESP32 pin receiving from E32 TX
#define LORA_TX 3     // ESP32 pin sending to E32 RX

// ------------------- Metric Variables -------------------
unsigned long totalPackets = 0;
unsigned long successPackets = 0;
unsigned long totalLatency = 0;
unsigned long startTime;
int msgLength = 0;

// ===================================================
//                        SETUP
// ===================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);

  Serial.println("🌿 Node B — LoRa Receiver + Metrics Collector + ACK Ready");
  Serial.println("Waiting for LoRa data...");
  startTime = millis();
}

// ===================================================
//                        LOOP
// ===================================================
void loop() {
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    if (msg.length() == 0) return;
    msgLength = msg.length();

    totalPackets++;
    unsigned long now = millis();

    // ---------- Parse timestamp ----------
    int idx = msg.indexOf("TIME:");
    if (idx >= 0) {
      int comma = msg.indexOf(',', idx);
      unsigned long tSend = msg.substring(idx + 5, comma).toInt();
      unsigned long latency = now - tSend;

      totalLatency += latency;
      successPackets++;

      Serial.println("📥 Received → " + msg);
      Serial.print("   ⏱ Latency: ");
      Serial.print(latency);
      Serial.println(" ms");
    } else {
      Serial.println("⚠ No timestamp found in message");
    }

    // ---------- Send ACK ----------
    Serial1.println("ACK");
    Serial.println("✅ ACK sent to Node A");
    Serial.println("--------------------------------");
  }

  // ---------- Summary after 5 minutes ----------
  if (millis() - startTime >= 100000) {  // 5 minutes = 300 000 ms
    float avgLatency  = successPackets ? (float)totalLatency / successPackets : 0;
    float successRate = totalPackets ? (100.0 * successPackets / totalPackets) : 0;
    float durationSec = (millis() - startTime) / 1000.0;
    float dataRate    = (successPackets * msgLength * 8) / durationSec; // bps

    Serial.println("\n===== 📊 METRIC SUMMARY (LoRa) =====");
    Serial.print("Total packets received: ");  Serial.println(totalPackets);
    Serial.print("Successful packets:     ");  Serial.println(successPackets);
    Serial.print("Packet success rate:   ");  Serial.print(successRate); Serial.println(" %");
    Serial.print("Average latency:      ");  Serial.print(avgLatency); Serial.println(" ms");
    Serial.print("Estimated data rate:  ");  Serial.print(dataRate);  Serial.println(" bps");
    Serial.println("====================================");

    while (true); // stop after reporting
  }
  delay(100);
}