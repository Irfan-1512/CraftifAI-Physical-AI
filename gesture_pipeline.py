import time
import json
import cv2
import numpy as np
import paho.mqtt.client as mqtt

# MQTT Broker Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "gesture/actuate"
CAMERA_INDEX = 0  # 0 for integrated webcam, 1 for external USB camera

mqtt_client = mqtt.Client(client_id="PipeGen_Vision_Node")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[VISION NODE] Connected to MQTT Broker successfully!")
    else:
        print(f"[VISION NODE] Connection failed with code {rc}")

mqtt_client.on_connect = on_connect
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()

def detect_gesture(frame):
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    lower_skin = np.array([0, 20, 70], dtype=np.uint8)
    upper_skin = np.array([20, 255, 255], dtype=np.uint8)
    mask = cv2.inRange(hsv, lower_skin, upper_skin)
    
    contours, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    
    if contours:
        largest = max(contours, key=cv2.contourArea)
        area = cv2.contourArea(largest)
        
        if area > 10000:
            M = cv2.moments(largest)
            if M["m00"] != 0:
                cx = int(M["m10"] / M["m00"])
                width = frame.shape[1]
                if cx < width / 3:
                    return "POINT_LEFT"
                elif cx > (2 * width) / 3:
                    return "POINT_RIGHT"
                else:
                    return "OPEN_PALM"
        elif area > 3000:
            return "FIST"
            
    return "NONE"

def main():
    cap = cv2.VideoCapture(CAMERA_INDEX)
    if not cap.isOpened():
        print("[ERROR] Could not access webcam.")
        return

    print("Perception node active. Press 'q' to quit.")
    last_gesture = ""
    last_sent_time = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        gesture = detect_gesture(frame)
        current_time = time.time()

        if gesture != "NONE" and (gesture != last_gesture or (current_time - last_sent_time) > 2.0):
            payload = {
                "device_id": "vision_cam_01",
                "timestamp": int(current_time),
                "gesture": gesture
            }
            mqtt_client.publish(MQTT_TOPIC, json.dumps(payload))
            print(f"[SENT MQTT] Topic: {MQTT_TOPIC} -> Gesture: {gesture}")
            last_gesture = gesture
            last_sent_time = current_time

        cv2.putText(frame, f"Detected: {gesture}", (30, 50), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.imshow("Vision Perception Node", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()
    mqtt_client.loop_stop()

if __name__ == "__main__":
    main()