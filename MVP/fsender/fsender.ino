/*header files */
#include <Arduino.h>
#include <Wire.h>  /* IIC communication */
#include <SPI.h>   /* SPI communication */
#include <Adafruit_Sensor.h> 
#include "Adafruit_BME680.h" /* BME680 */
#include "DFRobot_OxygenSensor.h" /* O2 sensor */
#include <SensirionI2cScd4x.h>  /* SCD41 sensor */
#include "HT_SSD1306Wire.h" /* OLED library for Heltec board */
#include "LoRaWan_APP.h" /* LoRa communication */
#include <Adafruit_ADS1X15.h> /* adc library */

/*============== Definition section =====================*/

/* OLED setup */
#define Vext 36 //POWER pin
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21

/* MUX setup */
#define MUX_SDA 41
#define MUX_SCL 42
#define MUX_ADDR 0x70

/* MUX CHANNELS DEFINITION */
#define MUX_CH_O2  0
#define MUX_CH_SCD 1
#define MUX_CH_ADS 3 
// Note: BME is NOT on Mux anymore, it is on SPI pins

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

/* seno322 setup*/
#define Oxygen_IICAddress ADDRESS_3 

/* BME680 SPI CONFIGURATION */
#define SEALEVELPRESSURE_HPA (1013.25)

// === IMPORTANT: WIRE BME680 TO THESE PINS ===
#define BME_SCK  33
#define BME_MISO 34
#define BME_MOSI 35
#define BME_CS   26 

/* ANALOG SETUP */
#define CO_PIN 7
#define IR13BD_PIN 6

extern SSD1306Wire display;

static char errorMessage[64];
static int16_t error;
bool bme_found = false; 

/* constructor/objects */
DFRobot_OxygenSensor oxygen(&Wire1); 
SensirionI2cScd4x sensor;

// Software SPI Constructor (Passes pins explicitly)
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);

Adafruit_ADS1115 ads;

//OLED power setup
void VextON() {
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext,LOW);
}

void VextOFF() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);  // HIGH = OFF
}

/* MUX SELECTION(helper function) */
void muxselect(uint8_t ch) {
  if (ch > 7) return;
  Wire1.beginTransmission(MUX_ADDR);
  Wire1.write(1 << ch);
  Wire1.endTransmission();
  delay(5);
}

/* SENSOR reading Helper functions */
float getOxygenData() {
  muxselect(MUX_CH_O2);
  delay(10); 
  float data = oxygen.getOxygenData(20); 
  return data;
}

void readScd41(float &co2, float &temp, float &hum) {
  muxselect(MUX_CH_SCD);
  sensor.wakeUp();
  
  Serial.println("   [SCD41] Measuring... (Waiting 5s)");
  sensor.measureSingleShot();
  delay(5000); 

  uint16_t co2Concentration = 0;
  float temperature = 0.0;
  float relativeHumidity = 0.0;

  error = sensor.readMeasurement(co2Concentration, temperature, relativeHumidity);

  if (!error) {
    co2 = (float)co2Concentration;
    temp = temperature;
    hum = relativeHumidity;
  } else {
    Serial.println("   [Error] SCD41 Read Failed");
    co2 = NAN; 
  }
}

void readBme680(float &temp, float &hum, float &pres, float &gas, float &alti) {
  // Direct SPI read (No Mux needed)
  if (!bme.performReading()) {
    Serial.println("   [Error] BME680 Read Failed!");
    temp = NAN; 
    return;
  }

  temp = bme.temperature;
  hum = bme.humidity;
  pres = bme.pressure / 100.0;
  gas = bme.gas_resistance / 1000.0;
  alti = bme.readAltitude(SEALEVELPRESSURE_HPA);
}

float readH2S() {
  muxselect(MUX_CH_ADS); 
  int16_t adc0 = ads.readADC_SingleEnded(0);
  float voltage = adc0 * 0.125 / 1000.0; 
  return voltage;
}

float readCarbonMonoxide(){
  int rawvalue = analogRead(CO_PIN);
  float voltage = (rawvalue / 4095.0) * 3.3;
  return voltage; 
}

float readMethane() {
  int irRaw = analogRead(IR13BD_PIN);
  float irVoltage = (irRaw / 4095.0) * 3.3; 
  return irVoltage;
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

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(1000);

  VextON();
  delay(200);
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Initializing...");
  display.display();

Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  // Initialize LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // Init Sensor Bus
  Wire1.begin(MUX_SDA, MUX_SCL, 100000);

  /*=========== Oxygen sensor setup (MUX 0) ============ */
  muxselect(MUX_CH_O2);
  if(!oxygen.begin(Oxygen_IICAddress)){
    Serial.println("O2 Error");
  } else {
    Serial.println("O2 Sensor Connected!");
  }

  /*========== SCD41 sensor setup (MUX 1) ==========*/
  muxselect(MUX_CH_SCD);
  sensor.begin(Wire1, SCD41_I2C_ADDR_62);
  sensor.stopPeriodicMeasurement();
  sensor.reinit();
  uint64_t serialNumber;
  error = sensor.getSerialNumber(serialNumber);
  if (error) {
    Serial.println("SCD41 Init Error");
  } else {
    Serial.println("SCD41 Detected!");
  }

  /*========== BME680 sensor setup (SPI) ==========*/
  // Note: Using Software SPI pins defined at top
  if (!bme.begin()) {    
    Serial.println("Could not find BME680 (Check Wiring!)");
    bme_found = false;
  } else {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); 
    Serial.println("BME680 Initialized (SPI)");
    bme_found = true;
  }

  /*================== ADS1115 (MUX 3) ====================*/
  muxselect(MUX_CH_ADS); 
  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("Failed to initialize ADS1115!");
  } else {
    ads.setGain(GAIN_ONE); 
    Serial.println("ADS1115 initialized!");
  }

  display.clear();
  display.drawString(0, 0, "System Ready!");
  display.display();
  delay(2000);
}

/* ================= LOOP ================= */
void loop() {
  Radio.IrqProcess(); 

  // 1. Gather Data with FULL DEBUGGING
  Serial.println("\n--------------------------------");
  Serial.println("       STARTING NEW READ        ");
  Serial.println("--------------------------------");
  
  // -- Oxygen --
  Serial.print("1. Oxygen: ");
  float o2_val = getOxygenData();
  Serial.print(o2_val); Serial.println(" %");

  // -- SCD41 --
  Serial.print("2. SCD41: ");
  float scd_co2 = 0, scd_temp = 0, scd_hum = 0;
  readScd41(scd_co2, scd_temp, scd_hum); // Prints "Waiting..." inside
  Serial.print("   -> CO2: "); Serial.print(scd_co2); Serial.println(" ppm");
  Serial.print("   -> Temp: "); Serial.print(scd_temp); Serial.println(" C");
  Serial.print("   -> Hum: "); Serial.print(scd_hum); Serial.println(" %");

  // -- BME680 --
  float bme_temp = 0, bme_hum = 0, bme_pres = 0, bme_gas = 0, bme_alt = 0;
  if (bme_found) {
    Serial.println("3. BME680 (SPI): Reading...");
    readBme680(bme_temp, bme_hum, bme_pres, bme_gas, bme_alt);
    Serial.print("   -> Temp: "); Serial.print(bme_temp); Serial.println(" C");
    Serial.print("   -> Hum: "); Serial.print(bme_hum); Serial.println(" %");
    Serial.print("   -> Pres: "); Serial.print(bme_pres); Serial.println(" hPa");
    Serial.print("   -> Gas: "); Serial.print(bme_gas); Serial.println(" KOhms");
  } else {
    Serial.println("3. BME680: SKIPPED (Not Found)");
  }
  
  // -- Analog --
  Serial.println("4. Analog Sensors:");
  float h2s_volts = readH2S();
  Serial.print("   -> H2S: "); Serial.print(h2s_volts); Serial.println(" V");
  
  float co_volts = readCarbonMonoxide();
  Serial.print("   -> CO: "); Serial.print(co_volts); Serial.println(" V");
  
  float ch4_volts = readMethane();
  Serial.print("   -> CH4: "); Serial.print(ch4_volts); Serial.println(" V");

  // 2. LoRa Sender
  if (lora_idle == true) {
    if(!isnan(o2_val) && !isnan(scd_co2)) {
      
      sprintf(txpacket, 
        "o2:%.2f co2:%.0f s_t:%.1f s_h:%.1f b_t:%.1f b_h:%.1f b_p:%.1f b_g:%.2f h2s:%.2f co:%.2f ch4:%.2f", 
        o2_val, scd_co2, scd_temp, scd_hum, bme_temp, bme_hum, bme_pres, bme_gas, h2s_volts, co_volts, ch4_volts
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

  // 3. Cycle Display on OLED
  display.clear();
  display.drawString(0, 0, "--- AIR QUALITY ---");
  display.drawString(0, 15, "O2: " + String(o2_val) + " %");
  display.drawString(0, 30, "CO2: " + String(scd_co2) + " ppm");
  display.display();
  delay(3000);

  display.clear();
  display.drawString(0, 0, "--- ENV DATA ---");
  if (bme_found) {
    display.drawString(0, 15, "Temp: " + String(bme_temp) + " C");
    display.drawString(0, 30, "Hum: " + String(bme_hum) + " %");
  } else {
    display.drawString(0, 15, "BME MISSING");
  }
  display.display();
  delay(3000);

  display.clear();
  display.drawString(0, 0, "--- GAS VOLTS ---");
  display.drawString(0, 15, "H2S: " + String(h2s_volts) + " V");
  display.drawString(0, 30, "CO: " + String(co_volts) + " V");
  display.display();
  delay(3000);
  


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