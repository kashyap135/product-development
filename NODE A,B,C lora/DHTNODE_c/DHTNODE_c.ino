#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "DHT.h"
#include "HT_SSD1306Wire.h"

// ---------------- PIN DEFINITIONS ----------------
#define DHTPIN 6        // Safe GPIO for DHT11 data
#define DHTTYPE DHT11
#define Vext 36         // Power control for onboard sensors/OLED

DHT dht(DHTPIN, DHTTYPE);

// ---------------- LORA CONFIG ----------------
#define RF_FREQUENCY        433000000 // India ISM band
#define TX_OUTPUT_POWER     5
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE         50

char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
void OnTxDone(void);//to check transmit is done or not 
void OnTxTimeout(void);// to check timeout lora

// Heltec library already defines the OLED object
extern SSD1306Wire display;

void VextON() {
  pinMode(Vext, OUTPUT);//on the power on board oled/sensors
  digitalWrite(Vext, LOW); // LOW = power ON
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);//start serial commuciation to leltech board

  // Initialize LoRa radio
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
  display.drawString(0, 0, "Node C Ready");
  display.drawString(0, 15, "LoRa + DHT11 Sensor");
  display.display();

  // Initialize DHT sensor
  dht.begin();
}

void loop() {
  //if lora idle is true to read the dht sensor value
  if (lora_idle == true) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    
      
    if (!isnan(h) && !isnan(t)) {
      // Prefix with Node C identifier
      sprintf(txpacket, "TempC:%.1f HumC:%.1f", t, h);
      Serial.printf("\r\nSending: %s\r\n", txpacket);
      //node c temp and hum data values show in oled display
      display.clear();
      display.drawString(0, 0, "Node C TX...");
      display.drawString(0, 30, txpacket);
      display.display();
      //to send the another lora tx packet to node b
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      delay(5000);
      lora_idle = false;
      //after sending data lora idle get false
    } else {
      Serial.println("Sensor read error!");
    }
  }
  Radio.IrqProcess();
}

void OnTxDone(void) {
  display.clear();
  display.drawString(0, 0, "Node C Sent OK");
  display.display();
  delay(5000);
  Serial.println("Node C sent successfully!");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Serial.println("Node C TX Timeout!");
  display.clear();
  display.drawString(0, 0, "Node C Timeout");
  display.display();
  Radio.Sleep();
  lora_idle = true;
}