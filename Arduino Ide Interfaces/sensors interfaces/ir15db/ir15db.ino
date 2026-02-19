#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "HT_SSD1306Wire.h"

#define ads1x15_SDA 41
#define ads1x15_SCL 42
#define Vext       36

#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
Adafruit_ADS1115 ads;
extern SSD1306Wire display;
// Variables
float co2_active, co2_ref, co2_ratio, co2_ppm;
float ch4_active, ch4_ref, ch4_ratio, ch4_ppm;
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void setup()
{
  
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
  Wire1.begin(ads1x15_SDA, ads1x15_SCL);
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Multi Gas Monitor");
  display.display();

  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("ADS1115 init failed!");
    while (1);
  }

  ads.setGain(GAIN_ONE);  // ±4.096V
  Serial.println("ADS1115 Ready");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Dual Gas Systemn ready");
  display.display();
  delay(1500);

}

void loop()
{
  // =====================
  // READ CO2
  // =====================
  int16_t raw_co2_A = ads.readADC_SingleEnded(0);
  int16_t raw_co2_R = ads.readADC_SingleEnded(1);

  co2_active = raw_co2_A * 0.125 / 1000.0;
  co2_ref    = raw_co2_R * 0.125 / 1000.0;

  if (co2_ref > 0.01)
  {
    co2_ratio = co2_active / co2_ref;
    co2_ppm = (1.0 - co2_ratio) * 2000;  // adjust after calibration
  }
  else co2_ppm = 0;

  // =====================
  // READ CH4
  // =====================
  int16_t raw_ch4_A = ads.readADC_SingleEnded(2);
  int16_t raw_ch4_R = ads.readADC_SingleEnded(3);

  ch4_active = raw_ch4_A * 0.125 / 1000.0;
  ch4_ref    = raw_ch4_R * 0.125 / 1000.0;

  if (ch4_ref > 0.01)
  {
    ch4_ratio = ch4_active / ch4_ref;
    ch4_ppm = (1.0 - ch4_ratio) * 5000;  // adjust after calibration
  }
  else ch4_ppm = 0;

  // =====================
  // SERIAL OUTPUT
  // =====================
  Serial.print("CO2: ");
  Serial.print(co2_ppm);
  Serial.print(" ppm  |  CH4: ");
  Serial.print(ch4_ppm);
  Serial.println(" ppm");

  // =====================
  // OLED DISPLAY
  // =====================
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawString(0, 0,  "CO2: " + String(co2_ppm, 0) + " ppm");
  display.drawString(0, 15, "CH4: " + String(ch4_ppm, 0) + " ppm");

  display.display();

  delay(1000);
}
