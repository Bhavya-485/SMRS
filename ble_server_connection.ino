#include <BLEDevice.h>
#include <BLEServer.h>

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Client Connected ✅");
  }

  void onDisconnect(BLEServer* pServer) {
    Serial.println("Client Disconnected");
  }
};

void setup() {
  Serial.begin(115200);
  delay(2000);

  BLEDevice::init("Medicine_Box");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEDevice::getAdvertising()->start();

  Serial.println("Server ready — waiting...");
}

void loop() {}