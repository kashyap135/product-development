#define TINY_GSM_MODEM_SIM800
#include <Wire.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>

// ---------------- LORA CONFIG ----------------
#define RF_FREQUENCY        433E6   // 433 MHz
#define LORA_SS             33  // NSS (CS)
#define LORA_RST            14      // RESET
#define LORA_DIO0           35    // DIO0 (IRQ)
// Buffer
#define BUFFER_SIZE         255
char rxpacket[BUFFER_SIZE];



// USER SETTINGS
#define SMS_TARGET "9640951822"
#define APN "airtelgprs.com"

#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_TOPIC "milieu/lora/temp"

// MODEM PINS
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

#define I2C_SDA 21
#define I2C_SCL 22

#define SerialMon Serial
#define SerialAT Serial1

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);

// Power chip settings
#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(en ? 0x37 : 0x35);
  return Wire.endTransmission() == 0;
}

void mqttConnect() {

  SerialMon.print("Connecting to MQTT...");

  while (!mqtt.connected()) {

    if (mqtt.connect("ESP32_SIM800")) {
      SerialMon.println("MQTT Connected");
    }

    else {
      SerialMon.print(".");
      delay(2000);
    }
  }
}

void setup() {

  SerialMon.begin(115200);
  while (!Serial);

  Serial.println("Node C - ESP32 WROVER + SX1278 Receiver (Debug Mode)");

  // LoRa init
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(RF_FREQUENCY)) {
    Serial.println("LoRa init failed. Check wiring!");
    while (true);
  }

  // Match Node B parameters
  LoRa.setSpreadingFactor(9);       // SF9
  LoRa.setSignalBandwidth(125E3);   // BW 125 kHz
  LoRa.setCodingRate4(5);           // CR 4/5

  // Debug: dump SX1278 registers
  Serial.println("Dumping SX1278 registers for debug:");
  LoRa.dumpRegisters(Serial);

  Serial.println("LoRa Receiver Ready");


  Wire.begin(I2C_SDA, I2C_SCL);
  setPowerBoostKeepOn(1);

  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  SerialMon.println("Initializing Modem...");
  modem.restart();

  SerialMon.println(modem.getModemInfo());

  SerialMon.println("Waiting for Network...");

  if (!modem.waitForNetwork()) {
    SerialMon.println("Network Failed");
    while(true);
  }

  SerialMon.println("Network Connected");

  if (!modem.gprsConnect(APN, "", "")) {
    SerialMon.println("GPRS Failed");
    while(true);
  }

  SerialMon.println("GPRS Connected");

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);

  mqttConnect();

  // SEND SMS
  if (modem.sendSMS(SMS_TARGET, "Hii this is milieu Global ")) {
    SerialMon.println("SMS Sent");
  } else {
    SerialMon.println("SMS Failed");
  }

  // MQTT PUBLISH
  mqtt.publish(MQTT_TOPIC, "SIM800 Connected to MQTT");

}

void loop() {

  if (!mqtt.connected()) {
    mqttConnect();
  }

  mqtt.loop();
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("\n[DEBUG] Packet detected, size=");
    Serial.println(packetSize);

    int i = 0;
    while (LoRa.available() && i < BUFFER_SIZE - 1) {
      rxpacket[i++] = (char)LoRa.read();
    }
    rxpacket[i] = '\0';

    Serial.print("[DEBUG] Raw packet: ");
    Serial.println(rxpacket);
  if (strlen(rxpacket) > 0) {
  mqtt.publish(MQTT_TOPIC, rxpacket);
  modem.sendSMS(SMS_TARGET, rxpacket);
} else {
  SerialMon.println("[DEBUG] Empty packet, not publishing");
}

    // Print RSSI and SNR
    Serial.print("[DEBUG] RSSI: ");
    Serial.println(LoRa.packetRssi());
    Serial.print("[DEBUG] SNR: ");
    Serial.println(LoRa.packetSnr());
  } else {
    // Show noise floor periodically
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
      Serial.print("[DEBUG] Listening... RSSI=");
      Serial.println(LoRa.packetRssi());
      lastPrint = millis();
    }
  }
}