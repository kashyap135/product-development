#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

Adafruit_ADS1115 ads;
extern SSD1306Wire display;

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);   // LOW = ON
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  VextON();
  delay(200);

  // Initialize I2C once with correct pins
  Wire.begin(41, 42);  // SDA=41, SCL=42

  // OLED init (uses same Wire bus)
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Heltec + ADS1115 + H2S");
  display.display();

  // ADS1115 init (same Wire bus)
  if (!ads.begin(0x48, &Wire)) {
    Serial.println("Failed to initialize ADS1115!");
    display.clear();
    display.drawString(0, 0, "ADS1115 init failed!");
    display.display();
    while (1);
  }
  ads.setGain(GAIN_ONE); // ±4.096V range
  Serial.println("ADS1115 initialized successfully!");
}

void loop() {
  int16_t adc0 = ads.readADC_SingleEnded(0);
  float voltage = adc0 * 0.125 / 1000.0;

  Serial.print("H2S Sensor Voltage: ");
  Serial.print(voltage, 3);
  Serial.println(" V");

  display.clear();
  display.drawString(0, 0, "H2S Voltage:");
  display.drawString(70, 0, String(voltage, 3) + " V");
  display.display();

  delay(2000);
}