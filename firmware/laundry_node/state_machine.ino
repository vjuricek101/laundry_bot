// state_machine.ino

enum MachineState { IDLE, STARTUP_WATCH, RUNNING, DONE };

MachineState currentState = IDLE;
unsigned long stateEnterTime = 0;

const float RMS_THRESHOLD = 0.15;
const unsigned long STARTUP_WATCH_TIME = 120000; // 2 minutes
const unsigned long DONE_DEBOUNCE_TIME = 30000;  // 30 seconds

// Track the start and end of the cycle for MQTT
unsigned long cycleStartTime = 0;
unsigned long cycleEndTime = 0;

// External publish functions (from mqtt_comms.ino)
extern void publishCycleStart();
extern void publishCycleEnd();
extern void publishAlert(String alertType, String detail);

void setLED(int r, int g, int b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

void transitionTo(MachineState newState) {
  currentState = newState;
  stateEnterTime = millis();
}

void setupStateMachine() {
  transitionTo(IDLE);
  setLED(LOW, LOW, LOW); // Off
}

void updateStateMachine() {
  // Check manual button press to reset
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Manual reset triggered via button.");
      transitionTo(IDLE);
      while (digitalRead(BUTTON_PIN) == LOW)
        delay(10); // Wait for release
    }
  }

  switch (currentState) {
  case IDLE:
    setLED(LOW, LOW, LOW); // Off
    if (smoothedRMS > RMS_THRESHOLD) {
      Serial.println("Vibration detected! Transition to STARTUP_WATCH.");
      transitionTo(STARTUP_WATCH);
    }
    break;

  case STARTUP_WATCH:
    // Amber blink (toggle every 500ms)
    if ((millis() / 500) % 2 == 0)
      setLED(HIGH, HIGH, LOW);
    else
      setLED(LOW, LOW, LOW);

    if (smoothedRMS < RMS_THRESHOLD) {
      Serial.println("Lost vibration during watch. False alarm/Failed start.");
      publishAlert("failed_start", "Vibration lost before 2 minutes");
      transitionTo(IDLE);
    } else if (millis() - stateEnterTime >= STARTUP_WATCH_TIME) {
      Serial.println("Vibration sustained. Transition to RUNNING.");
      cycleStartTime = millis();
      publishCycleStart();
      transitionTo(RUNNING);
    }
    break;

  case RUNNING:
    setLED(LOW, HIGH, LOW); // Green
    if (smoothedRMS < RMS_THRESHOLD) {
      // Wait 30 seconds before calling it done, just in case the machine is
      // pausing
      static unsigned long lowSince = 0;
      if (lowSince == 0)
        lowSince = millis();

      if (millis() - lowSince >= DONE_DEBOUNCE_TIME) {
        Serial.println("Cycle completed. Transition to DONE.");
        cycleEndTime = millis();
        publishCycleEnd();
        transitionTo(DONE);
        lowSince = 0; // reset
      }
    } else {
      // Vibration resumed, so reset the timer
    }
    break;

  case DONE:
    setLED(HIGH, LOW, LOW); // Red
    // Wait for button press or timeout
    if (millis() - stateEnterTime >= 1800000) { // 30 mins
      transitionTo(IDLE);
    }
    break;
  }
}
