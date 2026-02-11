#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "DFRobot_OxygenSensor.h"

// -------- Pin Definitions --------
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21

#define SENSOR_SDA 41
#define SENSOR_SCL 42
#define Vext       36
#define Oxygen_IICAddress ADDRESS_3

// 1. OLED Object (Uses default Wire on 17/18)
extern SSD1306Wire display;

// 2. Oxygen Sensor Object
// We pass &Wire1 HERE, in the constructor!
DFRobot_OxygenSensor oxygen(&Wire1); 

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void setup() {
  Serial.begin(115200);
  VextON();
  delay(500);

  // --- BUS 1: OLED (Internal) ---
  // Start the main I2C bus for the screen
  Wire.begin(OLED_SDA, OLED_SCL);
  
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Initializing...");
  display.display();

  // --- BUS 2: SENSOR (External) ---
  // Start the SECOND I2C bus for the sensor
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);

  // Initialize sensor (It now knows to use Wire1 because of the constructor)
  while(!oxygen.begin(Oxygen_IICAddress)){
    Serial.println("I2C error: Check Address/Wiring!");
    display.clear();
    display.drawString(0, 0, "Sensor Error!");
    display.display();
    delay(1000);
  }
  
  Serial.println("Oxygen Sensor Connected on Wire1!");
}

void loop() {
  // Read oxygen (uses Wire1 automatically)
  float oxygenData = oxygen.getOxygenData(10);

  // Print to Serial
  Serial.print("Oxygen: ");
  Serial.print(oxygenData);
  Serial.println(" %vol");

  // Update OLED (uses Wire automatically)
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Oxygen Level:");
  display.drawString(0, 25, String(oxygenData, 1) + " %vol");
  display.display();
  
  delay(1000);
}