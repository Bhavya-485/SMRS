#include <BLEDevice.h>

void setup(){
  Serial.begin(115200);
  delay(2000);   // IMPORTANT

  Serial.println("Starting BLE...");

  BLEDevice::init("ESP32");
  Serial.println("BLE OK ✅");
}

void loop(){}