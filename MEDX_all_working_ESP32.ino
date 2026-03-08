#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include <NimBLEDevice.h>
#include <ESP32Servo.h>

/* WIFI */
const char* ssid ="Chethan";
const char* password ="123456789";

/* FIREBASE */
char reminderTimes[10][6];  
char projectId[] = "medx-2d235";
char userId[] = "tGfHIAyHavSPG04Vt05A0eXalDD2";

/* TIME */
const char* ntpServer="pool.ntp.org";
const long gmtOffset_sec=19800;

/* PINS */
#define LED 8
#define BUZZER 2
#define BUTTON 10
#define REFILL_BUTTON 7
#define COMPARTMENT_SERVO_PIN 4
#define LID_SERVO_PIN 5

/* BLE */
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-abcdef123456"

NimBLECharacteristic* pCharacteristic;
bool deviceConnected=false;
bool notifyEnabled=false;

/* SERVOS */
Servo compartmentServo;
Servo lidServo;
int currentCompartmentAngle=0;
int currentLidAngle=0;

/* REMINDER STATE */
int timeCount=0;
int lastTriggered=-1;
bool reminderActive=false;
bool missedLogged=false;

unsigned long reminderStartMillis=0;
unsigned long lastAlertMillis=0;
unsigned long lastFetch=0;
unsigned long lastButtonPress=0;

/* ================= WIFI ================= */
void checkWiFi(){

  if(WiFi.status()==WL_CONNECTED) return;

  Serial.println("Connecting WiFi...");
  WiFi.disconnect(true);
  WiFi.begin(ssid,password);

  unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    if(millis()-start>10000){
      Serial.println("WiFi Failed");
      return;
    }
  }
  Serial.println("WiFi Connected");
}

/* ================= TIME ================= */
void setupTime(){
  configTime(gmtOffset_sec,0,ntpServer);
  struct tm t;
  while(!getLocalTime(&t)){
    delay(500);
  }
  Serial.println("Time Synced");
}

/* ================= SERVO ================= */
void moveServo(Servo &s,int pin,int target,int &current){

  if(!s.attached()) s.attach(pin);

  int step=(target>current)?1:-1;

  for(int pos=current;pos!=target;pos+=step){
    s.write(pos);
    delay(15);
  }

  s.write(target);
  delay(200);
  current=target;
  s.detach();
}

/* ================= OPEN ================= */
void openCompartment(int hour){

  int angle;

  if(hour<12) angle=0;
  else if(hour<18) angle=90;
  else angle=180;

  moveServo(compartmentServo,COMPARTMENT_SERVO_PIN,angle,currentCompartmentAngle);
  moveServo(lidServo,LID_SERVO_PIN,0,currentLidAngle);
}

/* ================= START REMINDER ================= */
void startReminder(){

  Serial.println("Reminder Triggered");

  reminderActive=true;
  missedLogged=false;
  reminderStartMillis=millis();
  lastAlertMillis=millis();

  digitalWrite(LED,HIGH);
  digitalWrite(BUZZER,HIGH);

  struct tm t;
  if(getLocalTime(&t)){
    openCompartment(t.tm_hour);
  }

  if(deviceConnected && notifyEnabled){
    pCharacteristic->setValue("Take medicine");
    pCharacteristic->notify();
  }
}

/* ================= STOP ================= */
void stopReminder(){

  Serial.println("Reminder Stopped");

  reminderActive=false;

  digitalWrite(LED,LOW);
  digitalWrite(BUZZER,LOW);

  moveServo(lidServo,LID_SERVO_PIN,90,currentLidAngle);
  moveServo(compartmentServo,COMPARTMENT_SERVO_PIN,0,currentCompartmentAngle);
}

/* ================= FETCH TIMES ================= */
/* ================= FETCH TIMES ================= */
void fetchTimes(){

  checkWiFi();
  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("No WiFi");
    return;
  }

  Serial.println("Fetching schedules...");

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  char url[300];
  snprintf(url, sizeof(url),
  "https://firestore.googleapis.com/v1/projects/%s/databases/(default)/documents/users/%s/schedules",
  projectId, userId);

  if(!http.begin(client,url)){
    Serial.println("HTTP Begin Failed");
    return;
  }

  int code=http.GET();

  if(code!=200){
    Serial.print("HTTP Error: ");
    Serial.println(code);
    http.end();
    return;
  }

  String payload=http.getString();

  Serial.println("===== RAW FIREBASE RESPONSE =====");
  Serial.println(payload);
  Serial.println("=================================");

  http.end();

  StaticJsonDocument<4000> doc;

  DeserializationError error = deserializeJson(doc,payload);

  if(error){
    Serial.print("JSON Error: ");
    Serial.println(error.c_str());
    return;
  }

  if(!doc.containsKey("documents")){
    Serial.println("No schedule documents found");
    return;
  }

  timeCount = 0;

  JsonArray documents = doc["documents"];

  for(JsonObject schedule : documents){

    /* ---- SAFE BOOLEAN READING ---- */

    bool isActive = true;          // default TRUE if missing
    bool reminderEnabled = false;  // default FALSE

    if(schedule["fields"]["isActive"]["booleanValue"].is<bool>()){
      isActive = schedule["fields"]["isActive"]["booleanValue"];
    }

    if(schedule["fields"]["reminderEnabled"]["booleanValue"].is<bool>()){
      reminderEnabled = schedule["fields"]["reminderEnabled"]["booleanValue"];
    }

    Serial.print("isActive: ");
    Serial.println(isActive);

    Serial.print("reminderEnabled: ");
    Serial.println(reminderEnabled);

    if(!isActive || !reminderEnabled){
      Serial.println("Schedule skipped");
      continue;
    }

    /* ---- SAFE TIMES READING ---- */

    if(!schedule["fields"]["times"]["arrayValue"]["values"].is<JsonArray>()){
      Serial.println("No times array");
      continue;
    }

    JsonArray times =
      schedule["fields"]["times"]["arrayValue"]["values"];

    for(JsonObject t : times){

      if(timeCount >= 10) break;

      const char* val = t["stringValue"];

      if(!val) continue;

      int hour=0, minute=0;

      // Supports both "2:50" and "02:50"
      if(sscanf(val,"%d:%d",&hour,&minute)==2){

        sprintf(reminderTimes[timeCount], "%02d:%02d", hour, minute);

        Serial.print("Loaded Time: ");
        Serial.println(reminderTimes[timeCount]);

        timeCount++;
      }
    }
  }

  Serial.print("Total Active Reminder Times: ");
  Serial.println(timeCount);
}
/* ================= CHECK TIME ================= */
void checkTime(){

  struct tm t;
  if(!getLocalTime(&t)) return;

  int now=t.tm_hour*60+t.tm_min;

  static int lastMinute=-1;
  if(now!=lastMinute){
    lastTriggered=-1;
    lastMinute=now;
  }

  for(int i=0;i<timeCount;i++){

    int h=(reminderTimes[i][0]-'0')*10+(reminderTimes[i][1]-'0');
    int m=(reminderTimes[i][3]-'0')*10+(reminderTimes[i][4]-'0');

    int r=h*60+m;

    if(now==r && lastTriggered!=r){
      startReminder();
      lastTriggered=r;
    }
  }
}

/* ================= SEND LOG ================= */
void sendLog(const char* status){

  checkWiFi();
  if(WiFi.status()!=WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  struct tm t;
  if(!getLocalTime(&t)) return;

  char timeStr[30];
  strftime(timeStr,sizeof(timeStr),"%Y-%m-%dT%H:%M:%S",&t);

  char url[300];
  snprintf(url,sizeof(url),
"https://firestore.googleapis.com/v1/projects/%s/databases/(default)/documents/users/%s/logs",
projectId,userId);

  StaticJsonDocument<300> doc;
  doc["fields"]["timestamp"]["stringValue"]=timeStr;
  doc["fields"]["status"]["stringValue"]=status;

  char body[400];
  serializeJson(doc,body);

  if(!http.begin(client,url)) return;

  http.addHeader("Content-Type","application/json");
  int code=http.POST((uint8_t*)body,strlen(body));

  if(code==200||code==201){
    Serial.println("Log Sent: "+String(status));
  }else{
    Serial.print("Log Error:");
    Serial.println(code);
  }

  http.end();
}

/* ================= BLE ================= */
class ServerCallbacks: public NimBLEServerCallbacks{
  void onConnect(NimBLEServer*,NimBLEConnInfo&) override{
    deviceConnected=true;
    Serial.println("BLE Connected");
  }
  void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int) override{
    deviceConnected=false;
    notifyEnabled=false;
    Serial.println("BLE Disconnected");
    NimBLEDevice::startAdvertising();
  }
};

class CharCallbacks: public NimBLECharacteristicCallbacks{
  void onSubscribe(NimBLECharacteristic*,NimBLEConnInfo&,uint16_t subValue) override{
    notifyEnabled=(subValue>0);
  }
};

ServerCallbacks serverCB;
CharCallbacks charCB;
/* ================= REFILL ================= */
void handleRefill(){

  Serial.println("Refill Button Pressed");

  // 1️⃣ Open Lid First
  moveServo(lidServo, LID_SERVO_PIN, 0, currentLidAngle);
  delay(1000);

  // 2️⃣ Open All Compartments One by One

  // Morning Compartment (0°)
  moveServo(compartmentServo, COMPARTMENT_SERVO_PIN, 0, currentCompartmentAngle);
  delay(2000);

  // Afternoon Compartment (90°)
  moveServo(compartmentServo, COMPARTMENT_SERVO_PIN, 90, currentCompartmentAngle);
  delay(2000);

  // Night Compartment (180°)
  moveServo(compartmentServo, COMPARTMENT_SERVO_PIN, 180, currentCompartmentAngle);
  delay(2000);

  // Return Compartment to Default (0°)
  moveServo(compartmentServo, COMPARTMENT_SERVO_PIN, 0, currentCompartmentAngle);
  delay(1000);

  // 3️⃣ Close Lid
  moveServo(lidServo, LID_SERVO_PIN, 90, currentLidAngle);

  // 4️⃣ BLE Notification
  if(deviceConnected && notifyEnabled){
    pCharacteristic->setValue("Medicine Refilled");
    pCharacteristic->notify();
  }

  Serial.println("Refill Completed");
}

/* ================= SETUP ================= */
void setup(){

  Serial.begin(115200);

  pinMode(LED,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  pinMode(BUTTON,INPUT_PULLUP);
  pinMode(REFILL_BUTTON,INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  checkWiFi();
  setupTime();
  fetchTimes();

  NimBLEDevice::init("MedESP32");
  NimBLEServer* s=NimBLEDevice::createServer();
  s->setCallbacks(&serverCB);

  NimBLEService* service=s->createService(SERVICE_UUID);

  pCharacteristic=service->createCharacteristic(
  CHARACTERISTIC_UUID,
  NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::NOTIFY);

  pCharacteristic->setCallbacks(&charCB);

  service->start();
  NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE Advertising Started");
}

/* ================= LOOP ================= */
void loop(){

  checkTime();

  if(reminderActive){

    unsigned long now=millis();

    if(now-lastAlertMillis>=60000){
      lastAlertMillis=now;
      digitalWrite(LED,HIGH);
      digitalWrite(BUZZER,HIGH);
      Serial.println("Re-alert");
    }

    if(now-reminderStartMillis>=180000 && !missedLogged){
      missedLogged=true;
      reminderActive=false;
      digitalWrite(LED,LOW);
      digitalWrite(BUZZER,LOW);
      sendLog("missed");
      Serial.println("Missed Logged");
    }
  }

  if(millis()-lastFetch>60000){
    fetchTimes();
    lastFetch=millis();
  }

  if(digitalRead(BUTTON)==LOW && millis()-lastButtonPress>1000){
    lastButtonPress=millis();
    stopReminder();
    sendLog("taken");
  }
    // REFILL BUTTON
  if(digitalRead(REFILL_BUTTON)==LOW && millis()-lastButtonPress>1000){
    lastButtonPress=millis();
    handleRefill();
  }

  delay(10);
}
