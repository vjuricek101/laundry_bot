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

// Hardware Pin Mappings
const int TMP36_PIN = A2;      // Analog input pin for TMP36 temperature sensor
const int LED_R = 15;          // Red LED pin for DONE/ALERT physical status
const int LED_G = 33;          // Green LED pin for RUNNING physical status
const int BUTTON_PIN = 38;      // Physical push-button for manual state reset

// Shared State Variables across module files (accel, temp, rpm, state_machine, mqtt_comms)
extern float currentRMS;
extern float smoothedRMS;
extern float currentTempC;
extern float currentRPM;
bool wasDeepSleep = false;     // Globally defined to track deep-sleep wake cycles (fixes compile error)

// machineType is set once from config.h and used by the state machine and MQTT
// payloads. It is never changed at runtime — machine identity is a deployment property.
String machineType = String(MACHINE_TYPE);

// Function Prototypes
void setupAccel();
void updateAccel();
void setupTemp();
void updateTemp();
void setupRPM();
void updateRPM();
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
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // SDA=GPIO20, SCL=GPIO22
  Wire.begin(22, 20);
  setupAccel();
  setupTemp();
  setupRPM();
  setupMQTT();
  setupStateMachine();

  Serial.println("Laundry Node initialized.");
}

void loop() {
  updateAccel();        // Sample ADXL345 at 50 Hz; updates currentRMS and smoothedRMS
  updateTemp();         // Read TMP36 every 30 s; reports currentTempC (no classification)
  updateRPM();          // Consume ISR flag; compute currentRPM from beamInterval
  updateMQTT(); // Non-blocking reconnect + pump the PubSub client loop
  updateStateMachine(); // Evaluate smoothedRMS/RPM against thresholds; publish
                        // events
  deepSleepCycle();     // If IDLE, enter 8 s deep sleep to conserve power
}
