// state_machine.ino
#include "config.h"

MachineState currentState = IDLE;
unsigned long stateEnterTime = 0;

extern String machineType;
extern float currentRPM;

const float RMS_THRESHOLD = 0.15;
const unsigned long STARTUP_WATCH_TIME = 120000; // 2 minutes
const unsigned long DONE_DEBOUNCE_TIME =
    5000; // 5 seconds (tuned to EMA τ ≈ 980 ms)

// Track the start and end of the cycle for MQTT
unsigned long cycleStartTime = 0;
unsigned long cycleEndTime = 0;

// Moved to file scope (not inside switch cases) to avoid C++ ambiguity
// warnings.
unsigned long lastStatePublishTime =
    0; // throttles publishStateUpdate() to every 5 s
unsigned long lowSince =
    0; // tracks how long smoothedRMS has been below threshold

// External publish functions (from mqtt_comms.ino)
extern void publishCycleStart();
extern void publishCycleEnd();
extern void publishAlert(String alertType, String detail);

void setLED(int r, int g) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
}

void transitionTo(MachineState newState) {
  currentState = newState;
  stateEnterTime = millis();
}

void setupStateMachine() {
  transitionTo(IDLE);
  setLED(LOW, LOW); // Off
}

void updateStateMachine() {
  // Non-blocking button debounce: detects a clean falling edge (HIGH->LOW)
  // with a 50 ms settle window. Does not block sensor sampling or MQTT loop.
  static bool lastButtonReading = HIGH;
  static unsigned long lastDebounceTime = 0;
  static bool buttonHandled = false;
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = reading;
    buttonHandled = false;
  }
  if (!buttonHandled && (millis() - lastDebounceTime > 50)) {
    if (reading == LOW) {
      Serial.println("Manual reset triggered via button.");
      transitionTo(IDLE);
      buttonHandled = true;
    }
  }

  switch (currentState) {
  case IDLE:
    setLED(LOW, LOW); // Off
    // Wait for sustained vibration above threshold to start the watch window.
    // No telemetry is published in IDLE.
    //
    // CRITICAL SYSTEM DESIGN WARNING:
    // If the node enters deep sleep (implemented in deepSleepCycle() inside mqtt_comms.ino)
    // during IDLE, the entire system resets on wake. This means the circular buffers in RAM
    // (rmsSumSqBuffer for RMS, rmsHistory for moving median) are completely wiped.
    // Consequently, the DSP pipeline will never have more than a single sample upon waking,
    // rendering the sliding filters useless and causing frequent Wi-Fi reconnect overhead (high power consumption!).
    // Recommended Fix: Use ESP32 Light Sleep, or configure the ADXL345 to trigger a hardware activity interrupt
    // on a GPIO pin to wake the ESP32 from deep sleep only when actual vibration is detected.
    if (smoothedRMS > RMS_THRESHOLD) {
      Serial.println("Vibration detected! Transition to STARTUP_WATCH.");
      transitionTo(STARTUP_WATCH);
    }
    break;

  case STARTUP_WATCH:
    // LED: amber blink (both channels toggled every 500 ms)
    if ((millis() / 500) % 2 == 0)
      setLED(HIGH, HIGH);
    else
      setLED(LOW, LOW);

    // If vibration drops before the 2-minute window expires, treat as a
    // false alarm (e.g., someone bumping the machine) and return to IDLE.
    // If vibration is sustained for 2 minutes, confident a real cycle
    // has started and commit to RUNNING.
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
    setLED(LOW, HIGH); // Green

    // Publish a full telemetry snapshot every 5 seconds: temperature, vibration
    // (smoothedRMS %), and RPM. All three are always included regardless of
    // machine type so the logged CSV contains complete time-series data for
    // post-processing and anomaly detection (e.g., heat curves, RPM profiles).
    if (millis() - lastStatePublishTime >= 5000) {
      lastStatePublishTime = millis();
      extern void publishStateUpdate();
      publishStateUpdate();
    }

    // Diagnostic alert: if the machine is a washer, has been running, and vibration remains high
    // but drum rotation (RPM) drops to zero for more than 30 seconds, flag a broken belt alert.
    static unsigned long zeroRpmSince = 0;
    if (machineType == "washer" && currentRPM == 0.0F && smoothedRMS >= RMS_THRESHOLD) {
      if (zeroRpmSince == 0) {
        zeroRpmSince = millis();
      } else if (millis() - zeroRpmSince >= 30000) { // 30 seconds
        static unsigned long lastBeltAlert = 0;
        if (millis() - lastBeltAlert >= 300000) { // throttle alerts to every 5 minutes
          lastBeltAlert = millis();
          publishAlert("broken_belt", "Vibration remains high but drum RPM has dropped to zero (potential broken belt).");
        }
      }
    } else {
      zeroRpmSince = 0;
    }

    // Cycle-end FSM trigger (separate from telemetry collection above):
    //   Washer: requires smoothedRMS to drop below threshold AND currentRPM
    //   < 15.
    //           Both conditions together confirm the motor stopped AND the drum
    //           stopped spinning. RPM data is still published even after this
    //           transition for post-processing.
    //   Dryer:  requires only smoothedRMS to drop below threshold.
    //           The IR sensor is a washer-only feature; dryer cycle-end is
    //           detected by vibration alone. The raw RPM value (0.0) continues
    //           to be logged in the CSV for completeness.
    // A 5-second debounce (DONE_DEBOUNCE_TIME) prevents spurious transitions
    // from short pauses in the spin cycle.
    if (smoothedRMS < RMS_THRESHOLD &&
        (machineType == "dryer" || currentRPM < 15.0)) {
      if (lowSince == 0)
        lowSince = millis();

      if (millis() - lowSince >= DONE_DEBOUNCE_TIME) {
        Serial.println("Cycle completed. Transition to DONE.");
        cycleEndTime = millis();
        publishCycleEnd();
        transitionTo(DONE);
        lowSince = 0;
      }
    } else {
      lowSince = 0; // reset debounce if signal recovers
    }
    break;

  case DONE:
    setLED(HIGH, LOW); // Red — laundry is ready to be picked up
    // Hold DONE state until the user acknowledges via button press or the
    // 30-minute auto-reset fires. An alert is published on auto-reset so the
    // backend can log the event for fleet uptime tracking.
    if (millis() - stateEnterTime >= 1800000) { // 30 mins
      publishAlert("auto_reset",
                   "DONE state expired after 30 min; returning to IDLE");
      transitionTo(IDLE);
    }
    break;
  }
}
