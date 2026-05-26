// temp.ino

float currentTempC = 0.0;
bool isDryer = false;
String machineType = "washer";

unsigned long lastTempReadTime = 0;
const unsigned long TEMP_READ_INTERVAL = 30000; // 30 seconds

void setupTemp() {
  // Set ADC attenuation for full range (0-3.3V)
  analogSetPinAttenuation(TMP36_PIN, ADC_11db); // Using 11db for wider voltage range
}

void updateTemp() {
  if (millis() - lastTempReadTime >= TEMP_READ_INTERVAL) {
    lastTempReadTime = millis();
    
    uint32_t milliVolts = analogReadMilliVolts(TMP_PIN);
    
    // TMP36 conversion: 10 mV/degree C, 500 mV offset at 0 degrees C
    currentTempC = ((milliVolts / 1000.0) - 0.5) * 100.0;
    
    Serial.print("Temp C: ");
    Serial.println(currentTempC);
    
    if (currentTempC > 40.0) {
      isDryer = true;
      machineType = "dryer";
    } else {
      isDryer = false;
      machineType = "washer";
    }
  }
}
