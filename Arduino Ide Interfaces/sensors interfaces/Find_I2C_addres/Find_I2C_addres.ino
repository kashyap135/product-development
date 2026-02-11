#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(41, 42); // SDA=41, SCL=42 for Heltec ESP32-S3

  Serial.println("Scanning I2C bus...");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found.");
  } else {
    Serial.println("Scan complete.");
  }

  delay(5000); // repeat every 5s
}