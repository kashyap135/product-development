#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define MUX_ADDR 0x70
#define MUX_CH_ADS 2

Adafruit_ADS1115 ads;

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

  muxselect(MUX_CH_ADS);
  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("ADS1115 not found!");
    while (1);
  }
  ads.setGain(GAIN_ONE);
  Serial.println("ADS1115 ready.");
   Serial.println("Without Gas .");
}

void loop() {
  muxselect(MUX_CH_ADS);


  int16_t h2s_raw = ads.readADC_SingleEnded(0);
  int16_t co_raw  = ads.readADC_SingleEnded(1);
  int16_t ch4_raw = ads.readADC_SingleEnded(2);

  float h2s_volt = h2s_raw * 0.125 / 1000.0; // ADS1115 LSB = 0.125mV
  float co_volt  = co_raw * 0.125 / 1000.0;
  float ch4_volt = ch4_raw * 0.125 / 1000.0;

  Serial.print("H2S: "); Serial.print(h2s_volt); Serial.println(" V");
  Serial.print("CO: ");  Serial.print(co_volt);  Serial.println(" V");
  Serial.print("CH4: "); Serial.print(ch4_volt); Serial.println(" V");
  reset();

  delay(2000);
}