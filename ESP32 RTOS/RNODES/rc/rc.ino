/* ==========================================
   PROJECT: Node C - GSM Gateway (RTOS Version)
   BOARD:   AM036 (ESP32 T-Call SIM800) + External LoRa
   ROLE:    Receive LoRa -> Send ACK -> Send SMS
   ========================================== */

#define TINY_GSM_MODEM_SIM800

#include <Wire.h>
#include <TinyGsmClient.h>
#include <SPI.h>
#include <LoRa.h>

// --- USER CONFIG ---
#define SMS_TARGET ""  // <--- PUT YOUR PHONE NUMBER HERE
#define LORA_BAND  433E6           

// --- PIN DEFINITIONS (AM036 / T-Call) ---
#define MODEM_RST      5
#define MODEM_PWKEY    4
#define MODEM_POWER_ON 23
#define MODEM_TX       27
#define MODEM_RX       26
#define I2C_SDA        21
#define I2C_SCL        22

// --- EXTERNAL LORA PINS ---
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   25 
#define LORA_CS     13  
#define LORA_RST    14
#define LORA_DIO0   34

// --- OBJECTS & RTOS ---
#define SerialMon Serial
#define SerialAT  Serial1
TinyGsm modem(SerialAT);

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

QueueHandle_t smsQueue;

// A safe box to pass text through the RTOS queue
struct SmsMessage {
  char text[255];
};

// Power Management for T-Call
bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(en ? 0x37 : 0x35);
  return Wire.endTransmission() == 0;
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

// ==========================================
// TASK 1: LORA RECEIVER & ACK SENDER
// ==========================================
void TaskReadLoRa(void *pvParameters) {
  SmsMessage newMsg;

  while(1) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String incoming = "";
      while (LoRa.available()) {
        incoming += (char)LoRa.read();
      }
      
      String cleanData = "";
      
      // --- CRITICAL CRC CHECK ---
      if (validatePacket(incoming, cleanData)) {
        SerialMon.println("CRC Match! Valid Data Received.");

        // 1. SEND ACK TO NODE B
        delay(50); 
        LoRa.beginPacket();
        LoRa.print("ACK_N1"); // Send confirmation
        LoRa.endPacket();

        // 2. FORMAT THE CLEAN DATA FOR SMS
        // Use 'cleanData' here, which has the |CRC stripped off
        cleanData.replace("o2:", "O2: ");
        cleanData.replace(" co2:", "%\nCO2: ");
        // ... (rest of your .replace() formatting)

        // 3. SEND TO SMS QUEUE
        strncpy(newMsg.text, cleanData.c_str(), sizeof(newMsg.text) - 1);
        newMsg.text[sizeof(newMsg.text) - 1] = '\0'; 
        
        xQueueSend(smsQueue, &newMsg, portMAX_DELAY);
        
      } else {
        SerialMon.println("CRC FAILED! Data corrupted. Ignored.");
        // No ACK sent. Node B will try again.
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

// ==========================================
// TASK 2: SMS SENDER (SIM800)
// ==========================================
void TaskSendSMS(void *pvParameters) {
  SmsMessage receivedMsg;

  while(1) {
    // Task sleeps here until a message arrives in the queue
    if (xQueueReceive(smsQueue, &receivedMsg, portMAX_DELAY) == pdTRUE) {
      
      SerialMon.println("Preparing to send SMS...");
      SerialMon.println(receivedMsg.text);
      
      // Send the SMS
      if (modem.sendSMS(SMS_TARGET, String(receivedMsg.text))) {
        SerialMon.println("SMS sent successfully!");
      } else {
        SerialMon.println("SMS failed to send.");
      }
    }
  }
}

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  // 1. Initialize Power
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isIP5306Node = setPowerBoostKeepOn(1);
  SerialMon.println(isIP5306Node ? "IP5306 Power ON" : "IP5306 Fail");

  // 2. Initialize Modem (SIM800)
  SerialMon.println("Init Modem...");
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  SerialMon.println("Restarting Modem...");
  modem.restart(); 
  SerialMon.println("Modem Ready.");

  // 3. Initialize LoRa
  SerialMon.println("Init LoRa...");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(LORA_BAND)) {
    SerialMon.println("LoRa Init Failed! Check Wiring.");
    while (1);
  }
  
  LoRa.setSpreadingFactor(7);      
  LoRa.setSignalBandwidth(125E3);  
  LoRa.setCodingRate4(5);          
  LoRa.setSyncWord(0x12);          
  LoRa.setPreambleLength(8);       
  
  SerialMon.println("Node C Ready: Listening for LoRa...");

  // 4. Create RTOS Queue & Tasks
  smsQueue = xQueueCreate(5, sizeof(SmsMessage));
  
  // LoRa needs High priority (2) to catch fast radio waves. SMS gets Normal priority (1).
  xTaskCreate(TaskReadLoRa, "LoRaTask", 8192, NULL, 2, NULL);
  xTaskCreate(TaskSendSMS, "SMSTask", 8192, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL);
}