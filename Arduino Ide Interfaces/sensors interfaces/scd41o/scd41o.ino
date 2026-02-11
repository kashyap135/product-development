#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// -------- Pin Definitions (Heltec V3) --------
#define SENSOR_SDA 41
#define SENSOR_SCL 42
#define Vext       36

#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21

// 1. Create Sensor Object (Empty constructor)
SensirionI2cScd4x sensor;

// 2. Use Existing Display Object
extern SSD1306Wire display;

static char errorMessage[64];
static int16_t error;

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); // Turn on power
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  // Power up
  VextON();
  delay(1000); 

  // --- BUS 1: OLED (Internal) ---
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Initializing SCD41...");
  display.display();

  // --- BUS 2: SENSOR (External) ---
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);

  // Initialize Sensor using Wire1
  sensor.begin(Wire1, SCD41_I2C_ADDR_62);

  // Stop any previous periodic measurement to ensure clean state
  sensor.stopPeriodicMeasurement();
  sensor.reinit();

  uint64_t serialNumber;
  error = sensor.getSerialNumber(serialNumber);
  if (error) {
    Serial.print("Sensor Error: ");
    errorToString(error, errorMessage, 64);
    Serial.println(errorMessage);
    
    display.clear();
    display.drawString(0, 0, "Sensor Error!");
    display.drawString(0, 15, "Check Wiring");
    display.display();
    while(1); // Stop here if sensor fails
  }

  Serial.println("SCD41 Detected!");
}

void loop() {
  uint16_t co2Concentration = 0;
  float temperature = 0.0;
  float relativeHumidity = 0.0;

  // 1. Wake up sensor
  sensor.wakeUp();
  
  // 2. Measure (Single Shot takes about 5 seconds)
  Serial.println("Measuring...");
  display.clear();
  display.drawString(0, 0, "Measuring...");
  display.display();

  sensor.measureSingleShot(); // This blocks internally until ready or use delay
  // Note: Single shot usually requires ~5 seconds wait. 
  // The library might handle this, but adding a small delay is safe.
  delay(5000); 

  // 3. Read Data
  error = sensor.readMeasurement(co2Concentration, temperature, relativeHumidity);

  if (error) {
    Serial.print("Read Error: ");
    errorToString(error, errorMessage, 64);
    Serial.println(errorMessage);
  } else {
    // Print to Serial
    Serial.print("CO2: "); Serial.print(co2Concentration); Serial.println(" ppm");
    Serial.print("Temp: "); Serial.print(temperature); Serial.println(" C");
    Serial.print("Hum: "); Serial.print(relativeHumidity); Serial.println(" %");

    // Update Display
    // Using larger font, we must space out the Y coordinates (0, 20, 40)
    display.clear();
    display.setFont(ArialMT_Plain_16);
    
    display.drawString(0, 0,  "CO2: " + String(co2Concentration) + " ppm");
    display.drawString(0, 20, "Tmp: " + String(temperature, 1) + " C");
    display.drawString(0, 40, "Hum: " + String(relativeHumidity, 1) + " %");
    
    display.display();
  }

  // 4. Sleep Sensor (Save battery)
  //sensor.sleep();
  
  Serial.println("Sleeping for 5 mins...");
  // Delay 5 minutes (300,000 ms)
  // For a real battery app, you would deep sleep the ESP32 here.
  delay(300000); 
}