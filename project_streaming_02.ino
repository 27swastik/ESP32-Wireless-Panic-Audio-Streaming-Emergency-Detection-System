#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Wi-Fi credentials
const char* ssid = "Wifi Name";
const char* password = "Password";
const char* websocket_server = "ip addres";
const uint16_t websocket_port = 8000;

// Hardware pins
#define BUTTON_PANIC     13
#define BUTTON_CANCEL    12
#define LED_PANIC        27
#define LED_NORMAL       26
#define BUZZER_PIN       25

#define DHT_PIN          4
#define DHT_TYPE         DHT11

#define I2S_WS           15
#define I2S_SD           32
#define I2S_SCK          14

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DHT dht(DHT_PIN, DHT_TYPE);
WebSocketsClient webSocket;

// I2S config
#define SAMPLE_RATE     16000
#define I2S_BUFFER_SIZE 1024
volatile int16_t audioBuffer[I2S_BUFFER_SIZE];

volatile bool panicFlag = false;
volatile bool cancelFlag = false;
bool streaming = false;
unsigned long lastSensorSend = 0;
unsigned long lastInterruptTime = 0;
#define DEBOUNCE_MS 200

void IRAM_ATTR handlePanic() { panicFlag = true; }
void IRAM_ATTR handleCancel() { cancelFlag = true; }

void setup() {
  Serial.begin(115200);
  Serial.println(" ESP32 Panic System Booting");

  pinMode(BUTTON_PANIC, INPUT_PULLUP);
  pinMode(BUTTON_CANCEL, INPUT_PULLUP);
  pinMode(LED_PANIC, OUTPUT);
  pinMode(LED_NORMAL, OUTPUT);
  digitalWrite(LED_NORMAL, HIGH);

  attachInterrupt(BUTTON_PANIC, handlePanic, FALLING);
  attachInterrupt(BUTTON_CANCEL, handleCancel, FALLING);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(" OLED init failed!");
    while (1);
  }
  updateDisplay("Booting...");
  dht.begin();

  WiFi.begin(ssid, password);
  updateDisplay("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  updateDisplay("WiFi Connected");

  webSocket.begin(websocket_server, websocket_port, "/");
  webSocket.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
      case WStype_CONNECTED:
        Serial.println(" WebSocket Connected");
        updateDisplay("Server Connected");
        break;
      case WStype_DISCONNECTED:
        Serial.println(" WebSocket Disconnected");
        updateDisplay("Server Down");
        break;
    }
  });
}

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 10,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void sendStatus(const char* status) {
  if (webSocket.isConnected()) {
    String msg = "{\"type\":\"status\",\"value\":\"" + String(status) + "\"}";
    webSocket.sendTXT(msg);
  }
}

void startStreaming() {
  if (streaming) return;

  setupI2S();
  streaming = true;
  digitalWrite(LED_PANIC, HIGH);
  digitalWrite(LED_NORMAL, LOW);
  updateDisplay("Streaming Active");

  String payload = "{\"type\":\"start\",\"timestamp\":" + String(millis()) + "}";
  webSocket.sendTXT(payload);
  sendStatus("streaming");

  Serial.println(" Streaming Started");
}

void stopStreaming() {
  if (!streaming) return;

  String payload = "{\"type\":\"stop\",\"timestamp\":" + String(millis()) + "}";
  webSocket.sendTXT(payload);
  sendStatus("standby");

  i2s_stop(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);
  streaming = false;
  digitalWrite(LED_PANIC, LOW);
  digitalWrite(LED_NORMAL, HIGH);
  updateDisplay("Standby");

  Serial.println(" Streaming Stopped");
}

void sendSensorData(size_t bytesRead) {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println(" DHT Read Failed!");
    return;
  }

  int16_t micPeak = 0;
  if (bytesRead > 0) {
    for (int i = 0; i < I2S_BUFFER_SIZE; i++) {
      if (abs(audioBuffer[i]) > micPeak) micPeak = abs(audioBuffer[i]);
    }
  }

  String payload = String("{\"type\":\"sensor\"") +
                   ",\"timestamp\":" + String(millis()) +
                   ",\"temp\":" + String(temp, 1) +
                   ",\"hum\":" + String(hum, 1) +
                   ",\"mic_peak\":" + String(micPeak) +
                   "}";
  webSocket.sendTXT(payload);
}

void loop() {
  webSocket.loop();

  if (panicFlag && (millis() - lastInterruptTime > DEBOUNCE_MS)) {
    panicFlag = false;
    lastInterruptTime = millis();
    startStreaming();
  }

  if (cancelFlag && (millis() - lastInterruptTime > DEBOUNCE_MS)) {
    cancelFlag = false;
    lastInterruptTime = millis();
    stopStreaming();
  }

  static unsigned long lastWSAttempt = 0;
  if (WiFi.status() == WL_CONNECTED && !webSocket.isConnected()) {
    if (millis() - lastWSAttempt > 5000) {
      Serial.println(" Reconnecting WebSocket...");
      webSocket.begin(websocket_server, websocket_port, "/");
      lastWSAttempt = millis();
    }
  }

  if (streaming) {
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, (void*)audioBuffer, sizeof(audioBuffer), &bytesRead, 0);

    if (bytesRead > 0 && webSocket.isConnected()) {

      //  GAIN BOOSTING 
      float gain = 2.0;  // base gain (try 1.5 to 2.0)
      int16_t maxVal = 0;

      for (int i = 0; i < I2S_BUFFER_SIZE; i++) {
        // Optional: apply base gain
        audioBuffer[i] = constrain(audioBuffer[i] * gain, -32768, 32767);
        int absVal = abs(audioBuffer[i]);
        if (absVal > maxVal) maxVal = absVal;

      }

      

      // Send improved audio to server
      webSocket.sendBIN((uint8_t*)audioBuffer, bytesRead);
    }


    if (millis() - lastSensorSend >= 2000) {
      sendSensorData(bytesRead);
      lastSensorSend = millis();
    }
  }
}

void updateDisplay(const String& msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Emergency System");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 15);
  display.println(msg);
  display.display();
}
