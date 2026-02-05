#define TINY_GSM_MODEM_SIM800
#include <Wire.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>

// --- USER CONFIG ---
#define SMS_TARGET ""   // Your Phone Number
#define LORA_BAND  433E6          // Match your LoRa module
#define APN        "airtelgprs.com" // Replace with your SIM APN
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "ravindra/lora/temp"

// --- PIN DEFINITIONS ---
#define MODEM_RST      5
#define MODEM_PWKEY    4
#define MODEM_POWER_ON 23
#define MODEM_TX       27
#define MODEM_RX       26
#define I2C_SDA        21
#define I2C_SCL        22

// --- LORA PINS ---
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   25
#define LORA_CS     13
#define LORA_RST    14
#define LORA_DIO0   34

#define SerialMon Serial
#define SerialAT  Serial1
TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(en ? 0x37 : 0x35);
  return Wire.endTransmission() == 0;
}

void mqttConnect() {
  SerialMon.print("Connecting to MQTT...");
  while (!mqtt.connected()) {
    if (mqtt.connect("ESP32ClientSIM800")) {
      SerialMon.println("connected!");
      mqtt.subscribe(MQTT_TOPIC); // optional subscription
    } else {
      SerialMon.print(".");
      delay(2000);
    }
  }
}

void setup() {
  SerialMon.begin(115200);

  // Power
  Wire.begin(I2C_SDA, I2C_SCL);
  setPowerBoostKeepOn(1);

  // Modem
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

  // GPRS connect
  SerialMon.println("Connecting to network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println("Network failed");
    while (true);
  }
  if (!modem.gprsConnect(APN, "", "")) {
    SerialMon.println("GPRS failed");
    while (true);
  }
  SerialMon.println("GPRS connected");

  // MQTT setup
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqttConnect();

  // LoRa
  SerialMon.println("Initializing LoRa...");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  LoRa.setSyncWord(0x12);
  if (!LoRa.begin(LORA_BAND)) {
    SerialMon.println("LoRa Failed! Check Wiring.");
    while (1);
  }
  SerialMon.println("LoRa Gateway Ready. Waiting for packets...");
}

void loop() {
  if (!mqtt.connected()) {
    mqttConnect();
  }
  mqtt.loop(); // keep MQTT alive

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    SerialMon.print("LoRa Received: ");
    SerialMon.println(incoming);

    // Send SMS
    String smsText = "Alert from Milieu Global: " + incoming;
    if(modem.sendSMS(SMS_TARGET, smsText)) {
      SerialMon.println("SMS Sent!");
    } else {
      SerialMon.println("SMS Failed.");
    }

    // Publish to MQTT
    SerialMon.println("Publishing to MQTT...");
    if (mqtt.publish(MQTT_TOPIC, incoming.c_str())) {
      SerialMon.println("MQTT Published!");
    } else {
      SerialMon.println("MQTT Publish Failed.");
      mqttConnect(); // reconnect if failed
    }
  }
}