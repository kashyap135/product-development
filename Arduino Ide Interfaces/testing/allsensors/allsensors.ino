#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "DFRobot_OxygenSensor.h"
#include <SensirionI2cScd4x.h>
#include <Adafruit_ADS1X15.h>

/* MUX setup */
#define MUX_SDA 41
#define MUX_SCL 42
#define MUX_ADDR 0x70

/* MUX channels */
#define MUX_CH_O2  4
#define MUX_CH_SCD 1
#define MUX_CH_ADS 2

/* Oxygen sensor */
#define Oxygen_IICAddress ADDRESS_3  

/* BME680 SPI pins */
#define BME_SCK  39
#define BME_MISO 40
#define BME_MOSI 45
#define BME_CS   46
#define SEALEVELPRESSURE_HPA (1013.25)

/* Objects */
DFRobot_OxygenSensor oxygen(&Wire1);
SensirionI2cScd4x scd41;
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
Adafruit_ADS1115 ads;

int16_t error;
bool bme_found = false;

/* Helper: select mux channel */
void muxselect(uint8_t ch) {
  Wire1.beginTransmission(MUX_ADDR);
  Wire1.write(1 << ch);
  Wire1.endTransmission();
  delay(5);
}
void reset()
{
Serial.println("Type 'R' to reset the board");
 if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'R' || cmd == 'r') {
      Serial.println("Resetting board...");
      delay(100);
      ESP.restart();   // ESP32 software reset
    }
  }

}
/* Oxygen */
float getOxygenData() {
  muxselect(MUX_CH_O2);
  delay(10);
  return oxygen.getOxygenData(20);
}

/* SCD41 */
void readScd41(float &co2, float &temp, float &hum) {
  muxselect(MUX_CH_SCD);
  scd41.wakeUp();
  scd41.measureSingleShot();
  delay(5000);

  uint16_t co2ppm;
  float t, h;
  error = scd41.readMeasurement(co2ppm, t, h);
  if (!error) {
    co2 = co2ppm;
    temp = t;
    hum = h;
  } else {
    co2 = NAN;
    temp = NAN;
    hum = NAN;
    Serial.println("[Error] SCD41 read failed");
  }
}

/* BME680 */
void readBme680(float &temp, float &hum, float &pres, float &gas, float &alti) {
  if (!bme.performReading()) {
    Serial.println("[Error] BME680 read failed");
    temp = NAN;
    return;
  }
  temp = bme.temperature;
  hum = bme.humidity;
  pres = bme.pressure / 100.0;
  gas = bme.gas_resistance / 1000.0;
  alti = bme.readAltitude(SEALEVELPRESSURE_HPA);
}

/* ADS1115 analog sensors */
float readH2S() {
  muxselect(MUX_CH_ADS);
  int16_t raw = ads.readADC_SingleEnded(0);
  return raw * 0.125 / 1000.0; // volts
}

float readCO() {
  muxselect(MUX_CH_ADS);
  int16_t raw = ads.readADC_SingleEnded(1);
  return raw * 0.125 / 1000.0;
}

float readCH4() {
  muxselect(MUX_CH_ADS);
  int16_t raw = ads.readADC_SingleEnded(2);
  return raw * 0.125 / 1000.0;
}

/* Setup */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire1.begin(MUX_SDA, MUX_SCL, 100000);

  // Oxygen
  muxselect(MUX_CH_O2);
  if (!oxygen.begin(Oxygen_IICAddress)) {
    Serial.println("O2 sensor not found!");
  } else {
    Serial.println("O2 sensor ready.");
  }

  // SCD41
  muxselect(MUX_CH_SCD);
  scd41.begin(Wire1, SCD41_I2C_ADDR_62);
  scd41.stopPeriodicMeasurement();
  scd41.reinit();
  Serial.println("SCD41 ready.");

  // BME680
  if (!bme.begin()) {
    Serial.println("BME680 not found!");
    bme_found = false;
  } else {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    bme_found = true;
    Serial.println("BME680 ready.");
  }

  // ADS1115
  muxselect(MUX_CH_ADS);
  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("ADS1115 not found!");
  } else {
    ads.setGain(GAIN_ONE);
    Serial.println("ADS1115 ready.");
  }
}

/* Loop */
void loop() {
  Serial.println("\n==============================");
  Serial.println("   NEW SENSOR READINGS");
  Serial.println("==============================");
 

  // Oxygen
  float o2 = getOxygenData();
  Serial.print("Oxygen: "); Serial.print(o2); Serial.println(" %");

  // SCD41
  float co2, t_scd, h_scd;
  readScd41(co2, t_scd, h_scd);
  Serial.print("CO2: "); Serial.print(co2); Serial.println(" ppm");
  Serial.print("Temp(SCD41): "); Serial.print(t_scd); Serial.println(" °C");
  Serial.print("Hum(SCD41): "); Serial.print(h_scd); Serial.println(" %");

  // BME680
  if (bme_found) {
    float t_bme, h_bme, p_bme, g_bme, alt_bme;
    readBme680(t_bme, h_bme, p_bme, g_bme, alt_bme);
    Serial.print("Temp(BME680): "); Serial.print(t_bme); Serial.println(" °C");
    Serial.print("Hum(BME680): "); Serial.print(h_bme); Serial.println(" %");
    Serial.print("Pres: "); Serial.print(p_bme); Serial.println(" hPa");
    Serial.print("Gas: "); Serial.print(g_bme); Serial.println(" KOhms");
    Serial.print("Alt: "); Serial.print(alt_bme); Serial.println(" m");
  }

  // Analog gases
  float h2s = readH2S();
  float co   = readCO();
  float ch4  = readCH4();
  Serial.print("H2S: "); Serial.print(h2s); Serial.println(" V");
  Serial.print("CO: ");  Serial.print(co);  Serial.println(" V");
  Serial.print("CH4: "); Serial.print(ch4); Serial.println(" V");
   reset();
   

  delay(5000);
}