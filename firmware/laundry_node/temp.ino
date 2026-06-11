// temp.ino
// Reads the TMP36 analog temperature sensor every 30 seconds.
// Temperature is published as a continuous telemetry metric in the MQTT state
// payload. The machine type (washer vs. dryer) is set at compile time via
// MACHINE_TYPE in config.h, NOT inferred from this reading.

float currentTempC = 0.0;

unsigned long lastTempReadTime = 0;
const unsigned long TEMP_READ_INTERVAL = 30000; // 30 seconds

void setupTemp() {
  // Set ADC attenuation for full 0-3.3V range
  analogSetPinAttenuation(TMP36_PIN, ADC_11db);
}

void updateTemp() {
  if (millis() - lastTempReadTime >= TEMP_READ_INTERVAL) {
    lastTempReadTime = millis();

    // Read analog voltage in millivolts using ESP32's built-in calibrated API.
    // Note: ESP32's internal ADCs are notoriously non-linear, especially near 0V and 3.3V.
    // Using analogReadMilliVolts() leverages factory calibration lookup tables (Vref calibration),
    // which is significantly more accurate than a simple map() or scaling of analogRead().
    uint32_t milliVolts = analogReadMilliVolts(TMP36_PIN);

    // TMP36 conversion formula:
    // The sensor outputs 10 mV per °C, with a 500 mV offset at 0°C to allow negative temperatures.
    // formula: Temp_C = (Voltage_in_V - 0.5V) * 100
    // We divide milliVolts by 1000.0 to convert to Volts, subtract 0.5V, then scale by 100.0.
    currentTempC = ((milliVolts / 1000.0F) - 0.5F) * 100.0F;

    Serial.print("Temp C: ");
    Serial.println(currentTempC);
    // Temperature is reported as-is. For a dryer, this reflects cabinet heat
    // conducted through the metal panel.
    // For a washer, it reflects ambient
    // (no heat applied).
    // Threshold-based classification removed; reporting all values as is
  }
}
