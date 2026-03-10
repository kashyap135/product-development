#define TINY_GSM_MODEM_SIM800

#include <Wire.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// -------- LoRa Pins --------
#define SCK 18
#define MISO 19
#define MOSI 25
#define SS 15
#define RST 14
#define DIO0 34
#define BAND 433E6

// -------- USER SETTINGS --------
#define SMS_TARGET "9640951822"
#define APN "airtelgprs.com"

#define MQTT_BROKER "o7417887.ala.asia-southeast1.emqxsl.com"
#define MQTT_PORT 8883
#define MQTT_USER "mqtt-node"
#define MQTT_PASS "YdsAGXMwj2iVe2c"

// -------- MODEM PINS --------
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

// -------- POWER CHIP --------
#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(en ? 0x37 : 0x35);
  return Wire.endTransmission() == 0;
}

// -------- MQTT CONNECT --------
void mqttConnect() {

  SerialMon.print("Connecting MQTT");

  while (!mqtt.connected()) {

    if (mqtt.connect("NODE_LOCAL_CLIENT", MQTT_USER, MQTT_PASS)) {
      SerialMon.println(" -> Connected");
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

  // -------- LoRa Setup --------
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND)) {
    Serial.println("LoRa Failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("Node C Gateway Ready");
   // SEND SMS
  if (modem.sendSMS(SMS_TARGET, "THIS IS FROM MILIEU GLOBAL")) {
    SerialMon.println("SMS Sent");
  } else {
    SerialMon.println("SMS Failed");
  }

}

void publishJSON(const char* topic, float value, const char* unit){

  StaticJsonDocument<200> doc;

  doc["value"] = value;
  doc["unit"] = unit;
  doc["sensor_id"] = "NODE_";
  doc["timestamp"] = millis();

  char payload[256];
  serializeJson(doc, payload);

  mqtt.publish(topic, payload);

}

void loop() {

  if (!mqtt.connected()) {
    mqttConnect();
  }

  mqtt.loop();

  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    String rxData = "";

    while (LoRa.available()) {
      rxData += (char)LoRa.read();
    }

    Serial.println("Received LoRa Data:");
    Serial.println(rxData);

    // Expected Format:
    // O2,CO2,TEMP,HUM,PRESSURE,GAS,ALT,H2S,CO,CH4

    float o2, co2, temp, hum, pressure, airquality, alt, h2s, co, ch4;

    sscanf(rxData.c_str(),
           "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
           &o2,&co2,&temp,&hum,&pressure,&airquality,&alt,&h2s,&co,&ch4);

    // -------- MQTT JSON Publish --------

    publishJSON("zone/zone_1/gas/oxygen", o2, "%");
    publishJSON("zone/zone_1/gas/carbonMonoxide", co, "ppm");
    publishJSON("zone/zone_1/gas/methane", ch4, "ppm");
    publishJSON("zone/zone_1/gas/hydrogenSulfide", h2s, "ppm");
    publishJSON("zone/zone_1/gas/temperature", temp, "C");
    publishJSON("zone/zone_1/gas/humidity", hum, "%");
    publishJSON("zone/zone_1/gas/pressure", pressure, "hPa");
    publishJSON("zone/zone_1/gas/carbondioxide", co2, "ppm");
    publishJSON("zone/zone_1/gas/airquality", airquality, "ppm");
    publishJSON("zone/zone_1/gas/altitude", alt, "ppm");
    

    Serial.println("MQTT JSON Published");

    // -------- SMS --------

    String sms = "O2:" + String(o2) +
                 " CO:" + String(co) +
                 " CH4:" + String(ch4) +
                 "Temp"  + String(temp) +
                 "Hum"  + String(hum) +
                 "Pressure" + String(pressure)+
                 "airquality" +String(airquality)+
                 "c02" + String(co2)+
                 "altitude" + String(alt)+
                 " H2S:" + String(h2s) ;

    modem.sendSMS(SMS_TARGET, sms);

    Serial.println("SMS Sent");

  }

}