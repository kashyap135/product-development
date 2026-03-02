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
#define BME_CS   46
#define BME_MOSI 45
#define BME_MISO 40
#define BME_SCK  39

/* ANALOG SETUP */
#define CO_PIN 7
#define IR13BD_PIN 6


//acknowledgmenet
#define TX_DONE_BIT      (1 << 0)
#define ACK_RECEIVED_BIT (1 << 1)
#define TX_FAILED_BIT    (1 << 2)
const String NODE_ID = "N1";
EventGroupHandle_t loraEvents;
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

int packetID = 0; //for lora

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
  vTaskDelay(pdMS_TO_TICKS(2000)); 

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
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(loraEvents, TX_DONE_BIT, &xHigherPriorityTaskWoken);
  lora_idle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("   [LoRa] TX Timeout");
  lora_idle = true;
}


void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  String msg = String((char*)payload).substring(0, size);
  // Check if the ACK is for THIS node
  if (msg == "ACK_" + NODE_ID) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(loraEvents, ACK_RECEIVED_BIT, &xHigherPriorityTaskWoken);
  }
}


uint8_t calculateCRC(char *data) {
  uint8_t crc = 0;
  while (*data) crc ^= *data++;
  return crc;
}

// 1. Added the semicolon here!
struct SensorData{
  float oxygen, co2, temp, humidity;
   float btemp, bhum, pressure, gas,altitude;
    float h2s, co, methane;
};

SemaphoreHandle_t muxMutex;
QueueHandle_t loraQueue;
QueueHandle_t displayQueue;

void TaskReadSensors(void *pvParameters){
  SensorData myData; 
  while(1){
    

    if (xSemaphoreTake(muxMutex, portMAX_DELAY) == pdTRUE){
      // These functions match your 'fsender.ino' helpers [cite: 8, 9, 10]
      myData.oxygen = getOxygenData();
      readScd41(myData.co2, myData.temp, myData.humidity);
      myData.h2s = readH2S(); 
      
      xSemaphoreGive(muxMutex);
    }

 
    readBme680(myData.btemp, myData.bhum, myData.pressure, myData.gas,myData.altitude);
    
    myData.co = readCarbonMonoxide();
    myData.methane = readMethane();

    xQueueSend(loraQueue, &myData, portMAX_DELAY);
    xQueueSend(displayQueue, &myData, portMAX_DELAY);
    
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void TaskSendLoRa(void *pvParameters) {
  SensorData receivedData;
  int retryCount = 0;
  const int MAX_RETRIES = 3;

  while (1) {
    if (xQueueReceive(loraQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
      bool acknowledged = false;
      retryCount = 0;

      while (retryCount < MAX_RETRIES && !acknowledged) {
        // 1. Prepare Packet with Node ID and CRC
        char payload[128];
        sprintf(payload, " A|ID=%d| %s o2:%.2f co2:%.0f s_t:%.1f s_h:%.1f b_t:%.1f b_h:%.1f b_p:%.1f b_g:%.2f alt:%.2f h2s:%.2f co:%.2f ch4:%.2f", packetID, NODE_ID.c_str(),
                receivedData.oxygen, receivedData.co2, receivedData.temp, receivedData.humidity, 
                receivedData.btemp, receivedData.bhum, receivedData.pressure, receivedData.gas, receivedData.altitude, 
                receivedData.h2s, receivedData.co, receivedData.methane);
        
        uint8_t crc = calculateCRC(payload);
        sprintf(txpacket, "%s|%02X", payload, crc);

        // 2. Send Packet
        lora_idle = false;
        Radio.Send((uint8_t *)txpacket, strlen(txpacket));

        // 3. Wait for TX to finish (Max 3 seconds)
        xEventGroupWaitBits(loraEvents, TX_DONE_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(3000));

        // 4. Switch to RX mode to listen for ACK (Wait 2 seconds)
        Radio.Rx(2000); 
        
        // 5. Wait for ACK bit to be set by OnRxDone
        EventBits_t bits = xEventGroupWaitBits(loraEvents, ACK_RECEIVED_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(2000));

        if ((bits & ACK_RECEIVED_BIT) != 0) {
          Serial.println("ACK Received! Node success.");
          acknowledged = true;
        } else {
          retryCount++;
          Serial.printf("No ACK. Retry %d/%d...\n", retryCount, MAX_RETRIES);
          // Jitter: Wait random time so nodes don't collide on retry
          vTaskDelay(pdMS_TO_TICKS(1000 + random(0, 500)));
        }
      }

      // 6. Final Step: Go to Deep Sleep for 5 mins
      if (acknowledged || retryCount >= MAX_RETRIES) {
          Serial.println("Task complete. Entering Deep Sleep...");
          vTaskDelay(pdMS_TO_TICKS(100)); // Let Serial finish
          esp_sleep_enable_timer_wakeup(5 * 60 * 1000000); // 5 mins
          esp_deep_sleep_start();
      }
    }
  }
}

void TaskUpdateDisplay(void *pvParameters) {
  SensorData receivedData;
  
  while(1) {
    // 1. Wait for a new box of data from the sensor task
    if (xQueueReceive(displayQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
      
      // --- SCREEN 1: AIR QUALITY ---
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "--- AIR QUALITY ---");
      display.drawString(0, 15, "O2: " + String(receivedData.oxygen) + " %");
      display.drawString(0, 30, "CO2: " + String(receivedData.co2) + " ppm");
      display.display();
      vTaskDelay(pdMS_TO_TICKS(3000)); // Show for 3 seconds

      // --- SCREEN 2: BME680 DATA ---
      display.clear();
      display.setFont(ArialMT_Plain_10);
      if (bme_found) {
        display.drawString(0, 0, "--- ENV DATA ---");
        display.drawString(0, 15, "Temp: " + String(receivedData.btemp) + " C");
        display.drawString(0, 30, "Hum: " + String(receivedData.bhum) + " %");
      } else {
        display.drawString(0, 15, "BME MISSING");
      }
      display.display();
      vTaskDelay(pdMS_TO_TICKS(3000)); 

      // --- SCREEN 3: GAS VOLTAGES ---
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "--- GAS VOLTS ---");
      display.drawString(0, 15, "H2S: " + String(receivedData.h2s) + " V");
      display.drawString(0, 30, "CO: " + String(receivedData.co) + " V");
      display.display();
      vTaskDelay(pdMS_TO_TICKS(3000)); 
    }
  }
}
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

loraEvents = xEventGroupCreate();

  muxMutex=xSemaphoreCreateMutex();
  loraQueue = xQueueCreate(10,sizeof(SensorData));
  displayQueue = xQueueCreate(10, sizeof(SensorData));
  xTaskCreate(TaskReadSensors,"sensorreadTask",4096,NULL,1,NULL);
  xTaskCreate(TaskSendLoRa,"LoRa_Task",4096,NULL,1,NULL);
  xTaskCreate(TaskUpdateDisplay,"OLED_Task",4096,NULL,0,NULL);
}

void loop() {
  // put your main code here, to run repeatedly:
vTaskDelete(NULL);
}
