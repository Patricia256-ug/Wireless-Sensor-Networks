#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <time.h>

// ---------- Wi-Fi credentials ----------
const char* ssid       = "Desire";
const char* password   = "Desire12";

// ---------- MQTT broker ----------
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;
const char* mqtt_topic_temp = "wsn/irrigation/temperature";
const char* mqtt_topic_soil = "wsn/irrigation/soil_moisture";
const char* mqtt_topic_pump = "wsn/irrigation/pump_status";
const char* mqtt_topic_metrics = "wsn/irrigation/metrics";  // optional extra topic

// ---------- UDP setup ----------
WiFiUDP udp;
const unsigned int localPort = 4210;

// ---------- Metric variables ----------
unsigned long totalPackets = 0;
unsigned long successPackets = 0;
unsigned long totalLatency = 0;
unsigned long startTime;

// ---------- MQTT client ----------
WiFiClient espClient;
PubSubClient client(espClient);

// ---------- NTP ----------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

// ===================================================
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "NoTime";
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void setup_wifi() {
  Serial.print("Connecting to Wi-Fi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connected");
  Serial.print("📡 IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("NodeB_Gateway")) {
      Serial.println("✅ connected");
    } else {
      Serial.print("❌ rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ===================================================
void setup() {
  Serial.begin(115200);
  setup_wifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  client.setServer(mqtt_server, mqtt_port);
  udp.begin(localPort);
  Serial.printf("📥 Listening for UDP packets on port %u ...\n", localPort);
  startTime = millis();
}

// ===================================================
void loop() {
  if (!client.connected()) reconnect_mqtt();
  client.loop();

  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buffer[250];
    int len = udp.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = 0;

    String msg = String(buffer);
    msg.trim();
    if (msg.length() == 0) return;

    totalPackets++;
    unsigned long now = millis();

    // Example format from Node A: TEMP:30.5,SOIL:280,PUMP:ON
    int c1 = msg.indexOf("TEMP:");
    int c2 = msg.indexOf(",SOIL:");
    int c3 = msg.indexOf(",PUMP:");
    float temp = msg.substring(c1 + 5, c2).toFloat();
    int soil = msg.substring(c2 + 6, c3).toInt();
    String pump = msg.substring(c3 + 6);

    unsigned long latency = 0; // (For UDP example, we’re not timestamping packets)
    successPackets++;

    // --- Calculate metrics ---
    float avgLatency  = successPackets ? (float)totalLatency / successPackets : 0;
    float successRate = totalPackets ? (100.0 * successPackets / totalPackets) : 0;
    float durationSec = (millis() - startTime) / 1000.0;
    float dataRate    = (successPackets * msg.length() * 8) / (durationSec ? durationSec : 1);

    // --- Timestamp ---
    String ts = getTimestamp();

    // --- Prepare JSON payloads ---
    String payload_temp = "{\"value\":" + String(temp,1) + ",\"time\":\"" + ts + "\",\"protocol\":\"WiFi\"}";
    String payload_soil = "{\"value\":" + String(soil) + ",\"time\":\"" + ts + "\",\"protocol\":\"WiFi\"}";
    String payload_pump = "{\"value\":\"" + pump + "\",\"time\":\"" + ts + "\",\"protocol\":\"WiFi\"}";

    String metrics_payload = "{";
    metrics_payload += "\"protocol\":\"WiFi\",";
    metrics_payload += "\"latency_ms\":" + String(avgLatency,1) + ",";
    metrics_payload += "\"success_rate\":" + String(successRate,1) + ",";
    metrics_payload += "\"data_rate_bps\":" + String(dataRate,1) + ",";
    metrics_payload += "\"timestamp\":\"" + ts + "\"";
    metrics_payload += "}";

    // --- Publish to MQTT ---
    client.publish(mqtt_topic_temp, payload_temp.c_str());
    client.publish(mqtt_topic_soil, payload_soil.c_str());
    client.publish(mqtt_topic_pump, payload_pump.c_str());
    client.publish(mqtt_topic_metrics, metrics_payload.c_str());

    // --- Serial Monitor output ---
    Serial.println("📥 Received → " + msg);
    Serial.println("  🌡 Temp  → " + payload_temp);
    Serial.println("  🌱 Soil  → " + payload_soil);
    Serial.println("  💧 Pump  → " + payload_pump);
    Serial.println("  📊 Metrics → " + metrics_payload);
    Serial.println("--------------------------------");
  }
  delay(500);
}