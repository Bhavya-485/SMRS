
#include <NimBLEDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ================= MOTOR ================= */
#define MOTOR_PIN 10   // ✅ coin vibrator pin (FIXED)

/* ================= BLE UUIDs ================= */
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-abcdef123456"

/* ================= BLE OBJECTS ================= */
static NimBLEAdvertisedDevice* advDevice = nullptr;
static NimBLEClient* pClient = nullptr;
static NimBLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

volatile bool connected = false;

/* ================= MOTOR ================= */
unsigned long motorStartTime = 0;
unsigned long motorDuration = 0;
bool motorActive = false;

void vibrateMotor(int durationMs) {
  Serial.println("VIBRATING MOTOR");
  digitalWrite(MOTOR_PIN, HIGH);
  motorStartTime = millis();
  motorDuration = durationMs;
  motorActive = true;

}

/* ================= OLED ================= */
char currentMessage[50];
unsigned long messageStartTime = 0;
bool messageActive = false;

void showMessage(const char* msg) {
  Serial.println("OLED DISPLAY UPDATE");

  strncpy(currentMessage, msg, sizeof(currentMessage));
currentMessage[sizeof(currentMessage) - 1] = '\0';
  messageStartTime = millis();
  messageActive = true;

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
}
/* ================= NOTIFY CALLBACK ================= */
void notifyCallback(
  NimBLERemoteCharacteristic*,
  uint8_t* pData,
  size_t length,
  bool) {
    char msg[50];
size_t copyLen = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
memcpy(msg, pData, copyLen);
msg[copyLen] = '\0';

  Serial.print("Received: ");
  Serial.println(msg);

  if (strcmp(msg, "Take medicine") == 0) {
    vibrateMotor(10000);
    showMessage("take\nmedicine");
    

  }
  else if (strcmp(msg, "Medicine taken") == 0){
    showMessage("Medicine\nTaken");
}
  else if (strcmp(msg, "Dosage missed") ==0) {
  vibrateMotor(5000);
  showMessage("Dosage\nMissed");
}
}

/* ================= CLIENT CALLBACK ================= */
class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient*) {
    Serial.println("BLE Connected");
    connected = true;
    showMessage("BLE\nConnected");
  }

  void onDisconnect(NimBLEClient*) {
    Serial.println("BLE Disconnected");
    connected = false;
    showMessage("Waiting\nBLE");
  }
};
ClientCallbacks clientCallbacks;

/* ================= SCAN CALLBACK ================= */
class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {

    if (device->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
      Serial.println("Target BLE Device Found");
      advDevice = new NimBLEAdvertisedDevice(*device);
      NimBLEDevice::getScan()->stop();
    }
  }
};

/* ================= CONNECT FUNCTION ================= */
bool connectToServer(NimBLEAdvertisedDevice* device) {

  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(&clientCallbacks);

  if (!pClient->connect(device)) {
    Serial.println("Connection Failed");
    return false;
  }

  Serial.println("Connected to Server");

  NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
  if (!pService) {
    Serial.println("Service Not Found");
    return false;
  }

  pRemoteCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);
  if (!pRemoteCharacteristic) {
    Serial.println("Characteristic Not Found");
    return false;
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->subscribe(true, notifyCallback);
    Serial.println("Notify Subscribed");
  }

  connected = true;
  return true;
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  /* OLED INIT */
  Wire.begin(5, 6);   // ✅ SDA=5, SCL=6 (FIXED)

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Init Failed");
    while (1);
  }

  showMessage("Waiting\nBLE");

  /* BLE INIT */
  NimBLEDevice::init("ESP32_Client");

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCallbacks());
  scan->setInterval(45);
  scan->setWindow(15);
  scan->setActiveScan(true);
  scan->start(0, false);
}

/* ================= LOOP ================= */
void loop() {
  // Auto clear OLED after 10 seconds (non-blocking)
if (messageActive && millis() - messageStartTime > 10000) {
  display.clearDisplay();
  display.display();
  messageActive = false;
}

/*connect if found*/
  if (!connected && advDevice != nullptr) {
    connectToServer(advDevice);
    delete advDevice;
    advDevice = nullptr;
  }
  /*reconect scan*/
  static unsigned long lastScan=0;

  if(!connected){

    if(millis()-lastScan>10000){

      Serial.println("Restart Scan");

      NimBLEDevice::getScan()->start(5,false);

      lastScan=millis();

    }

  }

  // Stop motor non-blocking
if (motorActive && millis() - motorStartTime > motorDuration) {
  digitalWrite(MOTOR_PIN, LOW);
  motorActive = false;
}
  
}