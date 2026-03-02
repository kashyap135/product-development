/* ==========================================
   PROJECT: Node B - FIXED Repeater (RTOS Version)
   BOARD:   Heltec WiFi LoRa 32 V3
   ROLE:    RX from A -> Wait 15s -> TX to C
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

// Event Bits
#define TX_DONE_BIT   (1 << 0)
#define RX_DONE_BIT   (1 << 1)
#define TX_FAIL_BIT   (1 << 2)

// RTOS Handles
EventGroupHandle_t radioEvents;
QueueHandle_t repeaterQueue;

struct RelayMessage {
  char payload[BUFFER_SIZE];
  int16_t rssi;
};

extern SSD1306Wire display;
static RadioEvents_t RadioEvents;

// Forward Declarations of functions and tasks
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnTxDone(void);
void OnTxTimeout(void);
void TaskRadioIRQ(void *pvParameters);
void TaskRelayData(void *pvParameters);

void VextON() {
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext,LOW);
}

// 1. The Math Function (Must match Node A exactly)
uint8_t calculateCRC(const char *data) {
  uint8_t crc = 0;
  while (*data) crc ^= *data++;
  return crc;
}

// 2. The Packet Inspector
// Returns TRUE if the packet is safe, and extracts the clean data.
bool validatePacket(String incomingPacket, String &cleanData) {
  int pipeIndex = incomingPacket.lastIndexOf('|');
  
  // If there is no '|', it's an invalid or corrupted format
  if (pipeIndex == -1) return false; 

  cleanData = incomingPacket.substring(0, pipeIndex);
  String crcString = incomingPacket.substring(pipeIndex + 1);
  
  // Convert the Hex string (e.g., "A5") back into an integer
  uint8_t receivedCRC = (uint8_t) strtol(crcString.c_str(), NULL, 16);
  uint8_t calculatedCRC = calculateCRC(cleanData.c_str());

  // If the math matches, the packet is 100% uncorrupted!
  return receivedCRC == calculatedCRC;
}



void setup() {
  Serial.begin(115200);
  VextON();
  delay(100);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,"Repeater Ready");
  display.display();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  // Initialize Radio Callbacks
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
  Radio.SetTxConfig(MODEM_LORA, 14, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
                    
  Serial.println("Node B: Initializing RTOS...");
  
  // Create RTOS Tools
  radioEvents = xEventGroupCreate();
  repeaterQueue = xQueueCreate(5, sizeof(RelayMessage));

  // Create Tasks
  xTaskCreate(TaskRadioIRQ, "RadioIRQ", 4096, NULL, 2, NULL); 
  xTaskCreate(TaskRelayData, "RelayTask", 8192, NULL, 1, NULL);

  // Start listening!
  Radio.Rx(0); 
}



void loop() {
  // The loop is dead! Long live RTOS!
  vTaskDelete(NULL);
}

// ==========================================
// RADIO INTERRUPT CALLBACKS
// ==========================================
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  RelayMessage incomingMsg;
  memcpy(incomingMsg.payload, payload, size);
  incomingMsg.payload[size] = '\0';
  incomingMsg.rssi = rssi;

  // Send the message to the waiting queue safely from an Interrupt
  xQueueSendFromISR(repeaterQueue, &incomingMsg, NULL);
  
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(radioEvents, RX_DONE_BIT, &xHigherPriorityTaskWoken);
}

void OnTxDone(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(radioEvents, TX_DONE_BIT, &xHigherPriorityTaskWoken);
}

void OnTxTimeout(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(radioEvents, TX_FAIL_BIT, &xHigherPriorityTaskWoken);
}

// ==========================================
// RTOS TASKS
// ==========================================
void TaskRadioIRQ(void *pvParameters) {
  while(1) {
    Radio.IrqProcess();
    vTaskDelay(pdMS_TO_TICKS(10)); // Keep radio alive in background
  }
}

void TaskRelayData(void *pvParameters) {
  RelayMessage receivedMsg;

  while(1) {
    if (xQueueReceive(repeaterQueue, &receivedMsg, portMAX_DELAY) == pdTRUE) {
      
      String incomingPacket = String(receivedMsg.payload);
      String cleanData = "";

      // --- CRITICAL CRC CHECK ---
      if (validatePacket(incomingPacket, cleanData)) {
        Serial.println("CRC Match! Packet is valid.");
        
        // 1. SEND ACK TO NODE A
        char ackPacket[] = "ACK_N1"; 
        Radio.Send((uint8_t *)ackPacket, strlen(ackPacket));
        xEventGroupWaitBits(radioEvents, TX_DONE_BIT | TX_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(3000));

        // 2. HOLD FOR 15s
        vTaskDelay(pdMS_TO_TICKS(15000)); 

        // 3. RELAY ORIGINAL PACKET TO NODE C
        Radio.Send((uint8_t *)receivedMsg.payload, strlen(receivedMsg.payload));
        // ... (rest of the wait for Node C ACK logic)
        
      } else {
        // PACKET CORRUPTED!
        Serial.println("CRC FAILED! Dropping packet.");
        // We do NOT send an ACK. Node A will time out and resend it automatically!
      }
      
      Radio.Rx(0); // Back to listening
    }
  }
}