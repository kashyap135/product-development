#define GPIO_OUT_W1TS_REG (*(volatile unsigned int*)0x3FF44008) // set bits
#define GPIO_OUT_W1TC_REG (*(volatile unsigned int*)0x3FF4400C) // clear bits
#define GPIO_ENABLE_REG   (*(volatile unsigned int*)0x3FF44020)

#define LED_PIN 13   // use GPIO13 since your LED is connected there

void led_init(void) {
    GPIO_ENABLE_REG |= (1 << LED_PIN);   // enable GPIO13 as output
}

void led_on(void) {
    GPIO_OUT_W1TS_REG = (1 << LED_PIN);  // drive GPIO13 HIGH
}

void led_off(void) {
    GPIO_OUT_W1TC_REG = (1 << LED_PIN);  // drive GPIO13 LOW
}

void setup() {
    led_init();
    Serial.begin(115200);
}

void loop() {
    led_on();
    delay(1000);
    Serial.println("LED ON (GPIO13)");

    led_off();
    delay(1000);
    Serial.println("LED OFF (GPIO13)");
}
/*
#include <Arduino.h>
#include "soc/io_mux_reg.h"
#include "driver/gpio.h"

// Direct register access
#define GPIO_OUT_W1TS_REG (*(volatile unsigned int*)0x3FF44008) // Write‑1‑to‑Set
#define GPIO_OUT_W1TC_REG (*(volatile unsigned int*)0x3FF4400C) // Write‑1‑to‑Clear
#define GPIO_ENABLE_REG   (*(volatile unsigned int*)0x3FF44020) // Enable output

#define LED_PIN 13   // external LED connected to GPIO13

// Initialize GPIO13 as output and route it to GPIO controller
void led_init(void) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[LED_PIN], PIN_FUNC_GPIO); // route pin to GPIO
    GPIO_ENABLE_REG |= (1 << LED_PIN);   // enable GPIO13 as output
}

// Turn LED ON (drive pin HIGH)
void led_on(void) {
    GPIO_OUT_W1TS_REG = (1 << LED_PIN);  // set bit → HIGH
}

// Turn LED OFF (drive pin LOW)
void led_off(void) {
    GPIO_OUT_W1TC_REG = (1 << LED_PIN);  // clear bit → LOW
}

// Toggle LED state
void led_toggle(void) {
    static bool state = false;
    state = !state;
    if (state) {
        led_on();
    } else {
        led_off();
    }
}

void setup() {
    Serial.begin(115200);
    led_init();
    Serial.println("GPIO13 LED initialized");
}

void loop() {
    led_on();
    Serial.println("LED ON (GPIO13)");
    delay(1000);

    led_off();
    Serial.println("LED OFF (GPIO13)");
    delay(1000);

    led_toggle();
    Serial.println("LED TOGGLED (GPIO13)");
    delay(1000);
}*/