// accel.ino
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <algorithm>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

float currentRMS = 0.0;
float smoothedRMS = 0.0;
// EMA with α = 2/(N+1) where N=50. Using F-suffix to keep arithmetic in float.
// Time constant: τ = (1/α - 1) × 20ms ≈ 980 ms → transitions settle in ~5τ ≈ 5
// s. This directly drives the 5-second DONE_DEBOUNCE_TIME in the state machine.
const float EMA_ALPHA = 2.0F / 51.0F;

// Sliding window RMS variables (N=100 samples @ 50Hz = 2s window)
const int RMS_WINDOW_SIZE = 100;
float rmsSumSqBuffer[RMS_WINDOW_SIZE] = {0.0};
int rmsSumSqIndex = 0;
float rmsSumSqRunning = 0.0;
bool rmsBufferFull = false;

unsigned long lastAccelSample = 0;
const unsigned long ACCEL_SAMPLE_PERIOD = 20; // 20 ms for 50Hz

// Moving Median variables (N=50 sliding window on RMS values = 1s window)
const int MEDIAN_WINDOW_SIZE = 50;
float rmsHistory[MEDIAN_WINDOW_SIZE] = {0.0};
int rmsHistoryIndex = 0;
bool rmsHistoryFull = false;

void setupAccel() {
  if (!accel.begin()) {
    Serial.println("ADXL345 not found — check wiring");
    // while (1);
  }

  accel.setDataRate(ADXL345_DATARATE_50_HZ);
  accel.setRange(ADXL345_RANGE_16_G);
}

void updateAccel() {
  unsigned long now = millis();
  if (now - lastAccelSample >= ACCEL_SAMPLE_PERIOD) {
    lastAccelSample = now;

    sensors_event_t event;
    accel.getEvent(&event);

    float x = event.acceleration.x;
    float y = event.acceleration.y;
    float z = event.acceleration.z;

    // Compute the total acceleration magnitude across all three axes.
    // NOTE: Subtracting the gravity constant assumes the sensor is level!!
    // If the sensor is tilted, the static gravity vector shifts across axes,
    // which would introduce a constant offset. In a production environment,
    // a calibration phase should record the static gravity vector [gx, gy, gz]
    // and subtract it vectorially: AC = sqrt((x-gx)^2 + (y-gy)^2 + (z-gz)^2).
    float rawMag = sqrt((x * x) + (y * y) + (z * z));
    float acG = (rawMag - 9.80665F) /
                9.80665F; // convert to g, zero-centered around idle
    float magSq = acG * acG;

    // O(1) sliding RMS calculation:
    // Subtract the square exiting the window, add the new square.
    // This avoids re-summing the entire 100-element buffer every tick.
    rmsSumSqRunning -= rmsSumSqBuffer[rmsSumSqIndex]; // evict oldest sample
    rmsSumSqBuffer[rmsSumSqIndex] = magSq;            // insert newest sample
    rmsSumSqRunning += magSq;                         // update running sum

    // Safety Clamp: Floating-point arithmetic is subject to rounding errors.
    // Over long periods of continuous addition/subtraction, rmsSumSqRunning
    // can drift below 0.0, causing sqrt() to return NaN. We clamp it at 0.0 to prevent this.
    // Critical Optimization Note: A periodic re-summation (e.g., every 1000 samples)
    // could be implemented to reset any accumulated floating-point drift.
    if (rmsSumSqRunning < 0.0F) {
      rmsSumSqRunning = 0.0F;
    }

    rmsSumSqIndex++;
    if (rmsSumSqIndex >= RMS_WINDOW_SIZE) {
      rmsSumSqIndex = 0;
      rmsBufferFull = true; // flag stays true permanently after first full fill
    }

    int rmsCount = rmsBufferFull ? RMS_WINDOW_SIZE : rmsSumSqIndex;
    if (rmsCount > 0) {
      currentRMS = sqrt(rmsSumSqRunning / (float)rmsCount);
    } else {
      currentRMS = 0.0;
    }

    // Update sliding Median buffer
    rmsHistory[rmsHistoryIndex] = currentRMS;
    rmsHistoryIndex++;
    if (rmsHistoryIndex >= MEDIAN_WINDOW_SIZE) {
      rmsHistoryIndex = 0;
      rmsHistoryFull = true;
    }

    // Calculate moving median
    int medianCount = rmsHistoryFull ? MEDIAN_WINDOW_SIZE : rmsHistoryIndex;
    float medianRMS = currentRMS;
    if (medianCount > 0) {
      // Copy to scratch buffer before sorting (preserves the circular history
      // order). std::sort on N=50 floats ≈ 282 comparisons per 20 ms tick —
      // acceptable at 50 Hz. A min-max heap pair would reduce this to O(log N)
      // per insert if needed.
      float sortBuffer[MEDIAN_WINDOW_SIZE];
      for (int i = 0; i < medianCount; i++) {
        sortBuffer[i] = rmsHistory[i];
      }
      std::sort(sortBuffer, sortBuffer + medianCount);

      if (medianCount % 2 == 1) {
        medianRMS = sortBuffer[medianCount / 2];
      } else {
        medianRMS =
            (sortBuffer[(medianCount / 2) - 1] + sortBuffer[medianCount / 2]) /
            2.0F;
      }
    }

    // Apply EMA filter to the median RMS output.
    // α = 2/51 ≈ 0.039; τ ≈ 980 ms; transitions take ~5τ ≈ 5 s to fully settle.
    smoothedRMS = (EMA_ALPHA * medianRMS) + (1.0F - EMA_ALPHA) * smoothedRMS;

    // Periodically print progress for debugging
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
      lastPrint = millis();
      Serial.print("RMS: ");
      Serial.print(currentRMS);
      Serial.print(" | Median: ");
      Serial.print(medianRMS);
      Serial.print(" | Smoothed: ");
      Serial.println(smoothedRMS);
    }
  }
}