/* ==========================================
   PROJECT: Node B - Repeater
   BOARD:   Heltec WiFi LoRa 32 V3
   ROLE:    RX from Node A -> Wait -> TX to Node C
   ========================================== */

#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

#define RF_FREQUENCY 433000000
#define LORA_BANDWIDTH 0       // 125 kHz
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1      // 4/5
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

// -------- Callbacks --------
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

// -------- Power --------
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
  display.drawString(0,0,"Node B Repeater Init...");
  display.display();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  Radio.SetTxConfig(MODEM_LORA, 17, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // IMPORTANT: match Node A
  Radio.SetPublicNetwork(true);

  Serial.println("Node B: Listening...");
  display.drawString(0, 15, "Listening...");
  display.display();

  Radio.Rx(0); 
}



// ================= LOOP =================
void loop() {
  Radio.IrqProcess();

  if (packetReceived) {
    packetReceived = false;
    
    Serial.println("\n[RX] Msg Received!");
    Serial.print("Payload: "); Serial.println(rxpacket);
    Serial.print("RSSI: "); Serial.println(last_rssi);

    display.clear();
    display.drawString(0, 0, "RX OK | RSSI: " + String(last_rssi));
    display.drawString(0, 15, "Relaying soon...");
    display.display();

    // Stop radio before TX
    Radio.Sleep();

    // Delay before relay (example: 5s, adjust as needed)
    delay(5000);

    // Prepare to send
    txDone = false;
    Radio.Send((uint8_t *)rxpacket, strlen(rxpacket));
    
    // Wait for TX finish
    unsigned long startTx = millis();
    while(!txDone && millis() - startTx < 4000) {
      Radio.IrqProcess();
    }
    
    // Back to RX
    Serial.println("Back to listening...");
    display.clear();
    display.drawString(0, 0, "Listening again...");
    display.display();
    Radio.Rx(0);
  }
}
