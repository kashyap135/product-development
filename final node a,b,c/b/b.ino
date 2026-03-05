#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// ---------------- LORA CONFIG ----------------
#define RF_FREQUENCY        433000000
#define TX_OUTPUT_POWER     17
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 9
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE         255

#define Vext 36

char rxpacket[BUFFER_SIZE];
char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;

void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

extern SSD1306Wire display;

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void setup() {

  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  // LoRa Setup
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  Radio.Rx(0);

  // OLED
  VextON();
  delay(300);
  display.init();
  display.setFont(ArialMT_Plain_10);

  display.clear();
  display.drawString(0,0,"Node B Ready");
  display.drawString(0,15,"LoRa Relay");
  display.display();
}

void loop() {
  Radio.IrqProcess();
}

// ---------------- RECEIVE DATA ----------------

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {

  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';

  Serial.printf("\nNode B Received: %s\n", rxpacket);

  display.clear();
  display.drawString(0,0,"Node B RX");
  display.drawString(0,20,rxpacket);
  display.display();

  // Forward same packet
  strcpy(txpacket, rxpacket);

  Serial.printf("Forwarding to Node c: %s\n", txpacket);

  display.drawString(0,40,"Forwarding...");
  display.display();

  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  lora_idle = false;
}

// ---------------- TX DONE ----------------

void OnTxDone(void) {

  Serial.println("Node B forwarded successfully!");

  display.clear();
  display.drawString(0,0,"Forwarded OK");
  display.display();

  lora_idle = true;
  Radio.Rx(0);
}

// ---------------- TX TIMEOUT ----------------

void OnTxTimeout(void) {

  Serial.println("Node B TX Timeout!");

  display.clear();
  display.drawString(0,0,"TX Timeout");
  display.display();

  Radio.Sleep();
  lora_idle = true;
  Radio.Rx(0);
}