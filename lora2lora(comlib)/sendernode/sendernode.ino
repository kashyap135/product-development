#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <DHT.h>

// -------- DHT ----------
#define DHTPIN 6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------- OLED ----------
extern SSD1306Wire display;
#define Vext 36

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

// -------- SX1262 (Heltec V3) --------
#define LORA_CS   8
#define LORA_DIO1 14
#define LORA_RST  12
#define LORA_BUSY 13

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

void setup() {
  Serial.begin(115200);

  // OLED power
  VextON();
  delay(200);

  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0,0,"Heltec Sender");
  display.display();

  dht.begin();

  Serial.println("Init SX1262...");

  int state = radio.begin(433.0,125.0,7,5,0x34,14,8);

  if (state != RADIOLIB_ERR_NONE) {
    display.clear();
    display.drawString(0,0,"LoRa FAIL");
    display.display();
    Serial.println(state);
    while(true);
  }

  display.drawString(0,15,"LoRa OK");
  display.display();
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    String msg = "Temp:" + String(t,1) + " Hum:" + String(h,1);

    display.clear();
    display.drawString(0,0,"Sending:");
    display.drawString(0,20,msg);
    display.display();

    Serial.println(msg);

    radio.transmit(msg);
  }

  delay(5000);
}
