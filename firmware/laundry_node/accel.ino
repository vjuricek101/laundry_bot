// accel.ino
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

float currentRMS = 0.0;
float smoothedRMS = 0.0;
const float EMA_ALPHA = 2.0 / 51.0;

void setupAccel() {
  if (!accel.begin()) {
    Serial.println("ADXL345 not found — check wiring");
    // while (1);
  }

  accel.setDataRate(ADXL345_DATARATE_50_HZ);
  accel.setRange(ADXL345_RANGE_16_G);
}

void updateAccel() {
  float sumSq = 0.0;

  for (int i = 0; i < 100; i++) {
    sensors_event_t event;
    accel.getEvent(&event);

    float x = event.acceleration.x;
    float y = event.acceleration.y;
    float z = event.acceleration.z;

    sumSq += (x * x) + (y * y) + (z * z);

    delay(20);
  }

  currentRMS = sqrt(sumSq / 100.0);

  // smooth it out so it’s not jumping around
  smoothedRMS = (EMA_ALPHA * currentRMS) + (1.0 - EMA_ALPHA) * smoothedRMS;

  Serial.println(smoothedRMS);
}