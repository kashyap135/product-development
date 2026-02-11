#include <Wire.h>
#include "DHT.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"   // Heltec core (Vext, display)
#include<SPI.h>
#define DHTPIN 4          // SAFE GPIO
#define DHTTYPE DHT11
#define Vext 36

DHT dht(DHTPIN, DHTTYPE);

// ---- Vext power control (YOU must define this) ----
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

  // Power OLED
  VextON();
  delay(100);

  // OLED init
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawString(0, 0, "Heltec + DHT11");
  display.display();

  // DHT init
  dht.begin();
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature(); // Celsius

  display.clear();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("DHT read failed!");

    display.drawString(0, 0, "DHT ERROR");
    display.drawString(0, 12, "Check wiring");
    display.display();
    delay(2000);
    return;
  }

  // Serial output
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  // OLED output
  display.drawString(0, 0, "Humidity:");
  display.drawString(70, 0, String(humidity, 1) + " %");

  display.drawString(0, 16, "Temperature:");
  display.drawString(70, 16, String(temperature, 1) + " C");

  display.display();

  delay(3000);
}
