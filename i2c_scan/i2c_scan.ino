#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(22, 27);   // SDA = IO22, SCL = IO27
}

void loop() {
  Serial.println("Scanning...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found device at 0x%02X\n", addr);
    }
  }
  delay(2000);
}