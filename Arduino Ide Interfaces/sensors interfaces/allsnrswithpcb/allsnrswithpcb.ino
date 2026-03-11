#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SensirionI2cScd4x.h>
#include "HT_SSD1306Wire.h"
#include "DFRobot_OxygenSensor.h"

// -------- Pins --------
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

// -------- Objects --------
Adafruit_ADS1115 ads;
SensirionI2cScd4x scd4x;
DFRobot_OxygenSensor oxygen(&Wire1);
extern SSD1306Wire display;

static char errorMessage[64];
static int16_t error;

// -------- Power --------
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

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

  // OLED Bus
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "System Initializing...");
  display.display();

  // Sensor Bus
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  Wire1.setClock(100000);
  delay(500);

  // -------- ADS INIT --------
  selectMux(CH_ADS);
  delay(100);

  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("ADS1115 Not Found!");
  } else {
    ads.setGain(GAIN_ONE);
    Serial.println("ADS1115 Ready");
  }

  // -------- Oxygen INIT --------
  selectMux(CH_OXYGEN);
  delay(100);

  if (!oxygen.begin(OXY_ADDR)) {
    Serial.println("Oxygen Sensor Not Found!");
  } else {
    Serial.println("Oxygen Sensor Ready");
  }

  // -------- SCD41 INIT --------
  selectMux(CH_SCD41);
  delay(100);

  scd4x.begin(Wire1, SCD_ADDR);
  scd4x.stopPeriodicMeasurement();
  delay(100);
  scd4x.reinit();
  delay(100);

  uint64_t serialNumber;
  error = scd4x.getSerialNumber(serialNumber);

  if (error) {
    Serial.println("SCD41 Not Found!");
  } else {
    Serial.println("SCD41 Ready");
  }

  display.clear();
  display.drawString(0, 0, "System Ready!");
  display.display();
}

// ================= LOOP =================
void loop() {

  // ================= ADS READ =================
  selectMux(CH_ADS);
  delay(5);

  float h2s = ads.computeVolts(ads.readADC_SingleEnded(0));
  float co  = ads.computeVolts(ads.readADC_SingleEnded(1));
  float ir  = ads.computeVolts(ads.readADC_SingleEnded(2));

  // ================= OXYGEN READ =================
  selectMux(CH_OXYGEN);
  delay(5);

  float oxygenData = oxygen.getOxygenData(10);

  // ================= SCD41 READ =================
  selectMux(CH_SCD41);
  delay(5);

  uint16_t co2 = 0;
  float temperature = 0;
  float humidity = 0;

  scd4x.wakeUp();
  scd4x.measureSingleShot();
  delay(5000);

  error = scd4x.readMeasurement(co2, temperature, humidity);

  // ================= SERIAL PRINT =================
  Serial.println("------------");

  Serial.print("H2S: "); Serial.println(h2s, 3);
  Serial.print("CO: ");  Serial.println(co, 3);
  Serial.print("IR: ");  Serial.println(ir, 3);

  Serial.print("Oxygen: ");
  Serial.print(oxygenData);
  Serial.println(" %vol");

  if (!error) {
    Serial.print("CO2: "); Serial.print(co2); Serial.println(" ppm");
    Serial.print("Temp: "); Serial.print(temperature); Serial.println(" C");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
  }

  // ================= OLED DISPLAY =================
  display.clear();
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "O2: " + String(oxygenData,1) + "%");
  display.drawString(0, 12, "CO2: " + String(co2) + "ppm");
  display.drawString(0, 24, "H2S: " + String(h2s,2));
  display.drawString(0, 36, "Temp: " + String(temperature,1));

  display.display();

  delay(5000);
}