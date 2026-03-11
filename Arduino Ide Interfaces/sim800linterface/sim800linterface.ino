#define TINY_GSM_MODEM_SIM800
#include <Wire.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

// USER SETTINGS
#define SMS_TARGET "9640951822"
#define APN "airtelgprs.com"

#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_TOPIC "ravindra/sim800/test"

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
  if (modem.sendSMS(SMS_TARGET, "SIM800 MQTT Gateway Started")) {
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

  static unsigned long lastTime = 0;

  if (millis() - lastTime > 10000) {

    lastTime = millis();

    String msg = "Hello from ESP32 SIM800";

    SerialMon.println("Publishing MQTT");

    mqtt.publish(MQTT_TOPIC, msg.c_str());

    modem.sendSMS(SMS_TARGET, msg);

  }
}