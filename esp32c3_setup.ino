void setup(){
  Serial.begin(115200);

  // wait for USB serial (important ESP32-C3)
  unsigned long start = millis();
  while(!Serial && millis()-start < 6000){}

  delay(1000);
  Serial.println("HELLO SMRS");
}                                                             

void loop(){
  Serial.println("RUNNING...");
  delay(2000);
}
