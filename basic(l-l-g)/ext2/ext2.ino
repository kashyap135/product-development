
#define TINY_GSM_MODEM_SIM800

#include <Wire.h>
#include <TinyGsmClient.h>
#include <SPI.h>
#include <LoRa.h>

// --- USER CONFIG ---
#define SMS_TARGET "+918247567314" // Your Phone Number
#define LORA_BAND  865E6           // Match your LoRa module (433E6, 868E6, 915E6)

// --- PIN DEFINITIONS (AM036 / T-Call) ---
#define MODEM_RST      5
#define MODEM_PWKEY    4
#define MODEM_POWER_ON 23
#define MODEM_TX       27
#define MODEM_RX       26
#define I2C_SDA        21
#define I2C_SCL        22

// --- LORA PINS (CUSTOM) ---
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   25  // Using GPIO 25 instead of 23
#define LORA_CS     13  // Using GPIO 13 instead of 5
#define LORA_RST    14
#define LORA_DIO0   34

// --- LIBRARIES ---
#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT  Serial1
TinyGsm modem(SerialAT);

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

// Power Management for T-Call
bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(en ? 0x37 : 0x35);
  return Wire.endTransmission() == 0;
}

void setup() {
  SerialMon.begin(115200);
  
  
  // 1. Initialize Power
  Wire.begin(I2C_SDA, I2C_SCL);
  setPowerBoostKeepOn(1);

  // 2. Initialize Modem (Wake up)
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  SerialMon.println("Initializing Modem...");
  modem.restart(); // Full restart to ensure clean state
  
  // 3. Initialize LoRa
  SerialMon.println("Initializing LoRa...");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  LoRa.setSyncWord(0x12); // Try 0x12 first, or 0xF3 if using Heltec defaults
  if (!LoRa.begin(LORA_BAND)) {
    SerialMon.println("LoRa Failed! Check Wiring.");
    while (1);
  }
  SerialMon.println("LoRa Gateway Ready. Waiting for packets...");
}

void loop() {
  // Check if a LoRa packet has arrived
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    
    SerialMon.print("LoRa Received: ");
    SerialMon.println(incoming);
    
    // Send to SMS
    SerialMon.println("Forwarding to SMS...");
    String smsText = "Alert from Heltec: " + incoming;
    
    if(modem.sendSMS(SMS_TARGET, smsText)) {
      SerialMon.println("SMS Sent!");
    } else {
      SerialMon.println("SMS Failed.");
    }
  }
}