/* Heltec Automation Receive communication test example
 *
 * Function:
 * 1. Receive the same frequency band lora signal program
 *  
 * Description:
 * 
 * HelTec AutoMation, Chengdu, China
 * 成都惠利特自动化科技有限公司
 * www.heltec.org
 *
 * this project also realess in GitHub:
 * https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series
 * */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "DHT.h"
#include "HT_SSD1306Wire.h"

#define Vext 36


#define RF_FREQUENCY                                865000000 // Hz

#define TX_OUTPUT_POWER                             14        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false


#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 50 // Define the payload size here

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
// FIX: Do NOT define 'SSD1306Wire display(...)' here. 
// The Heltec library already did it. We just reference it:
extern SSD1306Wire display; 
volatile bool packetReceived = false;

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

int16_t txNumber;

int16_t rssi,rxSize;

bool lora_idle = true;

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
    VextON();
  delay(300);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();

display.display();
  display.drawString(0, 0, "Heltec LoRa Ready(reciever)");
  display.display();


    txNumber=0;
    rssi=0;
  
    RadioEvents.RxDone = OnRxDone;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                               LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                               LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                               0, true, 0, 0, LORA_IQ_INVERSION_ON, true );


}



void loop()
{
  if (lora_idle)
  {
    lora_idle = false;
    Serial.println("Listening...");
    Radio.Rx(0);
  }

  if (packetReceived)
  {
    packetReceived = false;

    Serial.printf("RX: %s | RSSI: %d\n", rxpacket, rssi);

    display.clear();
    display.drawString(0, 0, "LoRa Packet RX");
    display.drawString(0, 15, rxpacket);
    display.drawString(0, 30, "RSSI: " + String(rssi));
    display.display();
  }

  Radio.IrqProcess();
}


void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi_val, int8_t snr)
{
  rxSize = size;
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';   // terminate string

  rssi = rssi_val;

  packetReceived = true;  // signal main loop
  lora_idle = true;

  Radio.Sleep();
}
