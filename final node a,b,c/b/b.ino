#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HT_SSD1306Wire.h" // Required for onboard OLED

// ---------------- CONFIGURATION ----------------
#define RF_FREQUENCY          433000000 
#define TX_OUTPUT_POWER       14
#define LORA_BANDWIDTH        0     // 125 kHz
#define LORA_SPREADING_FACTOR 7     //
#define LORA_CODINGRATE       1     // 4/5
#define LORA_PREAMBLE_LENGTH  8     //
#define LORA_SYMBOL_TIMEOUT   0
#define LORA_IQ_INVERSION_ON  false //

#define BUFFER_SIZE 255             
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;

// Initialize the OLED display (Onboard Heltec V3 uses 0x3c)
extern SSD1306Wire  display; 

void OnTxDone(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); // LOW powers ON the OLED and sensors
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  // 1. Power on and Initialize OLED
  VextON();
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.flipScreenVertically();
  display.clear();
  display.drawString(0, 0, "NODE B RELAY");
  display.drawString(0, 12, "Listening...");
  display.display();

  // 2. Initialize Radio Callbacks
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, false, true, 0, 0, 
                    LORA_IQ_INVERSION_ON, 3000);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, false, 0, true, 0, 0, 
                    LORA_IQ_INVERSION_ON, true);

  Radio.Rx(0); 
  Serial.println("Node B Relay Listening...");
}

void loop() {
  Radio.IrqProcess(); 
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';

  // Update OLED with Received Data
  display.clear();
  display.drawString(0, 0, "RECEIVED FROM A:");
  display.drawString(0, 12, String(rxpacket));
  display.drawString(0, 52, "RSSI: " + String(rssi));
  display.display();

  Serial.printf("\nReceived from Node A: %s (RSSI: %d)\n", rxpacket, rssi);

  delay(500); // Small delay to avoid collision
  Radio.Send((uint8_t *)rxpacket, strlen(rxpacket));
}

void OnTxDone(void) {
  // Update OLED with Forwarding Status
  display.drawString(0, 32, "FORWARDED TO C");
  display.display();
  
  Serial.println("Forwarded Successfully.");
  Radio.Rx(0); // Return to RX mode
}