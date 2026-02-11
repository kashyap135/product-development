#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"   // Heltec core (Vext, display)
#include<SPI.h>
int sensorPin = 34;
//int snrvalue = 0;

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

  display.drawString(0, 0, "Heltec + H2S");
  display.display();
}

void loop() {
  int snrvalue = analogRead(sensorPin);

  Serial.print("Sensor Value: ");
  Serial.println(snrvalue);

  display.clear();
  display.drawString(0, 0, "Sensor Value: " + String(snrvalue));
  display.display();

  delay(1000);
}
