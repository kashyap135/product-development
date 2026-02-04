#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "DHT.h"
#include "HT_SSD1306Wire.h"

// 1. PIN DEFINITIONS
#define DHTPIN 6   // Use Pin 13 to avoid conflict with OLED (Pin 4)
#define DHTTYPE DHT11
#define Vext 36     // Power control for onboard sensors/OLED

DHT dht(DHTPIN, DHTTYPE);

// 2. LORA CONFIG
#define RF_FREQUENCY 865000000 
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 50

char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

// FIX: Do NOT define 'SSD1306Wire display(...)' here. 
// The Heltec library already did it. We just reference it:
extern SSD1306Wire display; 

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  // Initialize LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // Initialize OLED
  VextON();
  delay(300);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();

display.display();
  display.drawString(0, 0, "Heltec LoRa Ready");
  display.display();

  dht.begin();
}

void loop() {
  if (lora_idle == true) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      sprintf(txpacket, "Temp:%.1f Hum:%.1f", t, h);
      Serial.printf("\r\nSending: %s\r\n", txpacket);

      display.clear();
      display.drawString(0, 0, "Sending LoRa...");
      display.drawString(0, 20, txpacket);
      display.display();

      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      delay(5000);
      lora_idle = false;
    }
  }
  Radio.IrqProcess();
}

void OnTxDone(void) {
  Serial.println("Sent successfully!");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  lora_idle = true;
}