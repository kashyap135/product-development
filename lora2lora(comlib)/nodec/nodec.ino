#define TINY_GSM_MODEM_SIM800

#include <RadioLib.h>
#include <TinyGsmClient.h>
#include <Wire.h>

// -------- SMS --------
#define SMS_TARGET "+918247567314"

// -------- GSM PINS --------
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_PWRKEY 4
#define MODEM_RST 5
#define MODEM_POWER_ON 23

#define SerialAT Serial1
TinyGsm modem(SerialAT);

// -------- RA-02 SX1278 PINS --------
#define LORA_CS   13
#define LORA_DIO0 34
#define LORA_RST  14
#define LORA_SCK  18
#define LORA_MISO 19
#define LORA_MOSI 25

SX1278 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, RADIOLIB_NC);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ---- GSM Init ----
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_POWER_ON, HIGH);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_PWRKEY, LOW);

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  Serial.println("Initializing Modem...");
  modem.restart();

  // ---- SPI ----
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  Serial.println("Initializing SX1278...");

  int state = radio.begin(
      433.0,
      125.0,
      7,
      5,
      0x34,
      14,
      8
  );

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Radio init failed: ");
    Serial.println(state);
    while (true);
  }

  Serial.println("Receiver Ready");
}

void loop() {
  String str;
  int state = radio.receive(str);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("\nReceived:");
    Serial.println(str);

    String sms = "Heltec Sensor -> " + str;

    Serial.println("Sending SMS...");
    bool ok = modem.sendSMS(SMS_TARGET, sms);

    if (ok) Serial.println("SMS Sent");
    else Serial.println("SMS Failed");

    delay(4000); // GSM power recovery
  }
  else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.print("Receive failed: ");
    Serial.println(state);
  }
}
