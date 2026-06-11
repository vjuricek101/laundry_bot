// rpm.ino - Ground-Truth Drum Rotation Tracking via IR Break-Beam Sensor
#include <Arduino.h>

// GPIO 27 is hardware interrupt-capable and kept free from output signals
const int BEAM_PIN = 27;

// Volatile variables used within the ISR to prevent compiler optimization
// errors
volatile unsigned long lastBeamTime = 0;
volatile unsigned long beamInterval = 0;
volatile bool newBeamEvent = false;
float currentRPM = 0.0;

// Interrupt Service Routine (ISR) triggered on a RISING edge (flag enters the
// beam) Marked IRAM_ATTR to ensure fast interrupt execution directly from RAM.
// Interrupt Service Routine (ISR) triggered on a RISING edge (flag enters the
// beam) Marked IRAM_ATTR to ensure fast interrupt execution directly from RAM.
void IRAM_ATTR beamISR() {
  unsigned long now = micros();
  unsigned long interval = now - lastBeamTime;

  // Software debounce: discard pulses closer than 20 ms apart.
  // This rejects electrical noise and bounds the measurable max to 3000 RPM.
  // IRAM_ATTR ensures this ISR runs from fast SRAM, not slower flash.
  if (interval > 20000) {
    beamInterval = interval;   // microseconds between consecutive drum flag passes
    lastBeamTime = now;
    newBeamEvent = true;       // signal main loop; no math done inside ISR to keep ISR execution time minimal
  }
}

// Configures GPIO pin and registers the hardware interrupt
void setupRPM() {
  // Configures the internal pull-up resistor to pull the open-collector signal
  // HIGH cleanly, preventing floating inputs
  pinMode(BEAM_PIN, INPUT_PULLUP);

  // Attaches the ISR to execute instantly on the RISING edge of a flag pulse
  attachInterrupt(digitalPinToInterrupt(BEAM_PIN), beamISR, RISING);
  Serial.println("IR Break-Beam Sensor initialized on GPIO 27.");
}

// Periodic update handler called in the main execution loop
void updateRPM() {
  // Critical Section Optimization:
  // Atomically snapshot ISR-shared volatile variables before clearing the flag.
  // We disable interrupts briefly so that the ISR doesn't fire between checking
  // newBeamEvent and reading beamInterval, which could lead to data corruption
  // or a mismatch between the flag status and the interval value.
  noInterrupts();
  bool gotEvent = newBeamEvent;
  unsigned long interval = beamInterval;
  newBeamEvent = false;
  interrupts();

  if (gotEvent) {
    if (interval > 0) {
      // RPM = (60 sec/min * 1,000,000 µs/sec) / interval_in_µs
      // Optimization Suggestion: Measuring RPM from a single interval is highly sensitive to jitter.
      // Implementing a moving average of the last N intervals would stabilize the reading.
      currentRPM = (60.0F * 1000000.0F) / (float)interval;
    }
  } else {
    // Timeout check: if no pulses are detected for 3 seconds, assume the drum has stopped.
    // This is necessary because the ISR only triggers on motion; a stationary drum
    // produces no interrupts, so the RPM would stay stuck at the last calculated value.
    if (micros() - lastBeamTime > 3000000) {
      currentRPM = 0.0F;
    }
  }
}
