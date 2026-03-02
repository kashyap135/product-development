#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>   /* SPI communication */
#include <Adafruit_Sensor.h> 
#include <Adafruit_ADS1X15.h>
#include <SensirionI2cScd4x.h>
#include "HT_SSD1306Wire.h"
#include "Adafruit_BME680.h"
#include "DFRobot_OxygenSensor.h"
#include "LoRaWan_APP.h" /* LoRa communication */
#define SEALEVELPRESSURE_HPA (1013.25)

// SPI pins
#define BME_CS   46
#define BME_MOSI 45
#define BME_MISO 40
#define BME_SCK  39
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
/* LoRa setup */
#define RF_FREQUENCY 433E6
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 255 

char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

// -------- Objects --------
// Create BME680 object (SPI)
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
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
void VextOFF() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);  // HIGH = OFF
}
// -------- MUX Select --------
void selectMux(uint8_t channel) {
  Wire1.beginTransmission(TCA_ADDR);
  Wire1.write(1 << channel);
  Wire1.endTransmission();
}
 /* LoRa Callbacks */
void OnTxDone(void) {
  Serial.println("   [LoRa] TX Finished");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("   [LoRa] TX Timeout");
  lora_idle = true;
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
    // Initialize LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // Sensor Bus
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  Wire1.setClock(100000);
  delay(500);
   // BME680 init
  if (!bme.begin()) {
    Serial.println("Could not find BME680 sensor!");
    display.drawString(0, 12, "BME680 ERROR");
    display.display();
    while (1) delay(1000);
  }
    // Configure oversampling & heater
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms

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
  Radio.IrqProcess(); 

  // ================= ADS READ =================
  selectMux(CH_ADS);
  delay(5);
   if (!bme.performReading()) {
    Serial.println("Failed to read BME680!");
    return;
  }
  // Collect data
  //float temp = bme.temperature;
  //float hum = bme.humidity;
  float pres = bme.pressure / 100.0; // hPa
  float gas = bme.gas_resistance / 1000.0; // KOhms
  float alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  float h2s = ads.computeVolts(ads.readADC_SingleEnded(0));
  float co  = ads.computeVolts(ads.readADC_SingleEnded(1));
  float ir  = ads.computeVolts(ads.readADC_SingleEnded(2));
  // Serial output
  //Serial.print("Temp: "); Serial.print(temp); Serial.println(" °C");
  //Serial.print("Hum: "); Serial.print(hum); Serial.println(" %");
  Serial.print("Pres: "); Serial.print(pres); Serial.println(" hPa");
  Serial.print("Gas: "); Serial.print(gas); Serial.println(" KOhms");
  Serial.print("Alt: "); Serial.print(alt); Serial.println(" m");

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
  Serial.print("ch4: ");  Serial.println(ir, 3);

  Serial.print("Oxygen: ");
  Serial.print(oxygenData);
  Serial.println(" %vol");

  if (!error) {
    Serial.print("H2S: "); Serial.println(h2s, 3);
  Serial.print("CO: ");  Serial.println(co, 3);
  Serial.print("ch4: ");  Serial.println(ir, 3);

  }
    // 2. LoRa Sender
  if (lora_idle == true) {
    if(!isnan(oxygenData ) && !isnan(co2)) {
      
    sprintf(txpacket,
  "O2: %.2f %%\nCO2: %.0f ppm\nTemp: %.1f C\nHum: %.1f %%\nPres: %.1f hPa\nGas: %.2f KOhm\nAlt: %.1f m\nH2S: %.2f V\nCO: %.2f V\nCH4: %.2f V",
  oxygenData,
  co2,
  temperature,
  humidity,
  pres,
  gas,
  alt,
  h2s,
  co,
  ir
);
      Serial.print("5. Sending LoRa Packet: ");
      Serial.println(txpacket);

      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "Sending LoRa...");
      display.display();
      
      lora_idle = false;
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      delay(1000); 
    } else {
        Serial.println("5. LoRa Send Skipped (Sensor Error)");
    }
  }


  // ================= OLED DISPLAY =================
  display.clear();
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "O2: " + String(oxygenData,1) + "%");
  display.drawString(0, 12, "CO2: " + String(co2) + "ppm");
  display.drawString(0, 24, "H2S: " + String(h2s,2));
  display.drawString(0, 36, "Temp: " + String(temperature,1));
  display.drawString(0, 36, "hum: " + String(humidity,1));

  display.display();
   delay(2000);

   display.clear();
  display.setFont(ArialMT_Plain_10);
   display.drawString(0, 24, "Pres: " + String(pres, 1) + " hPa");
   display.drawString(0, 36, "Gas: " + String(gas, 1) + " KOhm");
   display.drawString(0, 48, "Alt: " + String(alt, 1) + " m");
   display.display();

  delay(5000);
    // 1. Turn off the LoRa Radio
Radio.Sleep();

// 2. Turn off the OLED Screen
display.displayOff();

// 3. Turn off power to external sensors (Vext)
VextOFF(); 

// 4. Configure the wake-up timer (2 minutes in microseconds)
// 2 * 60 * 1,000,000 = 120,000,000
esp_sleep_enable_timer_wakeup(120 * 1000000ULL);

// 5. Go to sleep (The system basically shuts down here)
Serial.println("Going to sleep for 2 mins...");
esp_deep_sleep_start();
}
