// mqtt_comms.ino
#include "config.h"
WiFiClient espClient;
PubSubClient mqttClient(espClient);

extern bool wasDeepSleep;
extern unsigned long cycleStartTime;
extern unsigned long cycleEndTime;

// NOTE: millis() resets to 0 after every deep-sleep wake, so cycleStartTime is
// relative to the current boot, not wall-clock time. A future improvement is to
// add NTP sync at startup and publish Unix timestamps instead.

// MQTT Callback Handler for incoming subscribed messages
void callback(char *topic, byte *payload, unsigned int length) {
  // Optimization: Construct the String directly from the payload buffer
  // instead of character-by-character concatenation. This avoids multiple
  // heap allocations and fragmentation.
  String message((char*)payload, length);

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  Serial.println(message);

  if (String(topic).endsWith("/cmd")) {
    if (message.indexOf("\"reset\"") != -1) {
      Serial.println("Remote reset command received.");
      extern void transitionTo(MachineState);
      transitionTo(IDLE); // IDLE
    }
  }
}

void setupMQTT() {
  Serial.print("ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress());
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
  mqttClient.setCallback(callback);
}

// Non-blocking reconnect variables
unsigned long lastMqttConnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // 5 seconds

extern float currentRPM;
extern float currentTempC;
extern float smoothedRMS;
extern String machineType;

bool connectMQTTNonBlocking() {
  if (mqttClient.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiWarn = 0;
    if (millis() - lastWifiWarn > 5000) {
      Serial.println("WiFi disconnected. Skipping MQTT reconnect.");
      lastWifiWarn = millis();
    }
    return false;
  }

  Serial.print("Attempting MQTT connection (non-blocking)...");
  // Use a deterministic client ID derived from MAC address so the broker
  // can distinguish reconnects from new clients (important for QoS 1 queuing).
  String clientId = "laundry-" + WiFi.macAddress();

  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("connected");
    String cmdTopic =
        "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/cmd";
    mqttClient.subscribe(cmdTopic.c_str());

    String stateTopic =
        "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/state";
    String payload =
        "{\"state\":\"idle\", \"machine_type\":\"" + machineType + "\"}";
    mqttClient.publish(stateTopic.c_str(), payload.c_str(), true);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

void updateMQTT() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttConnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttConnectAttempt = now;
      connectMQTTNonBlocking();
    }
  } else {
    mqttClient.loop();
  }
}

void publishStateUpdate() {
  String stateTopic =
      "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/state";
  String payload =
      "{\"state\":\"running\", \"machine_type\":\"" + machineType + "\"";
  // Always publish all three metrics. Temperature reflects cabinet heat for
  // dryers and ambient for washers. Dashboard decides what to display.
  payload += ", \"health_metrics\":{";
  payload += "\"temperature\":" + String(currentTempC);
  payload += ", \"vibration\":" + String(smoothedRMS * 100.0);
  payload += ", \"rpm\":" + String(currentRPM); // should be 0 for dryer...
  payload += "}}";
  mqttClient.publish(stateTopic.c_str(), payload.c_str(), true);
  Serial.print("Published state update: ");
  Serial.println(payload);
}

void publishCycleStart() {
  String topic = "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) +
                 "/cycle_start";
  String payload = "{\"start_time\":" + String(cycleStartTime) +
                   ", \"machine_type\":\"" + machineType +
                   "\", \"building\":\"" + String(BUILDING_ID) +
                   "\", \"machine_id\":\"" + String(MACHINE_ID) + "\"}";
  mqttClient.publish(topic.c_str(), payload.c_str());

  // Update state topic with full health metrics
  publishStateUpdate();
}

void publishCycleEnd() {
  String topic = "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) +
                 "/cycle_end";
  unsigned long durationMin = (cycleEndTime - cycleStartTime) / 60000;
  String payload = "{\"start_time\":" + String(cycleStartTime) +
                   ", \"end_time\":" + String(cycleEndTime) +
                   ", \"duration_min\":" + String(durationMin) +
                   ", \"machine_type\":\"" + machineType + "\"}";
  mqttClient.publish(topic.c_str(), payload.c_str());

  // Update state topic
  String stateTopic =
      "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/state";
  String statePayload =
      "{\"state\":\"done\", \"machine_type\":\"" + machineType + "\"}";
  mqttClient.publish(stateTopic.c_str(), statePayload.c_str(), true);
}

void publishAlert(String alertType, String detail) {
  String topic =
      "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/alert";
  String payload = "{\"alert_type\":\"" + alertType + "\", \"machine_id\":\"" +
                   String(MACHINE_ID) +
                   "\", \"timestamp\":" + String(millis()) + ", \"detail\":\"" +
                   detail + "\"}";
  mqttClient.publish(topic.c_str(), payload.c_str());
}

void deepSleepCycle() {
  extern MachineState currentState;
  if (currentState == IDLE) {
    Serial.println("Entering deep sleep for 8 seconds to save battery...");
    Serial.flush();
    wasDeepSleep = true;
    ESP.deepSleep(8e6); // 8 seconds
  }
}
