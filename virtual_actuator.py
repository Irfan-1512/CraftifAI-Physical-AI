import json
import paho.mqtt.client as mqtt

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "gesture/actuate"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("=" * 50)
        print("[VIRTUAL ESP32 NODE] Connected to MQTT Broker!")
        print("[VIRTUAL ESP32 NODE] Listening for gesture actions...")
        print("=" * 50)
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        gesture = payload.get("gesture", "NONE")
        
        print("\n--------------------------------------------------")
        print(f"[HARDWARE RECEIVER] Received Command: {gesture}")
        
        if gesture == "OPEN_PALM":
            print(" >> [ACTION] RELAY: HIGH (ON)  | LED: ON")
        elif gesture == "FIST":
            print(" >> [ACTION] RELAY: LOW (OFF)  | LED: OFF")
        elif gesture == "POINT_LEFT":
            print(" >> [ACTION] SERVO: Rotated to 0 DEGREES")
        elif gesture == "POINT_RIGHT":
            print(" >> [ACTION] SERVO: Rotated to 180 DEGREES")
        print("--------------------------------------------------")
    except Exception as e:
        print(f"Error parsing payload: {e}")

client = mqtt.Client(client_id="Virtual_ESP32_Receiver")
client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()