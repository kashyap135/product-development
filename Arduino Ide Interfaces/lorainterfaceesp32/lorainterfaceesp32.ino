#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- LoRa Pins (SX1278) ---
#define SCK     18
#define MISO    19
#define MOSI    25
#define SS      1
#define RST     14
#define DIO0    34
#define BAND    433E6

// --- SIM800L Pins ---
#define MODEM_TX             26
#define MODEM_RX             27
#define MODEM_SERIAL_BAUD    9600
HardwareSerial SerialGSM(1); // Use Serial1 for SIM800L

// --- WiFi & MQTT ---
const char* ssid          = "Ravindra";
const char* password      = "9912831367";
const char* mqtt_server   = "broker.hivemq.com";
const char* mqtt_topic    = "sensors/node_a/data";

// --- SMS Config ---
const char* phone_number  = "+919640951822"; // Your mobile number

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize SIM800L Serial
  SerialGSM.begin(MODEM_SERIAL_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  Serial.println("Initializing SIM800L...");
  initSIM800L();

  // 2. WiFi Setup
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  // 3. LoRa Setup
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa Error!");
    while (1);
  }
  
  // Set LoRa to match Node A & B
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("Node C (Gateway) Ready!");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // Check for LoRa packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String rxData = "";
    while (LoRa.available()) {
      rxData += (char)LoRa.read();
    }

    Serial.println("\n--- Received Data ---");
    Serial.println(rxData);

    // 1. Send to MQTT
    client.publish(mqtt_topic, rxData.c_str());
    Serial.println("Sent to MQTT");

    // 2. Send SMS via SIM800L
    sendSMS(rxData);
  }
}

// --- Helper Functions ---

void initSIM800L() {
  SerialGSM.println("AT"); // Handshake
  delay(1000);
  SerialGSM.println("AT+CMGF=1"); // Set SMS text mode
  delay(1000);
}

void sendSMS(String text) {
  Serial.println("Sending SMS...");
  SerialGSM.print("AT+CMGS=\"");
  SerialGSM.print(phone_number);
  SerialGSM.println("\"");
  delay(1000);
  SerialGSM.print(text); // The sensor data string
  delay(100);
  SerialGSM.write(26); // ASCII code for CTRL+Z to send
  delay(5000); 
  Serial.println("SMS Processed.");
}

void setup_wifi() {
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected.");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("NodeC_Gateway")) {
      Serial.println("MQTT Connected");
    } else {
      delay(5000);
    }
  }
}