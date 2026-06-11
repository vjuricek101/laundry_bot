import paho.mqtt.client as mqtt
import json
import time
import threading
import argparse
import os
from dotenv import load_dotenv

parser = argparse.ArgumentParser()
parser.add_argument("--hivemq", action="store_true", help="Connect to public HiveMQ broker (broker.hivemq.com:1883) instead of local broker")
args = parser.parse_args()

if args.hivemq:
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
TOPIC_STATE = "laundry/+/+/state"
TOPIC_END = "laundry/+/+/cycle_end"

# Dictionary to keep track of machine states and last seen timestamps
machines = {}

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Manager connected to {BROKER}")
    client.subscribe([(TOPIC_STATE, 1), (TOPIC_END, 1)])

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        topic_parts = msg.topic.split('/')
        
        building = topic_parts[1]
        machine_id = topic_parts[2]
        event = topic_parts[3]
        
        key = f"{building}/{machine_id}"
        if key not in machines:
            machines[key] = {"state": "unknown", "last_seen": time.time(), "running_since": None}
            
        machines[key]["last_seen"] = time.time()
        
        if event == "state":
            new_state = data.get("state", "unknown")
            old_state = machines[key].get("state", "unknown")
            machines[key]["state"] = new_state
            # Track when this machine first entered the 'running' state
            if new_state == "running" and old_state != "running":
                machines[key]["running_since"] = time.time()
            elif new_state != "running":
                machines[key]["running_since"] = None
            
        elif event == "cycle_end":
            duration = data.get("duration_min", 0)
            if duration < 3:
                # Short cycle anomaly
                alert_payload = json.dumps({
                    "alert_type": "short_cycle",
                    "machine_id": machine_id,
                    "timestamp": time.time(),
                    "detail": f"Suspiciously short cycle: {duration} min"
                })
                client.publish(f"laundry/{building}/{machine_id}/alert", alert_payload)
        
    except Exception as e:
        print(f"Manager error: {e}")

def monitor_loop(client):
    while True:
        now = time.time()
        for key, info in machines.items():
            building, machine_id = key.split('/')
            
            # Dead node detection (>24h without any message)
            if now - info["last_seen"] > 86400:
                alert_payload = json.dumps({"alert_type": "offline", "machine_id": machine_id, "timestamp": now, "detail": "Node inactive for >24 hours"})
                client.publish(f"laundry/{building}/{machine_id}/alert", alert_payload)
                
            # Stuck-state detection: machine has been in 'running' for >3 hours.
            # NOTE: We check running_since (set at state entry), NOT last_seen.
            # last_seen is refreshed by every 5-second heartbeat, so
            # `now - last_seen` can never exceed ~10s for an active node.
            running_since = info.get("running_since")
            if info["state"] == "running" and running_since and (now - running_since > 10800):
                alert_payload = json.dumps({"alert_type": "stuck", "machine_id": machine_id, "timestamp": now, "detail": "Machine running for >3 hours"})
                client.publish(f"laundry/{building}/{machine_id}/alert", alert_payload)
        
        time.sleep(60)

if __name__ == "__main__":
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if USERNAME and PASSWORD:
        client.username_pw_set(USERNAME, PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    
    # Start background monitor thread
    monitor_thread = threading.Thread(target=monitor_loop, args=(client,), daemon=True)
    monitor_thread.start()
    
    print("Starting management service...")
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("Stopping manager")
        client.disconnect()
