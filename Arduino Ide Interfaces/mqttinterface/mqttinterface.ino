#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_DEBUG Serial

#include <TinyGsmClient.h>
#include <PubSubClient.h>

#define SerialMon Serial
#define SerialAT Serial1

// APN
const char apn[] = "airtelgprs.com";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT Broker
const char* broker = "o7417887.ala.asia-southeast1.emqxsl.com";
const int port = 1883;

const char* mqttUser = "mqtt-node";
const char* mqttPass = "YdsAGXMwj2iVe2c";

// Topic
const char* topicOxygen = "zone/zone_1/gas/oxygen";

// SIM800 Pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);

long lastMsg = 0;
uint32_t lastReconnectAttempt = 0;

bool mqttConnect() {

  SerialMon.print("Connecting to MQTT...");

  if (mqtt.connect("ESP32_SIM800L", mqttUser, mqttPass)) {

    SerialMon.println("SUCCESS");
    return true;
  }

  SerialMon.print("FAILED rc=");
  SerialMon.println(mqtt.state());
  return false;
}

void setup() {

  SerialMon.begin(115200);
  delay(10);

  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  SerialMon.println("Starting SIM800...");

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);

  modem.restart();

  SerialMon.println("Waiting for network...");

  if (!modem.waitForNetwork()) {

    SerialMon.println("Network Failed");
    while (true);
  }

  SerialMon.println("Network Connected");

  SerialMon.print("Connecting GPRS: ");
  SerialMon.println(apn);

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {

    SerialMon.println("GPRS Failed");
    while (true);
  }

  SerialMon.println("GPRS Connected");

  SerialMon.print("Signal Strength: ");
  SerialMon.println(modem.getSignalQuality());

  mqtt.setServer(broker, port);
  mqtt.setBufferSize(512);
}

void loop() {

  if (!mqtt.connected()) {

    uint32_t t = millis();

    if (t - lastReconnectAttempt > 10000L) {

      lastReconnectAttempt = t;

      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }

    delay(100);
    return;
  }

  mqtt.loop();

  long now = millis();

  if (now - lastMsg > 10000) {

    lastMsg = now;

    float oxygen = 21.6;

    char payload[10];
    dtostrf(oxygen, 1, 2, payload);

    SerialMon.print("Oxygen: ");
    SerialMon.println(payload);

    if (mqtt.publish(topicOxygen, payload)) {

      SerialMon.println("MQTT Publish OK");
    }
    else {

      SerialMon.println("MQTT Publish FAILED");
    }
  }
}