/* ==========================================
   PROJECT: Node B - FIXED Repeater
   BOARD:   Heltec WiFi LoRa 32 V3
   ROLE:    RX from A -> Wait 50s -> TX to C
   FIXES:   Sync Word 0x12, Radio Sleep Reset
   ========================================== */

#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

#define RF_FREQUENCY 433E6  
#define LORA_BANDWIDTH 0       
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1      
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 255

// OLED Pins
#define Vext 36 
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21

extern SSD1306Wire display;

char rxpacket[BUFFER_SIZE]; 
int16_t last_rssi = 0;
bool packetReceived = false;
bool txDone = true; 

static RadioEvents_t RadioEvents;
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnTxDone(void);
void OnTxTimeout(void);

void VextON() {
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext,LOW);
}

void setup() {
  Serial.begin(115200);
  VextON();
  delay(100);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,"Repeater Fix...");
  display.display();

Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  // CRITICAL FIX 1: Force Private Network (0x12)
  // This makes it talk the same language as Node C
 // Radio.SetPublicNetwork(false);

  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  
  // RX Config
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  // TX Config
  // CRITICAL FIX 2: Lower Power to 14 to prevent brownouts
  Radio.SetTxConfig(MODEM_LORA, 14, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
                    
  Serial.println("Node B: Listening...");
  Radio.Rx(0); 
}

void loop() {
  Radio.IrqProcess();

  if (packetReceived) {
    packetReceived = false;
    
    Serial.println("\n[RX] Msg Received!");
    
    display.clear();
    display.drawString(0, 0, "RX OK | RSSI: " + String(last_rssi));
    display.drawString(0, 15, "Waiting 50s...");
    display.display();

    // 1. STOP RADIO (Fixes the state machine)
    Radio.Sleep();

    // 2. The 50s Delay
    Serial.print("Holding.");
    for(int i=0; i<15; i++) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nResending now...");

    // 3. Update Screen
    display.clear();
    display.drawString(0, 0, "Relaying to Node C...");
    display.display();
    
    // 4. PREPARE TO SEND
    // CRITICAL FIX 3: Re-force network type and small delay
    Radio.SetPublicNetwork(false); 
    delay(50);
    
    txDone = false;
    Radio.Send((uint8_t *)rxpacket, strlen(rxpacket));
    
    // 5. Wait for TX finish
    unsigned long startTx = millis();
    while(!txDone && millis() - startTx < 4000) {
      Radio.IrqProcess();
    }
    
    // 6. Back to RX
    Serial.println("Back to listening...");
    Radio.Rx(0);
  }
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0'; 
  last_rssi = rssi;
  packetReceived = true;
}

void OnTxDone(void) {
  Serial.println("[TX] Success! Sent to Node C.");
  txDone = true;
}

void OnTxTimeout(void) {
  Serial.println("[TX] ERROR: Radio Timeout!");
  txDone = true;
}