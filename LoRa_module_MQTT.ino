/*********************************************************************
  Node B — ESP32 Nano + LoRa E32 + Metrics Collector + MQTT + ACK
*********************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ------------------- Wi-Fi & MQTT -------------------
const char* ssid       = "Desire";
const char* password   = "Desire12";
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;
const char* topic_temp   = "wsn/irrigation/temperature";
const char* topic_soil   = "wsn/irrigation/soil_moisture";
const char* topic_pump   = "wsn/irrigation/pump_status";
const char* topic_metrics= "wsn/irrigation/metrics";

// ------------------- LoRa Pins -------------------
#define LORA_RX 2     // ESP32 pin receiving from E32 TX
#define LORA_TX 3     // ESP32 pin sending to E32 RX

// ------------------- Metric Variables -------------------
unsigned long totalPackets = 0;
unsigned long successPackets = 0;
unsigned long totalLatency = 0;
unsigned long startTime;
int msgLength = 0;

// ------------------- MQTT & Time -------------------
WiFiClient wifiClient;
PubSubClient client(wifiClient);
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

// ===================================================
//                     Helper Functions
// ===================================================
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "NoTime";
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void setupWiFi() {
  Serial.print("Connecting to Wi-Fi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connected");
  Serial.print("📡 IP: "); Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("NodeB_LoRa_Gateway")) {
      Serial.println("✅ Connected to HiveMQ");
    } else {
      Serial.print("❌ rc="); Serial.println(client.state());
      delay(3000);
    }
  }
}

// ===================================================
//                        SETUP
// ===================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);

  Serial.println("🌿 Node B — LoRa Receiver + Metrics + MQTT + ACK Ready");

  setupWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  client.setServer(mqtt_server, mqtt_port);

  startTime = millis();
}

// ===================================================
//                        LOOP
// ===================================================
void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

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

      // --- Parse sensor data ---
      int tIdx = msg.indexOf("TEMP:");
      int sIdx = msg.indexOf(",SOIL:");
      int pIdx = msg.indexOf(",PUMP:");
      float temp = msg.substring(tIdx + 5, sIdx).toFloat();
      int soil = msg.substring(sIdx + 6, pIdx).toInt();
      String pump = msg.substring(pIdx + 6);

      // --- Compute metrics ---
      float avgLatency  = (float)totalLatency / successPackets;
      float successRate = (100.0 * successPackets / totalPackets);
      float durationSec = (millis() - startTime) / 1000.0;
      float dataRate    = (successPackets * msgLength * 8) / (durationSec ? durationSec : 1);

      String ts = getTimestamp();

      // --- Build JSON payloads ---
      String payload_temp = "{\"value\":" + String(temp,1) + ",\"time\":\"" + ts + "\",\"protocol\":\"LoRa\"}";
      String payload_soil = "{\"value\":" + String(soil) + ",\"time\":\"" + ts + "\",\"protocol\":\"LoRa\"}";
      String payload_pump = "{\"value\":\"" + pump + "\",\"time\":\"" + ts + "\",\"protocol\":\"LoRa\"}";

      String metrics_payload = "{";
      metrics_payload += "\"protocol\":\"LoRa\",";
      metrics_payload += "\"avg_latency_ms\":" + String(avgLatency,1) + ",";
      metrics_payload += "\"success_rate\":" + String(successRate,1) + ",";
      metrics_payload += "\"data_rate_bps\":" + String(dataRate,1) + ",";
      metrics_payload += "\"timestamp\":\"" + ts + "\"";
      metrics_payload += "}";

      // --- MQTT publish ---
      client.publish(topic_temp, payload_temp.c_str());
      client.publish(topic_soil, payload_soil.c_str());
      client.publish(topic_pump, payload_pump.c_str());
      client.publish(topic_metrics, metrics_payload.c_str());

      // --- Serial output ---
      Serial.println("📥 LoRa Received → " + msg);
      Serial.print("   ⏱ Latency: "); Serial.print(latency); Serial.println(" ms");
      Serial.println("  🌡 " + payload_temp);
      Serial.println("  🌱 " + payload_soil);
      Serial.println("  💧 " + payload_pump);
      Serial.println("  📊 " + metrics_payload);

    } else {
      Serial.println("⚠ No timestamp found in message");
    }

    // ---------- Send ACK ----------
    Serial1.println("ACK");
    Serial.println("✅ ACK sent to Node A");
    Serial.println("--------------------------------");
  }

  delay(100);
}