#include <Wire.h>
#include "DFRobot_OxygenSensor.h"

#define MUX_ADDR 0x70
#define MUX_CH_O2 4
#define Oxygen_IICAddress ADDRESS_3

DFRobot_OxygenSensor oxygen(&Wire1);

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
  Wire1.begin(41, 42); // SDA, SCL for MUX

  muxselect(MUX_CH_O2);
  if (!oxygen.begin(Oxygen_IICAddress)) {
    Serial.println("O2 Sensor not found!");
  } else {
    Serial.println("O2 Sensor ready.");
  }
}

void loop() {
  muxselect(MUX_CH_O2);
  float o2 = oxygen.getOxygenData(20);
  Serial.print("Oxygen: ");
  Serial.print(o2);
  Serial.println(" %");
  reset();
  delay(2000);
}