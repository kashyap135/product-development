#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

#define Vext 36   // Heltec Vext pin
#define SEALEVELPRESSURE_HPA (1013.25)

// SPI pins
#define BME_CS   34
#define BME_MOSI 35
#define BME_MISO 37
#define BME_SCK  36

// Create BME680 object (SPI)
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);

// ---- Vext power control ----
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);   // LOW = ON
}

void VextOFF() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);  // HIGH = OFF
}

// ---- Use Heltec's OLED object ----
extern SSD1306Wire display;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Power OLED + sensor
  VextON();
  delay(100);

  // OLED init
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Heltec + BME680 (SPI)");
  display.display();

  // BME680 init
  if (!bme.begin()) {
    Serial.println("Could not find BME680 sensor!");
    display.drawString(0, 12, "BME680 ERROR");
    display.display();
    while (1) delay(1000);
  }

  // Configure oversampling & heater
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms
}

void loop() {
  if (!bme.performReading()) {
    Serial.println("Failed to read BME680!");
    return;
  }

  // Collect data
  float temp = bme.temperature;
  float hum = bme.humidity;
  float pres = bme.pressure / 100.0; // hPa
  float gas = bme.gas_resistance / 1000.0; // KOhms
  float alt = bme.readAltitude(SEALEVELPRESSURE_HPA);

  // Serial output
  Serial.print("Temp: "); Serial.print(temp); Serial.println(" °C");
  Serial.print("Hum: "); Serial.print(hum); Serial.println(" %");
  Serial.print("Pres: "); Serial.print(pres); Serial.println(" hPa");
  Serial.print("Gas: "); Serial.print(gas); Serial.println(" KOhms");
  Serial.print("Alt: "); Serial.print(alt); Serial.println(" m");

  // OLED output
  display.clear();
  display.drawString(0, 0, "Temp: " + String(temp, 1) + " C");
  display.drawString(0, 12, "Hum: " + String(hum, 1) + " %");
  display.drawString(0, 24, "Pres: " + String(pres, 1) + " hPa");
  display.drawString(0, 36, "Gas: " + String(gas, 1) + " KOhm");
  display.drawString(0, 48, "Alt: " + String(alt, 1) + " m");
  display.display();

  delay(3000);
}