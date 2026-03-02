#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h> 
#include <Adafruit_ADS1X15.h>
#include <SensirionI2cScd4x.h>
#include "HT_SSD1306Wire.h"
#include "Adafruit_BME680.h"
#include "DFRobot_OxygenSensor.h"
#include "LoRaWan_APP.h"

#define SEALEVELPRESSURE_HPA (1013.25)

// SPI pins
#define BME_CS   46
#define BME_MOSI 45
#define BME_MISO 40
#define BME_SCK  39

// I2C pins
#define SENSOR_SDA 41
#define SENSOR_SCL 42
#define OLED_SDA   17
#define OLED_SCL   18
#define Vext       36

#define TCA_ADDR   0x70

// MUX Channels
#define CH_SCD41   1
#define CH_ADS     2
#define CH_OXYGEN  4

#define SCD_ADDR   0x62
#define OXY_ADDR   ADDRESS_3

// LoRa settings
#define RF_FREQUENCY 433000000
#define TX_OUTPUT_POWER 17   // reliable power
#define LORA_BANDWIDTH 0     // 125 kHz
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1    // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 255 

char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;

// LoRa callbacks
void OnTxDone(void) { Serial.println("✅ LoRa TX Done"); lora_idle = true; }
void OnTxTimeout(void) { Serial.println("❌ LoRa TX Timeout"); lora_idle = true; }
void OnTxError(void) { Serial.println("❌ LoRa TX Error"); lora_idle = true; }

// -------- Objects --------
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
Adafruit_ADS1115 ads;
SensirionI2cScd4x scd4x;
DFRobot_OxygenSensor oxygen(&Wire1);
extern SSD1306Wire display;

static int16_t error;

// -------- Power --------
void VextON() { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW); }
void VextOFF() { pinMode(Vext, OUTPUT); digitalWrite(Vext, HIGH); }

// -------- MUX Select --------
void selectMux(uint8_t channel) {
  Wire1.beginTransmission(TCA_ADDR);
  Wire1.write(1 << channel);
  Wire1.endTransmission();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  VextON();
  delay(1000);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "System Initializing...");
  display.display();

  // Initialize LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  //RadioEvents.TxError = OnTxError;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    0, LORA_PREAMBLE_LENGTH,
                    0, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
  Radio.SetPublicNetwork(true); // sync word 0x34

  // Sensor Bus
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  Wire1.setClock(100000);
  delay(500);

  // Init sensors (same as your original)
  if (!bme.begin()) { Serial.println("BME680 not found!"); while (1) delay(1000); }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  ads.begin(0x48, &Wire1);
  ads.setGain(GAIN_ONE);
  oxygen.begin(OXY_ADDR);
  scd4x.begin(Wire1, SCD_ADDR);
  scd4x.stopPeriodicMeasurement();
  scd4x.reinit();

  display.clear();
  display.drawString(0, 0, "System Ready!");
  display.display();
}

// ================= LOOP =================
void loop() {
  Radio.IrqProcess();

  // Read sensors (same as your original)
  selectMux(CH_ADS);
  bme.performReading();
  float pres = bme.pressure / 100.0;
  float gas = bme.gas_resistance / 1000.0;
  float alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  float h2s = ads.computeVolts(ads.readADC_SingleEnded(0));
  float co  = ads.computeVolts(ads.readADC_SingleEnded(1));
  float ir  = ads.computeVolts(ads.readADC_SingleEnded(2));

  selectMux(CH_OXYGEN);
  float oxygenData = oxygen.getOxygenData(10);

  selectMux(CH_SCD41);
  uint16_t co2 = 0; float temperature = 0; float humidity = 0;
  scd4x.wakeUp(); scd4x.measureSingleShot(); delay(5000);
  scd4x.readMeasurement(co2, temperature, humidity);

  // Send LoRa packet
  if (lora_idle) {
    sprintf(txpacket, "%.2f,%.0f,%.1f,%.1f,%.1f,%.2f,%.1f,%.2f,%.2f,%.2f",
            oxygenData, co2, temperature, humidity, pres, gas, alt, h2s, co, ir);
    Serial.print("Sending LoRa Packet: "); Serial.println(txpacket);
    lora_idle = false;
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  }

  delay(10000); // send every 10s
}
