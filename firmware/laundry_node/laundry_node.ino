#include "config.h"
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>

// --- Globals ---
extern Adafruit_ADXL345_Unified accel;
extern WiFiClient espClient;
extern PubSubClient mqttClient;

// Pins
const int TMP36_PIN = A2;
const int LED_R = 15;
const int LED_G = 33;
const int LED_B = 27;
const int BUTTON_PIN = 38;

// Shared State Variables
extern float currentRMS;
extern float smoothedRMS;
extern float currentTempC;
extern bool isDryer;
extern String machineType;

// Function Prototypes
void setupAccel();
void updateAccel();
void setupTemp();
void updateTemp();
void setupStateMachine();
void updateStateMachine();
void setupMQTT();
void updateMQTT();
void deepSleepCycle();

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setupAccel();
  setupTemp();
  setupMQTT();
  setupStateMachine();

  Serial.println("Laundry Node initialized.");
}

void loop() {
  // Update sub-systems
  updateAccel();
  updateTemp();
  updateMQTT();
  updateStateMachine();

  // Deep sleep when node is in IDLE to save power
  deepSleepCycle();
}
