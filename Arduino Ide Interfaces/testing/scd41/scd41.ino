#include <Wire.h>
#include <SensirionI2cScd4x.h>

#define MUX_ADDR 0x70
#define MUX_CH_SCD 1

SensirionI2cScd4x scd41;
int16_t error;

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
void setup() {
  Serial.begin(115200);
  Wire1.begin(41, 42);

  muxselect(MUX_CH_SCD);
  scd41.begin(Wire1, SCD41_I2C_ADDR_62);
  scd41.stopPeriodicMeasurement();
  scd41.reinit();
  Serial.println("SCD41 ready.");
}

void loop() {
  muxselect(MUX_CH_SCD);
  scd41.measureSingleShot();
  delay(5000);

  uint16_t co2;
  float temp, hum;
  error = scd41.readMeasurement(co2, temp, hum);

  if (!error) {
    Serial.print("CO2: "); Serial.print(co2); Serial.println(" ppm");
    Serial.print("Temp: "); Serial.print(temp); Serial.println(" °C");
    Serial.print("Hum: "); Serial.print(hum); Serial.println(" %");
  } else {
    Serial.println("SCD41 read error!");
  }
  delay(2000);
  reset();
}