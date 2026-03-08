#include <BLEDevice.h>
#include <BLEClient.h>

BLEAddress serverAddress("1C:DB:D4:E0:87:92");

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("Starting BLE client...");

  BLEDevice::init("");

  BLEClient* client = BLEDevice::createClient();

  Serial.println("Connecting...");

  bool ok = client->connect(serverAddress);

  if (ok) {
    Serial.println("Connected to SuperMini ✅");
  } else {
    Serial.println("Connection failed ❌");
  }
}

void loop() {}