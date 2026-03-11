#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <DFRobot_OxygenSensor.h>
#include <Adafruit_ADS1X15.h>
#include "LoRaWan_APP.h"

// ================= PINS =================
#define SDA_PIN 41
#define SCL_PIN 42
#define Vext 36
#define TCA_ADDR 0x70   // I2C MUX Address

// ================= OBJECTS =================
SensirionI2cScd4x scd41;
DFRobot_OxygenSensor oxygen(&Wire1);
Adafruit_ADS1115 ads;

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

  // --- Serial Output ---
  Serial.println("------ SENSOR DATA ------");
  Serial.print("SCD41 CO2: "); Serial.print(co2); Serial.println(" ppm");
  Serial.print("Temp: "); Serial.print(temp); Serial.println(" C");
  Serial.print("Humidity: "); Serial.print(hum); Serial.println(" %");
  Serial.print("Oxygen: "); Serial.print(oxygenData); Serial.println(" %vol");
  Serial.println("-------------------------\n");

  delay(3000);
}
