import paho.mqtt.client as mqtt
import json
import time
import random
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

# Synthetic machines
machines = [
    {"building": "roble", "id": "w01", "type": "washer", "state": "idle", "timer": 0},
    {"building": "roble", "id": "d02", "type": "dryer", "state": "idle", "timer": 0}
]

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Simulator connected to {BROKER}")

def publish_state(client, m):
    topic = f"laundry/{m['building']}/{m['id']}/state"
    payload_dict = {"state": m['state'], "machine_type": m['type']}
    
    if m.get('metrics') and m['state'] == 'running':
        payload_dict['health_metrics'] = m['metrics']
        
    payload = json.dumps(payload_dict)
    client.publish(topic, payload, retain=True)
    print(f"Published: {topic} -> {payload}")

def run_simulation():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if USERNAME and PASSWORD:
        client.username_pw_set(USERNAME, PASSWORD)
    client.on_connect = on_connect
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
    except Exception as e:
        print(f"Failed to connect: {e}")
        return

    print("Starting simulation...")
    
    try:
        while True:
            for m in machines:
                if m['state'] == "idle":
                    if random.random() < 0.3: # 30% chance to start
                        m['state'] = "running"
                        m['timer'] = random.randint(3, 8) # Run for 3-8 loops
                        publish_state(client, m)
                        
                elif m['state'] == "running":
                    m['timer'] -= 1
                    
                    # Generate live metrics while running
                    if m['type'] == 'washer':
                        # Normally 50-100 vibration. 10% chance of broken drum (<30)
                        if random.random() < 0.1:
                            m['metrics'] = {"vibration": random.randint(10, 29)}
                        else:
                            m['metrics'] = {"vibration": random.randint(50, 95)}
                    elif m['type'] == 'dryer':
                        # Normally 50C-70C. 10% chance of broken heating element (<35)
                        if random.random() < 0.1:
                            m['metrics'] = {"temperature": random.randint(20, 34)}
                        else:
                            m['metrics'] = {"temperature": random.randint(50, 70)}
                            
                    publish_state(client, m) # Publish updated metrics every tick
                    
                    if m['timer'] <= 0:
                        m['state'] = "done"
                        m.pop('metrics', None)
                        publish_state(client, m)
                        
                        # Publish cycle end
                        topic = f"laundry/{m['building']}/{m['id']}/cycle_end"
                        payload = json.dumps({
                            "duration_min": random.randint(30, 45), 
                            "machine_type": m['type']
                        })
                        client.publish(topic, payload)
                        print(f"Published Cycle End: {topic}")
                        
                        # 10% chance to simulate a short cycle anomaly (for manager.py testing)
                        if random.random() < 0.1:
                            anomaly_topic = f"laundry/{m['building']}/{m['id']}/cycle_end"
                            anomaly_payload = json.dumps({
                                "duration_min": 1, # <3 triggers anomaly
                                "machine_type": m['type']
                            })
                            client.publish(anomaly_topic, anomaly_payload)
                            print(f"*** Generated Anomaly: {anomaly_topic}")

                elif m['state'] == "done":
                    if random.random() < 0.5: # 50% chance someone picks up laundry
                        m['state'] = "idle"
                        publish_state(client, m)
                        
            time.sleep(5) # Fast-forward time (5 seconds per tick)
            
    except KeyboardInterrupt:
        print("Simulation stopped.")
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    run_simulation()
