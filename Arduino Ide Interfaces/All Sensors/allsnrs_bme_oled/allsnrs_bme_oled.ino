#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <DFRobot_OxygenSensor.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

// ================= PINS =================
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define SDA_PIN 41
#define SCL_PIN 42
#define Vext 36
#define TCA_ADDR 0x70   // I2C MUX Address

// SPI pins for BME680
#define BME_CS   34
#define BME_MOSI 35
#define BME_MISO 37
#define BME_SCK  36
#define SEALEVELPRESSURE_HPA (1013.25)

// ================= OBJECTS =================
SensirionI2cScd4x scd41;
DFRobot_OxygenSensor oxygen(&Wire1);
Adafruit_ADS1115 ads;
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
extern SSD1306Wire display;

// Variables
float co2_active, co2_ref, co2_ratio, co2_ppm;
float ch4_active, ch4_ref, ch4_ratio, ch4_ppm;

// ================= MUX SELECT FUNCTION =================
void selectMux(uint8_t channel) {
  if (channel > 7) return;
  Wire1.beginTransmission(TCA_ADDR);
  Wire1.write(1 << channel);
  Wire1.endTransmission();
}

// ================= POWER =================
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

float convertVoltage(int16_t raw) {
  return raw * 0.125 / 1000.0;   // for ADS1115 GAIN_ONE
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  // Power up
  VextON();
  delay(1000); 
 
  Serial.println("System Starting...");

  // --- OLED Init (Wire bus) ---
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Init Sensors...");
  display.display();

  // --- I2C MUX bus (Wire1) ---
  Wire1.begin(SDA_PIN, SCL_PIN);

  // ================= SCD41 INIT (Channel 0) =================
  selectMux(0);
  scd41.begin(Wire1, 0x62);
  scd41.stopPeriodicMeasurement();
  scd41.reinit();
  Serial.println("SCD41 Initialized");

  // ================= Oxygen INIT (Channel 1) =================
  selectMux(1);
  while (!oxygen.begin(ADDRESS_3)) {
    Serial.println("Oxygen Sensor Not Detected!");
    delay(1000);
  }
  Serial.println("Oxygen Sensor Initialized");

  // ================= ADS1115 INIT (Channel 2) =================
  selectMux(2);
  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("ADS1115 init failed!");
    while (1);
  }
  ads.setGain(GAIN_ONE);  // ±4.096V
  Serial.println("ADS1115 Ready");

  // ================= BME680 INIT (SPI) =================
  if (!bme.begin()) {
    Serial.println("Could not find BME680 sensor!");
    while (1) delay(1000);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms
  Serial.println("BME680 Ready");

  Serial.println("All Sensors Ready!");
}

void h2s(int16_t raw) {
  float voltage = convertVoltage(raw);
  Serial.print("H2S Voltage: ");
  Serial.println(voltage, 3);
}

void ir15() {
  int16_t raw_co2_A = ads.readADC_SingleEnded(0);
  int16_t raw_co2_R = ads.readADC_SingleEnded(1);

  co2_active = raw_co2_A * 0.125 / 1000.0;
  co2_ref    = raw_co2_R * 0.125 / 1000.0;

  if (co2_ref > 0.01) {
    co2_ratio = co2_active / co2_ref;
    co2_ppm = (1.0 - co2_ratio) * 2000;   // calibrate later
  } else {
    co2_ppm = 0;
  }

  int16_t raw_ch4_A = ads.readADC_SingleEnded(0);
  int16_t raw_ch4_R = ads.readADC_SingleEnded(1);

  ch4_active = raw_ch4_A * 0.125 / 1000.0;
  ch4_ref    = raw_ch4_R * 0.125 / 1000.0;

  if (ch4_ref > 0.01) {
    ch4_ratio = ch4_active / ch4_ref;
    ch4_ppm = (1.0 - ch4_ratio) * 5000;   // calibrate later
  } else {
    ch4_ppm = 0;
  }

  Serial.print("CO2: "); Serial.print(co2_ppm);
  Serial.print(" ppm  |  CH4: "); Serial.print(ch4_ppm);
  Serial.println(" ppm");
}

void sgx_4co(int16_t raw) {
  float voltage = convertVoltage(raw);
  Serial.print("CO Voltage: ");
  Serial.println(voltage, 3);
}

void loop() {
  // --- SCD41 (Channel 0) ---
  selectMux(0);
  uint16_t co2;
  float temp, hum;
  scd41.measureSingleShot();
  delay(5000);  // SCD41 needs ~5 sec
  scd41.readMeasurement(co2, temp, hum);

  // --- ADS1115 (Channel 2) ---
  selectMux(2);
  int16_t h2s_raw = ads.readADC_SingleEnded(2);
  int16_t co_raw  = ads.readADC_SingleEnded(3);
  h2s(h2s_raw);
  ir15();
  sgx_4co(co_raw);

  // --- Oxygen (Channel 1) ---
  selectMux(1);
  float oxygenData = oxygen.getOxygenData(10);

  // --- BME680 (SPI) ---
  if (bme.performReading()) {
    float pres = bme.pressure / 100.0; // hPa
    float gas  = bme.gas_resistance / 1000.0; // KOhms
    float alt  = bme.readAltitude(SEALEVELPRESSURE_HPA);

    Serial.print("Pressure: "); Serial.print(pres); Serial.println(" hPa");
    Serial.print("Gas: "); Serial.print(gas); Serial.println(" KOhms");
    Serial.print("Altitude: "); Serial.print(alt); Serial.println(" m");

    display.clear();
    display.drawString(0, 0, "CO2: " + String(co2) + " ppm");
    display.drawString(0, 12, "Temp: " + String(temp,1) + " C");
    display.drawString(0, 24, "Hum: " + String(hum,1) + " %");
    display.drawString(0, 36, "O2: " + String(oxygenData,1) + " %vol");
    display.drawString(0, 48, "P:" + String(pres,1)+" hPa G:"+String(gas,1)+" Alt:"+String(alt,1));
    display.display();
  }

  Serial.println("-------------------------\n");
  delay(3000);
}
