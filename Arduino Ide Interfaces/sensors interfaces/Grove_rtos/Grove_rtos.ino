#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#define Vext 36
extern SSD1306Wire display;
SensirionI2cScd4x sensor;
// Shared data
volatile uint16_t co2 = 0;
volatile float temp = 0.0;
volatile float hum = 0.0;
volatile bool sensorOK = false;
// Task for sensor reading
void sensorTask(void *pvParameters) {
  for (;;) {
    uint16_t c; float t; float h;
    int16_t error = sensor.readMeasurement(c, t, h);
    if (!error) {
      co2 = c; temp = t; hum = h;
      Serial.print("CO2 [ppm]: "); Serial.println(co2);
      Serial.print("Temp [C]: "); Serial.println(temp);
      Serial.print("Humidity [%]: "); Serial.println(hum);
      sensorOK = true;
    } else {
      sensorOK = false;
    }
    vTaskDelay(pdMS_TO_TICKS(5000)); // wait 5s
  }
}
void oledTask(void *pvParameters) {
  for (;;) {
    display.clear();
    if (sensorOK) {
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 0, "CO2: " + String(co2) + " ppm");
      display.drawString(0, 12, "Temp: " + String(temp, 1) + " C");
      display.drawString(0, 24, "Hum: " + String(hum, 1) + " %");
    } else {
      display.drawString(0, 0, "Sensor error!");
    }
  }
}

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);   // LOW = ON
}
void setup() {
  Serial.begin(115200);

  // Power sensor
  VextON();
  delay(200);
  
  // Init OLED
  display.init();
  display.clear();
  display.drawString(0, 0, "OLED init OK");
  display.display();
  Serial.println("OLED initialized.");
Wire.begin(41, 42);

// Init sensor
sensor.begin(Wire, 0x62);

// Explicit reinit before starting
int16_t error = sensor.stopPeriodicMeasurement();
error = sensor.reinit();
if (error) {
  Serial.println("Error reinitializing sensor!");
} else {
  Serial.println("Sensor reinitialized.");
}
delay(100);

// Now start periodic measurement
error = sensor.startPeriodicMeasurement();
if (error) {
  Serial.println("Start measurement failed!");
} else {
  Serial.println("Measurement started.");
}


delay(5000); // warm-up
  

  // Create sensor task pinned to core 1
  xTaskCreatePinnedToCore(
    sensorTask,       // task function
    "SensorTask",     // name
    4096,             // stack size
    NULL,             // parameters
    1,                // priority
    NULL,             // task handle
    1                 // core
  );
// Create OLED task pinned to core 0
  xTaskCreatePinnedToCore(
    oledTask,          // task function
    "OLEDTask",        // name
    4096,              // stack size
    NULL,              // parameters
    1,                 // priority
    NULL,              // task handle
    0                  // core
  );
}

void loop() {
  // Nothing here — RTOS task handles sensor reads
}