#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "DHT.h"
#include "HT_SSD1306Wire.h"

// ---------------- PIN DEFINITIONS ----------------
#define DHTPIN 6        // GPIO for Node B’s DHT11
#define DHTTYPE DHT11
#define Vext 36         // Power control for onboard sensors/OLED

DHT dht(DHTPIN, DHTTYPE);

// ---------------- LORA CONFIG ----------------
#define RF_FREQUENCY        433000000  // India ISM band
#define TX_OUTPUT_POWER     5
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE         100

char rxpacket[BUFFER_SIZE];
char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

// Heltec library already defines the OLED object
extern SSD1306Wire display;

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); // LOW = power ON
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  // Initialize LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // Start listening
  Radio.Rx(0);

  // Initialize OLED
  VextON();
  delay(300);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Node B Ready");
  display.drawString(0, 15, "Relay Node (B+C -> A)");
  display.display();

  // Initialize DHT sensor
  dht.begin();
}

void loop() {
  Radio.IrqProcess(); // Handle LoRa events
}

// ---------------- CALLBACKS ----------------

// When Node B receives data from Node C
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  // to received node c data to node b
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
// to show in serial monitor in node b received data in node c
  Serial.printf("\r\nNode B received: %s\r\n", rxpacket);

  // Show Node C data on OLED
  display.clear();
  display.drawString(0, 0, "Node B RX (from C)");
  delay(5000);
  display.drawString(0, 20, rxpacket);
  display.display();

  // Read Node B’s own sensor
  float hB = dht.readHumidity();
  float tB = dht.readTemperature();

  if (!isnan(hB) && !isnan(tB)) {
    // Combine Node B + Node C data
    sprintf(txpacket, "TemB:%.1fC HumB%.1f%% | %s", tB, hB, rxpacket);

    Serial.printf("Forwarding to Node A: %s\r\n", txpacket);

    display.clear();
    display.drawString(0, 0, "Node B TX...");
    display.drawString(0, 20, "TemB: " + String(tB) + "C, " + String(hB) + "%humB");
    display.drawString(0, 40, rxpacket); // Show Node C data again
    display.display();
    //to send node b and c data to node a
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    lora_idle = false;
  } else {
    Serial.println("Node B sensor read error!");
  }
}

// Transmission success
void OnTxDone(void) {
  delay(5000);
  display.clear();
  display.drawString(0, 0, "Node B Sent OK");
  display.display();
  Serial.println("Node B forwarded successfully!");
  lora_idle = true;

  // Resume listening
  Radio.Rx(0);
}

// Transmission timeout
void OnTxTimeout(void) {
  Serial.println("Node B TX Timeout!");
  display.clear();
  display.drawString(0, 0, "Node B Timeout");
  display.display();
  Radio.Sleep();
  lora_idle = true;

  // Resume listening
  Radio.Rx(0);
}