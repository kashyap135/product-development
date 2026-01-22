#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ravi nani";
const char* password = "9912831367";

WebServer server(80);
int ledPin = 13;

void handleRoot() {
  server.send(200, "text/html", "<h1>ESP32 LED Control</h1><a href=\"/on\">ON</a> | <a href=\"/off\">OFF</a>");
}

void handleOn() {
  digitalWrite(ledPin, HIGH);
  server.send(200, "text/html", "LED is ON");
}

void handleOff() {
  digitalWrite(ledPin, LOW);
  server.send(200, "text/html", "LED is OFF");
}

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.begin();
}

void loop() {
  server.handleClient();
}