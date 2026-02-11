#include <Arduino.h>

// Choose pins that exist on your board
static const int LED_PIN = 2;     // use GPIO2 (often has onboard LED); change if needed
static const int ADC_PIN = 34;    // valid ADC input-only pin on ESP32

// Task 1: Blink LED (periodic, 500 ms)
void TaskBlink(void *pvParameters) {
    pinMode(LED_PIN, OUTPUT);
    TickType_t xLastWakeTime = xTaskGetTickCount(); // anchor for precise timing

    for (;;) {
        Serial.print("TaskBlink running at: ");
        Serial.println(xTaskGetTickCount() * portTICK_PERIOD_MS);

        digitalWrite(LED_PIN, HIGH);
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);

        digitalWrite(LED_PIN, LOW);
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);
    }
}

// Task 2: Sensor read (periodic, 1000 ms)
void TaskSensor(void *pvParameters) {
    // Optional: small settle delay for ADC
    vTaskDelay(100 / portTICK_PERIOD_MS);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        Serial.print("TaskSensor running at: ");
        Serial.println(xTaskGetTickCount() * portTICK_PERIOD_MS);

        int sensorValue = analogRead(ADC_PIN);  // read from GPIO34
        Serial.print("Sensor Value: ");
        Serial.println(sensorValue);

        vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    // Create tasks with safe stack sizes (bytes on ESP32)
    xTaskCreate(TaskBlink,  "Blink",  2048, NULL, 1, NULL);
    xTaskCreate(TaskSensor, "Sensor", 3072, NULL, 1, NULL);

    // Do NOT call vTaskStartScheduler() on ESP32 Arduino
}

void loop() {
    // Empty: FreeRTOS scheduler is already running
}