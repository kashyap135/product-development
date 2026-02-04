#include <SPI.h>
#include <LoRa.h>

#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_CS    18
#define LORA_RST   14
#define LORA_IRQ   26

#define LORA_BAND  433E6

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa Receiver Ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // Print raw packet
    Serial.print("Received raw: ");
    Serial.println(incoming);

    // Identify sender
    if (incoming.startsWith("TempC")) {
      Serial.println("---- Node C Data ----");
      Serial.println(incoming);
    } else if (incoming.startsWith("TempB")) {
      Serial.println("---- Node B Data ----");
      Serial.println(incoming);
    } else {
      Serial.println("Unknown node: " + incoming);
    }

    // Show RSSI
    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());
    Serial.println("---------------------");
  }
}