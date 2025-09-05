# 🚨 ESP32 Wireless Panic Audio Streaming & Emergency Detection System  

## 📌 Overview  
This project is a **wireless IoT panic system** built with an **ESP32 microcontroller**.  
It captures **live audio** and **sensor data**, streams it over Wi-Fi to a **Node.js server**, and displays everything on a **real-time web dashboard**.  

Key features:  
- 🎙️ **Audio capture** using INMP441 microphone.  
- 🧠 **Speech-to-Text transcription** using [Vosk STT](https://alphacephei.com/vosk/).  
- 🚨 **Emergency keyword detection** (e.g., “help”, “fire”) → instant **Telegram alerts**.  
- 🌡️ **Sensor logging** (temperature, humidity, mic activity) into CSV logs.  
- 📊 **Web dashboard** for waveform, transcript, sensor data, and history.  
- 🔋 **Battery-powered portability** using rechargeable cells and a 7805 regulator.  

---

## 🔌 Hardware Setup  

- **ESP32 DevKit V1** – Wi-Fi microcontroller (core of the system).  
- **INMP441 Microphone** – I²S digital mic for real-time audio capture.  
- **DHT Sensor** – measures temperature & humidity.  
- **OLED Display (SSD1306, I²C)** – shows system status (Wi-Fi, streaming, alerts).  
- **7805 Voltage Regulator** – steps down ~7.4 V to stable 5 V for ESP32.  
- **Battery Pack** – 2× 7.4 V rechargeable lithium batteries in series.  

➡️ Entire system is **wireless** and can run standalone in field conditions.  

---

## 🛠️ Software Components  

- **ESP32 Firmware** (`project_streaming_02.ino`)  
  - Captures audio via INMP441 (I²S).  
  - Reads temperature & humidity from DHT.  
  - Sends data via **WebSocket** to Node.js server.  
  - Updates OLED with **system status** (connected/streaming/standby).  

- **Node.js Server** (`server.js`)  
  - Handles WebSocket connections.  
  - Stores audio recordings (`.wav`) and sensor logs (`.csv`).  
  - Relays live data to browser dashboards.  
  - Forwards audio to Python STT engine.  
  - Sends **Telegram alerts** on emergency keyword detection.  

- **Speech Recognition** (`realtime_stt.py`)  
  - Uses **Vosk model** (offline).  
  - Produces real-time transcripts from incoming audio.  

- **Web Dashboard** (`index.html`, `processor.js`)  
  - Displays **live audio waveform**.  
  - Shows **real-time transcript**.  
  - Alerts with **red flashing banner** on keyword detection.  
  - Plots **live temperature, humidity, mic peak** with Chart.js.  
  - Provides **download & replay** for past sessions.  

---

## 🔄 Program Flow  

1. **System Power On**  
   - ESP32 boots from 2×7.4 V battery pack via 7805 regulator.  
   - Connects to Wi-Fi and initializes mic, DHT, and OLED.  

2. **WebSocket Connection**  
   - ESP32 connects to Node.js server (`server.js`).  
   - OLED shows “Connected” status.  

3. **Session Start**  
   - ESP32 sends `start` → server creates new `.wav` and `.csv` files.  
   - OLED updates to show “Streaming…” mode.  

4. **Data Streaming**  
   - PCM audio chunks sent over WebSocket.  
   - Sensor values `{temp, hum, mic_peak}` sent periodically.  
   - Server stores:  
     - Audio → `audio/panic_timestamp.wav`  
     - Sensors → `data/sensor_timestamp.csv`  

5. **Speech-to-Text**  
   - Node server pipes audio to `realtime_stt.py`.  
   - Vosk generates transcripts.  
   - Browser dashboard updates **Live Transcript**.  

6. **Emergency Detection**  
   - If transcript contains **“help”** or **“fire”**:  
     - 🚨 Telegram bot sends instant alert with timestamp.  
     - 🚨 Dashboard shows **red flashing emergency banner**.  

7. **Session End**  
   - ESP32 sends `stop`.  
   - Server closes `.wav` and `.csv` files.  
   - Dashboard updates historical recordings/logs list.  

---

## 📂 Project Structure  
├── project_streaming_02.ino # ESP32 firmware\
├── server.js # Node.js server (WebSocket + Express)\
├── realtime_stt.py # Vosk STT engine\
├── index.html # Web dashboard\
├── processor.js # AudioWorklet for PCM playback\
├── package.json # Node dependencies\
├── audio/ # Saved audio recordings\
├── data/ # Saved sensor logs\
└── vosk-model-small-en-us/ # Vosk model files\

**🔮 Future Improvements**

🎛️ Add DSP filters for noise reduction & echo cancellation.

🌍 Support multi-language STT (switch Vosk models).

☁️ Push logs/audio to cloud storage (AWS S3 / Firebase).

📱 Mobile app for remote monitoring.

🤖 On-device keyword spotting (Edge AI on ESP32).
