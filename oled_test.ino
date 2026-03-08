#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1   // no reset pin
#define I2C_SDA 6
#define I2C_SCL 7

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);

  // Start I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("OLED Test Starting...");

  // OLED address usually 0x3C
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED not detected");
    while (1);
  }

  Serial.println("✅ OLED detected successfully");

  display.clearDisplay();

  // Test content
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32-C3 OLED Test");
  display.println("-------------------");
  display.println("OLED Working OK!");
  display.println("I2C Interface OK");
  display.println("0.96 inch Display");
  display.println("Status: SUCCESS");

  display.display();
}

void loop() {
  // Animation test
  static int x = 0;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("OLED Animation Test");

  display.fillCircle(x, 40, 4, SSD1306_WHITE);
  display.display();

  x++;
  if (x > 127) x = 0;

  delay(50);
}