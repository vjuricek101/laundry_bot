import paho.mqtt.client as mqtt
import json
import requests
import argparse
import os
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

# Subscribe to cycle_end and alert across all buildings/machines
TOPICS = [("laundry/+/+/cycle_end", 1), ("laundry/+/+/alert", 1)]

# Mock webhook URL for notifications
WEBHOOK_URL = "http://localhost:8080/webhook" 

def send_notification(message):
    print(f"NOTIFICATION DISPATCHED: {message}")
    # In production, dispatch to Twilio/Email/Webhook
    try:
        # requests.post(WEBHOOK_URL, json={"text": message})
        pass
    except Exception as e:
        print(f"Webhook failed: {e}")

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Notifier connected to {BROKER} with result code {reason_code}")
    client.subscribe(TOPICS)

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        topic_parts = msg.topic.split('/')
        
        building = topic_parts[1]
        machine_id = topic_parts[2]
        event = topic_parts[3]
        
        if event == "cycle_end":
            duration = data.get("duration_min", "?")
            m_type = data.get("machine_type", "machine")
            msg_text = f"Your {m_type} cycle in {building} ({machine_id}) is DONE! (Duration: {duration} min)"
            send_notification(msg_text)
            
        elif event == "alert":
            alert_type = data.get("alert_type", "unknown")
            detail = data.get("detail", "")
            msg_text = f"ALERT for {building}/{machine_id}: {alert_type} - {detail}"
            send_notification(msg_text)
            
    except Exception as e:
        print(f"Error processing notification: {e}")

if __name__ == "__main__":
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if USERNAME and PASSWORD:
        client.username_pw_set(USERNAME, PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    
    print("Starting notifier service...")
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("Stopping notifier")
        client.disconnect()
