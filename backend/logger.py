import paho.mqtt.client as mqtt
import json
import csv
import time
import os
from datetime import datetime
import argparse
from dotenv import load_dotenv

parser = argparse.ArgumentParser()
parser.add_argument("--synthetic", action="store_true", help="Use public HiveMQ for testing without VPN")
args = parser.parse_args()

if args.synthetic:
    BROKER = "broker.hivemq.com"
    PORT = 1883
    USERNAME = None
    PASSWORD = None
else:
    load_dotenv()
    BROKER = os.getenv("MQTT_BROKER_IP", "10.136.92.10")
    PORT = int(os.getenv("MQTT_PORT", 1883))
    USERNAME = os.getenv("MQTT_USERNAME")
    PASSWORD = os.getenv("MQTT_PASSWORD")
TOPIC = "laundry/#"

CSV_FILE = "laundry_log.csv"

def init_csv():
    if not os.path.exists(CSV_FILE):
        with open(CSV_FILE, mode='w', newline='') as file:
            writer = csv.writer(file)
            # CSV Header
            writer.writerow(["timestamp_iso", "building", "machine_id", "machine_type", "event", "duration_min", "detail"])

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected to {BROKER} with result code {reason_code}")
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        topic_parts = msg.topic.split('/')
        
        # Expected topic format: laundry/{building}/{machine}/{event_type}
        if len(topic_parts) >= 4:
            building = topic_parts[1]
            machine_id = topic_parts[2]
            event = topic_parts[3]
            
            machine_type = data.get("machine_type", "")
            duration = data.get("duration_min", "")
            detail = data.get("detail", "")
            
            # Log to CSV
            with open(CSV_FILE, mode='a', newline='') as file:
                writer = csv.writer(file)
                writer.writerow([datetime.now().isoformat(), building, machine_id, machine_type, event, duration, detail])
            
            print(f"Logged event: {event} for {building}/{machine_id}")
            
    except json.JSONDecodeError:
        # Ignore non-JSON messages from other people on the public broker
        pass
    except Exception as e:
        # Only print other actual errors
        pass

def run_logger():
    init_csv()
    
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if USERNAME and PASSWORD:
        client.username_pw_set(USERNAME, PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    
    print(f"Starting logger. Connecting to {BROKER}...")
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("Stopping logger")
        client.disconnect()

if __name__ == "__main__":
    run_logger()
