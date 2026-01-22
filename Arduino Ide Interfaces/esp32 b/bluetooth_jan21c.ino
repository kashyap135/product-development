#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_MILIEU");
  Serial.println("Bluetooth Started.Connect to ESP32_MILIEU");
  pinMode(13, OUTPUT);
}

void loop() {
  char w;
  w = SerialBT.read();
  if (Serial.available()) {
    SerialBT.write(Serial.read()); 
  }
  if (SerialBT.available()) {
  Serial.write(w); 
  }
  if(w == 'f')
  {
    digitalWrite(13, HIGH);
  }
  else if (w == 'h')
  {
    digitalWrite(13, LOW);
  }
}
