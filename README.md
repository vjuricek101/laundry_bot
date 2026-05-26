# Smart Laundry Notifier
Non-invasive IoT system to track shared laundry machines. Uses an ESP32, accelerometer, and temp sensor.

## How It Works
- **Vibration Sensing**: ADXL345 detects running states via EMA filtering.
- **Dryer Detection**: TMP36 heat sensor identifies dryers.
- **Smart Logic**: ESP32 filters false starts and pauses.
- **Anomaly Detection**: Flags short cycles (<3m), stuck machines (>3h), and nodes that appear to be offline.
- **Communication**: Publishes JSON payloads via MQTT over Wi-Fi.

## Structure
- `/firmware`: C++ ESP32 code. Handles sensors, state machine, deep sleep, and MQTT. Copy `config_template.h` to `config.h` for credentials.
- `/backend`: Python services. 
  - `logger.py`: Logs events from MQTT to CSV.
  - `notifier.py`: Sends notifications to Telegram. 
  - `manager.py`: Watchdog for offline, short-cycle, or stuck anomalies.
- `/dashboard`: HTML/CSS/JS frontend. Connects to MQTT via WebSockets for live status and alerts.

## Setup
1. **Hardware**: Wire ADXL345 (I2C) and TMP36 (Analog) to ESP32.
2. **Firmware**: Add Wi-Fi/MQTT details in `config.h` and flash.
3. **Backend**: `pip install -r backend/requirements.txt` then run scripts.
4. **Dashboard**: Open `dashboard/index.html`.
-----------
5. **Testing (Synthetic - Frontend only)**: Run `python backend/simulator.py --synthetic` and ensure `USE_SYNTHETIC = true` in `dashboard/config.js`
