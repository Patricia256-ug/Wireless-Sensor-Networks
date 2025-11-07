#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ---------- Wi-Fi credentials ----------
const char* ssid = "Desire";
const char* password = "Desire12";

// ---------- MQTT broker ----------
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;
const char* topic_temp   = "wsn/irrigation/temperature";
const char* topic_soil   = "wsn/irrigation/soil_moisture";
const char* topic_pump   = "wsn/irrigation/pump_status";
const char* topic_metrics= "wsn/irrigation/metrics";

// ---------- NTP ----------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

// ---------- MQTT client ----------
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ---------- BLE UUIDs (match Node A) ----------
static BLEUUID serviceUUID("180A");   // same service
static BLEUUID dataUUID("2A6E");      // single characteristic

BLEAdvertisedDevice* advDevice;
bool deviceFound = false;

// ---------- Metrics ----------
unsigned long totalPackets = 0;
unsigned long successPackets = 0;
unsigned long totalLatency = 0;
unsigned long startTime;

// ------------------------------------------------------
// BLE scan callback
// ------------------------------------------------------
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.getServiceUUID().equals(serviceUUID)) {
      advDevice = new BLEAdvertisedDevice(advertisedDevice);
      deviceFound = true;
      advertisedDevice.getScan()->stop();
      Serial.println("✅ Found NodeA_MKR1010 advertising!");
    }
  }
};

// ------------------------------------------------------
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "NoTime";
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void setup_wifi() {
  Serial.print("Connecting Wi-Fi ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connected");
  Serial.print("📡 IP: "); Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (client.connect("NodeB_BLE_Gateway")) {
      Serial.println("✅ connected");
    } else {
      Serial.print("❌ rc="); Serial.println(client.state());
      delay(2000);
    }
  }
}

// ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("🌿 Node B — BLE + MQTT Gateway");

  setup_wifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  client.setServer(mqtt_server, mqtt_port);

  BLEDevice::init("");
  startTime = millis();
}

// ------------------------------------------------------
void loop() {
  if (!client.connected()) reconnect_mqtt();
  client.loop();

  // ---- Scan for Node A ----
  if (!deviceFound) {
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    scan->setActiveScan(true);
    scan->start(5, false);
    delay(1000);
    return;
  }

  // ---- Connect to Node A ----
  BLEClient* clientBLE = BLEDevice::createClient();
  Serial.println("🔗 Connecting to Node A...");
  clientBLE->connect(advDevice);

  if (!clientBLE->isConnected()) {
    Serial.println("❌ Connection failed — rescanning...");
    deviceFound = false;
    delete advDevice;
    delay(2000);
    return;
  }
  Serial.println("✅ Connected to Node A");

  BLERemoteService* service = clientBLE->getService(serviceUUID);
  if (!service) {
    Serial.println("❌ Service not found");
    clientBLE->disconnect();
    deviceFound = false;
    delay(2000);
    return;
  }

  BLERemoteCharacteristic* dataC = service->getCharacteristic(dataUUID);
  if (!dataC) {
    Serial.println("❌ Data characteristic not found!");
    clientBLE->disconnect();
    deviceFound = false;
    delay(2000);
    return;
  }

  // ---- Read values repeatedly ----
  while (clientBLE->isConnected()) {
    std::string val = dataC->readValue();
    String msg = String(val.c_str());
    msg.trim();

    if (msg.length() > 0) {
      Serial.println("📥 Received → " + msg);

      // Parse fields
      int tIdx = msg.indexOf("TIME:");
      int tempIdx = msg.indexOf("TEMP:");
      int soilIdx = msg.indexOf("SOIL:");
      int pumpIdx = msg.indexOf("PUMP:");

      unsigned long tSend = msg.substring(tIdx + 5, tempIdx - 1).toInt();
      float temp = msg.substring(tempIdx + 5, soilIdx - 1).toFloat();
      int soil = msg.substring(soilIdx + 5, pumpIdx - 1).toInt();
      String pump = msg.substring(pumpIdx + 5);

      unsigned long latency = millis() - tSend;

      totalPackets++;
      successPackets++;
      totalLatency += latency;

      float avgLatency  = (float)totalLatency / successPackets;
      float successRate = 100.0 * successPackets / totalPackets;
      float durationSec = (millis() - startTime) / 1000.0;
      float dataRate = (successPackets * msg.length() * 8) / (durationSec ? durationSec : 1);

      String ts = getTimestamp();

      // --- JSON payloads ---
      String payload_temp = "{\"value\":" + String(temp,1) + ",\"time\":\"" + ts + "\",\"protocol\":\"BLE\"}";
      String payload_soil = "{\"value\":" + String(soil) + ",\"time\":\"" + ts + "\",\"protocol\":\"BLE\"}";
      String payload_pump = "{\"value\":\"" + pump + "\",\"time\":\"" + ts + "\",\"protocol\":\"BLE\"}";

      String metrics_payload = "{";
      metrics_payload += "\"protocol\":\"BLE\",";
      metrics_payload += "\"latency_ms\":" + String(avgLatency,1) + ",";
      metrics_payload += "\"success_rate\":" + String(successRate,1) + ",";
      metrics_payload += "\"data_rate_bps\":" + String(dataRate,1) + ",";
      metrics_payload += "\"timestamp\":\"" + ts + "\"}";
      
      // --- Publish to MQTT ---
      client.publish(topic_temp, payload_temp.c_str());
      client.publish(topic_soil, payload_soil.c_str());
      client.publish(topic_pump, payload_pump.c_str());
      client.publish(topic_metrics, metrics_payload.c_str());

      // --- Serial log ---
      Serial.println("🌡  Temp  → " + payload_temp);
      Serial.println("🌱  Soil  → " + payload_soil);
      Serial.println("💧  Pump  → " + payload_pump);
      Serial.println("📊  Metrics → " + metrics_payload);
      Serial.println("--------------------------------");
    }

    delay(2000);
  }

  Serial.println("🔁 Disconnected — restarting scan...");
  deviceFound = false;
  delete advDevice;
  delay(2000);
}