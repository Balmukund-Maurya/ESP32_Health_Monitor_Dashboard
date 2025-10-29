/***************************************
 * Health Monitoring System - ESP32
 * Sensors: MAX30102, DHT11, MPU6050, LDR
 * Firebase Realtime Database + Telegram Alerts
 * Author: Balmukund Maurya
 ***************************************/

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "MAX30105.h"      
#include "DHT.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>

// ---------------- Wi-Fi ----------------
const char* WIFI_SSID = "wifi";
const char* WIFI_PASSWORD = "23456789";

// ---------------- Firebase ----------------
const char* FIREBASE_HOST = "https://example-default-.firebaseio.com"; // e.g. https://example-default-.firebaseio.com
const char* FIREBASE_API_KEY = "AIzgvfaSyDHTEtO7k2D4qP0ZTbKJfLLY4iKAA"; // e.g. AIzaSyDHTEtO7k2D4qP0ZTbKJfLBVm8iLdDSM

// ---------------- Telegram ----------------
const char* TELEGRAM_BOT_TOKEN = "84574774796:AATpAoHbkjbnjknkjnghiuhWMJk3iyEIm7QOaeukn8"; //e.g 8401574796:AAEcOTjvjhAS3iyEI4p0pV7m7QOaeukn8
const char* TELEGRAM_CHAT_ID = "7315545454"; // e.g. 7315465634

// ---------------- Sensors ----------------
MAX30105 particleSensor;
#define DHTPIN 5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
Adafruit_MPU6050 mpu;
#define LDR_PIN 34

// ---------------- Timing ----------------
unsigned long previousSensorMillis = 0;
const long sensorInterval = 1000; // 1 second

// ---------------- Thresholds ----------------
const int HR_HIGH = 120;  // Heart rate high threshold
const int HR_LOW  = 50;   // Heart rate low threshold
const float FALL_ACCEL_THRESHOLD = 15.0; // G-force for fall detection

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Wi-Fi Connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");

  // DHT11
  dht.begin();

  // MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found! Check wiring.");
    while (1);
  }
  particleSensor.setup(); // Default settings

  // MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found! Check wiring.");
    while (1);
  }

  // LDR
  pinMode(LDR_PIN, INPUT);

  Serial.println("All sensors initialized successfully!");
}

// ---------------- Loop ----------------
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorMillis >= sensorInterval) {
    previousSensorMillis = currentMillis;

    // --- Read Sensors ---

    // MAX30102 raw readings
    long irValue = particleSensor.getIR();
    long redValue = particleSensor.getRed();

    // Estimate demo values for dashboard visualization
    int heartRate = map(irValue % 1000, 0, 1000, 60, 100); // Simulated heart rate
    int spo2 = map(redValue % 1000, 0, 1000, 90, 100);     // Simulated SpO2 %

    // DHT11
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // MPU6050
    sensors_event_t a, g, tempEvent;
    mpu.getEvent(&a, &g, &tempEvent);
    float accelX = a.acceleration.x;
    float accelY = a.acceleration.y;
    float accelZ = a.acceleration.z;
    float totalAccel = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);

    // LDR
    int lightLevel = analogRead(LDR_PIN);

    // --- Print for Debug ---
    Serial.println("----- SENSOR READINGS -----");
    Serial.print("Heart Rate: "); Serial.println(heartRate);
    Serial.print("SpO2: "); Serial.println(spo2);
    Serial.print("Room Temp: "); Serial.println(temperature);
    Serial.print("Humidity: "); Serial.println(humidity);
    Serial.print("Acceleration: "); Serial.println(totalAccel);
    Serial.print("Light Level: "); Serial.println(lightLevel);
    Serial.println("----------------------------");

    // --- Send Data to Firebase ---
    if (WiFi.status() == WL_CONNECTED) {
      sendDataToFirebase(heartRate, spo2, temperature, humidity, totalAccel, lightLevel);
    }

    // --- Telegram Alerts ---
    if (heartRate > HR_HIGH || heartRate < HR_LOW) {
      sendTelegramAlert("Abnormal Heart Rate Detected!");
    }
    if (totalAccel > FALL_ACCEL_THRESHOLD) {
      sendTelegramAlert("Fall Detected!");
    }
  }
}

// ---------------- Functions ----------------

// Send sensor data to Firebase
void sendDataToFirebase(int hr, int spo2, float temp, float hum, float accel, int light) {
  HTTPClient http;
  String url = String(FIREBASE_HOST) + "/sensorData.json?auth=" + FIREBASE_API_KEY;

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["heartRate"] = hr;
  jsonDoc["SpO2"] = spo2;
  jsonDoc["temperature"] = temp;
  jsonDoc["humidity"] = hum;
  jsonDoc["acceleration"] = accel;
  jsonDoc["lightLevel"] = light;
  jsonDoc["timestamp"] = millis();

  String payload;
  serializeJson(jsonDoc, payload);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.PUT(payload);

  if (httpResponseCode > 0) {
    Serial.print("Firebase Updated (code ");
    Serial.print(httpResponseCode);
    Serial.println(")");
  } else {
    Serial.print("Firebase Error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  http.end();
}

// Send Telegram alert
void sendTelegramAlert(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) +
                 "/sendMessage?chat_id=" + String(TELEGRAM_CHAT_ID) +
                 "&text=" + message;

    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Telegram Alert Sent!");
    } else {
      Serial.print("Telegram Error: ");
      Serial.println(http.errorToString(httpCode));
    }
    http.end();
  }
}