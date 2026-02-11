#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

void setup() {
  Serial.begin(115200);

  // Use the same pins that worked in your scanner
  Wire.begin(41, 42);  // SDA=18, SCL=17

  // Pass Wire object explicitly
  if (!ads.begin(0x48, &Wire)) {
    Serial.println("Failed to initialize ADS1115!");
    while (1);
  }
  Serial.println("ADS1115 initialized successfully!");
}
void loop()
{
  
}