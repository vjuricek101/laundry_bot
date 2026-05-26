// mqtt_comms.ino
WiFiClient espClient;
PubSubClient mqttClient(espClient);

extern unsigned long cycleStartTime;
extern unsigned long cycleEndTime;

RTC_DATA_ATTR bool wasDeepSleep = false;
RTC_DATA_ATTR unsigned long rtcCycleStartTime = 0;
RTC_DATA_ATTR int rtcMachineState = 0;

void callback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  Serial.println(message);

  if (String(topic).endsWith("/cmd")) {
    if (message.indexOf("\"reset\"") != -1) {
      Serial.println("Remote reset command received.");
      extern void transitionTo(int);
      transitionTo(0); // IDLE
    }
  }
}

void setupMQTT() {
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

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      String cmdTopic =
          "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/cmd";
      mqttClient.subscribe(cmdTopic.c_str());

      String stateTopic = "laundry/" + String(BUILDING_ID) + "/" +
                          String(MACHINE_ID) + "/state";
      String payload =
          "{\"state\":\"idle\", \"machine_type\":\"" + machineType + "\"}";
      mqttClient.publish(stateTopic.c_str(), payload.c_str(), true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void updateMQTT() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
}

void publishCycleStart() {
  String topic = "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) +
                 "/cycle_start";
  String payload = "{\"start_time\":" + String(cycleStartTime) +
                   ", \"machine_type\":\"" + machineType +
                   "\", \"building\":\"" + String(BUILDING_ID) +
                   "\", \"machine_id\":\"" + String(MACHINE_ID) + "\"}";
  mqttClient.publish(topic.c_str(), payload.c_str());

  // Update state topic
  String stateTopic =
      "laundry/" + String(BUILDING_ID) + "/" + String(MACHINE_ID) + "/state";
  String statePayload =
      "{\"state\":\"running\", \"machine_type\":\"" + machineType + "\"}";
  mqttClient.publish(stateTopic.c_str(), statePayload.c_str(), true);
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
