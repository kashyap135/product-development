/* ==========================================
   PROJECT: Node C - GSM Gateway (AM036)
   BOARD:   AM036 (ESP32 T-Call SIM800) + External LoRa
   ROLE:    Receive LoRa -> Send SMS
   ========================================== */

#define TINY_GSM_MODEM_SIM800

#include <Wire.h>
#include <TinyGsmClient.h>
#include <SPI.h>
#include <LoRa.h>

// --- USER CONFIG ---
#define SMS_TARGET "9640951822"  // <--- PUT YOUR PHONE NUMBER HERE
#define LORA_BAND  433E6           

// --- PIN DEFINITIONS (AM036 / T-Call) ---
#define MODEM_RST      5
#define MODEM_PWKEY    4
#define MODEM_POWER_ON 23
#define MODEM_TX       27
#define MODEM_RX       26
#define I2C_SDA        21
#define I2C_SCL        22

// --- EXTERNAL LORA PINS (Use these to wire your module) ---
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   25 
#define LORA_CS     13  
#define LORA_RST    14
#define LORA_DIO0   34

// --- OBJECTS ---
#define SerialMon Serial
#define SerialAT  Serial1
TinyGsm modem(SerialAT);

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

// Power Management for T-Call (Keeps the battery power alive)
bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(en ? 0x37 : 0x35);
  return Wire.endTransmission() == 0;
}

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  // 1. Initialize Power (Crucial for T-Call)
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
  // CRITICAL: We must match the Heltec Sender/Repeater settings exactly
  SerialMon.println("Init LoRa...");
  
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(LORA_BAND)) {
    SerialMon.println("LoRa Init Failed! Check Wiring.");
    while (1);
  }
  
  // --- MATCHING HELTEC SETTINGS ---
  // These settings allow the external module to talk to the Heltec V3
  LoRa.setSpreadingFactor(7);      // SF7
  LoRa.setSignalBandwidth(125E3);  // 125 kHz (BW 0 on Heltec)
  LoRa.setCodingRate4(5);          // 4/5 (CR 1 on Heltec)
  LoRa.setSyncWord(0x12);          // Private Sync Word (Default for Heltec)
  LoRa.setPreambleLength(8);       // Standard Preamble
  
  SerialMon.println("Node C Ready: Listening for Node B...");
}

void loop() {
  // Check packet
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    
    SerialMon.print("RX from Node B: ");
    SerialMon.println(incoming);
    
    // --- FORMATTING LOGIC ---
    // Input: "o2:20.50 co2:400 bt:25.0 ..."
    // We format this into a clean SMS message
    
    String smsText = "AIR ALERT:\n";
    
    // Replace short codes with readable labels and newlines
    incoming.replace("o2:", "O2: ");
    incoming.replace(" co2:", " %\nCO2: ");
    incoming.replace(" s_t:", " ppm\nTemp: ");
    incoming.replace(" s_h:", " C\nHum: ");
    incoming.replace(" b_p:", " %\nPres: ");
    incoming.replace(" b_g:", " hPa\nGas: ");
    incoming.replace(" b_a:", " KOhm\nAlt: ");
    incoming.replace(" h2s:", " m\nH2S: ");
    incoming.replace(" co:", " V\nCO: ");
    incoming.replace(" ch4:", " V\nCH4: ");
    
    smsText += incoming + "v"; // Add last unit manually
    
    SerialMon.println("--- Sending SMS ---");
    SerialMon.println(smsText);

    // Send the SMS
    if(modem.sendSMS(SMS_TARGET, smsText)) {
      SerialMon.println("SMS Sent Successfully!");
    } else {
      SerialMon.println("SMS Failed.");
    }
    
    // Wait before listening again to prevent duplicate sends
    delay(5000); 
  }
}