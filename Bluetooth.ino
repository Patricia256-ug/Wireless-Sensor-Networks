#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>

// ---- UUIDs must match Node A ----
static BLEUUID serviceUUID("180A");    // same service UUID as Node A
static BLEUUID dataUUID("2A6E");       // single data characteristic

BLEAdvertisedDevice* advDevice;
bool deviceFound = false;

// ---------- Metric Variables ----------
unsigned long totalPackets = 0;
unsigned long successPackets = 0;
unsigned long totalLatency = 0;
unsigned long startTime;
int msgLength = 0;

// ------------------------------------------------------
// Scan callback: look for Node A advertising our service
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
void setup() {
  Serial.begin(115200);
  Serial.println("🔎 Node B — BLE Central scanning for NodeA_MKR1010...");
  BLEDevice::init("");
  startTime = millis();
}

// ------------------------------------------------------
void loop() {
  // ---- Scan until we find Node A ----
  if (!deviceFound) {
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    scan->setActiveScan(true);
    scan->start(5, false);   // scan for 5 seconds
    delay(1000);
    return;
  }

  // ---- Connect to Node A ----
  BLEClient* client = BLEDevice::createClient();
  Serial.println("🔗 Connecting to NodeA_MKR1010...");
  client->connect(advDevice);

  if (!client->isConnected()) {
    Serial.println("❌ Connection failed, rescanning...");
    deviceFound = false;
    delete advDevice;
    delay(2000);
    return;
  }

  Serial.println("✅ Connected to NodeA_MKR1010!");

  BLERemoteService* service = client->getService(serviceUUID);
  if (service == nullptr) {
    Serial.println("❌ Service not found on peripheral");
    client->disconnect();
    deviceFound = false;
    delay(2000);
    return;
  }

  BLERemoteCharacteristic* dataC = service->getCharacteristic(dataUUID);
  if (!dataC) {
    Serial.println("❌ Data characteristic missing!");
    client->disconnect();
    deviceFound = false;
    delay(2000);
    return;
  }

  // ---- Continuously read data while connected ----
  while (client->isConnected()) {
    std::string val = dataC->readValue();
    String msg = String(val.c_str());
    msg.trim();
    if (msg.length() == 0) continue;
    msgLength = msg.length();

    totalPackets++;
    unsigned long now = millis();

    // --- Parse TIME: for latency ---
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
      Serial.println("⚠ No timestamp found!");
    }

    delay(2000);  // match Node A update rate
  }

  Serial.println("🔁 Disconnected from Node A, restarting scan...");
  deviceFound = false;
  delete advDevice;
  delay(2000);

  // ---- After 5 minutes print summary ----
  if (millis() - startTime >= 300000) {
    float avgLatency  = successPackets ? (float)totalLatency / successPackets : 0;
    float successRate = totalPackets ? (100.0 * successPackets / totalPackets) : 0;
    float durationSec = (millis() - startTime) / 1000.0;
    float dataRate    = (successPackets * msgLength * 8) / durationSec;

    Serial.println("\n===== 📊 METRIC SUMMARY (BLE) =====");
    Serial.print("Total packets: ");    Serial.println(totalPackets);
    Serial.print("Successful: ");       Serial.println(successPackets);
    Serial.print("Success rate: ");     Serial.print(successRate); Serial.println("%");
    Serial.print("Average latency: ");  Serial.print(avgLatency);  Serial.println(" ms");
    Serial.print("Estimated data rate: "); Serial.print(dataRate); Serial.println(" bps");
    Serial.println("====================================");
    while (true); // stop after reporting
  }
  delay(100);
}