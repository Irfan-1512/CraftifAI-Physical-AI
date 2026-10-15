# CraftifAI — Gesture-Controlled Physical AI

## Overview
CraftifAI is a gesture-controlled device prototype that uses a laptop webcam to recognize hand gestures and send commands to a virtual actuator over MQTT.

## Features
- Webcam-based gesture perception using OpenCV and NumPy
- MQTT communication using Paho MQTT
- Virtual ESP32 receiver for simulated hardware actions
- ESP-IDF v5.5 firmware prepared for future hardware integration

## Gesture Commands

| Gesture | Simulated Action |
|---|---|
| OPEN_PALM | Relay ON and LED ON |
| FIST | Relay OFF and LED OFF |
| POINT_LEFT | Servo position 0 degrees |
| POINT_RIGHT | Servo position 180 degrees |

## Architecture

Webcam -> Gesture Detection -> MQTT Broker -> Virtual Actuator

## Requirements
- Python 3.12
- OpenCV
- NumPy
- Paho MQTT
- Internet connection for the MQTT broker

## Setup

Activate your Python virtual environment and install dependencies:

```powershell
C:\Users\SHAIK\venv\Scripts\Activate.ps1
python -m pip install opencv-python numpy paho-mqtt
```

Open two PowerShell terminals in the project directory.

Terminal 1:
```powershell
python virtual_actuator.py
```

Terminal 2:
```powershell
python gesture_pipeline.py
```

Show the four gestures to the webcam and observe the actuator terminal.

## Hardware Status
The perception and MQTT communication pipeline has been tested with a virtual actuator. Physical ESP32, relay, LED, and servo operation has not yet been tested. ESP-IDF firmware is included for future hardware integration.

## Technologies
Python, OpenCV, NumPy, MQTT, HiveMQ, ESP-IDF, C
