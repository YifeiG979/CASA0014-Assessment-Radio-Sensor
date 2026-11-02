#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"
#include <math.h>

// 1. Check Wifi Connection and MQTT Connection
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqttUser = SECRET_MQTTUSER;
const char* mqttPass = SECRET_MQTTPASS;
const char* mqttServer = "mqtt.cetools.org";
const int mqttPort = 1884;
const char* mqttTopic =
  "student/CASA0014/luminaire/13";
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
//2. Connect MAX4466 with WIFI 1010
const int MIC_PIN = A0;
float noiseFloor = 0;
float peakEnvelope = 25;
float soundLevel = 0;
float previousSound = 0;
bool calibrated = false;
unsigned long calibrationStart = 0;
float calibrationSum = 0;
int calibrationCount = 0;
const unsigned long CALIBRATION_TIME = 3000;

// 3. Minimum sound range to avoid
const float MIN_RANGE = 35.0;

// 4. Setting LED Light
const int WIDTH = 12;
const int HEIGHT = 6;
const int NUM_LEDS = 72;
byte RGBpayload[216];
const bool SERPENTINE = false;

// 5. Show How the animation of Lights
float waveRadius = 0;
float waveEnergy = 0;
unsigned long lastFrame = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
const unsigned long FRAME_INTERVAL = 60;

// 6. Connect WIFI
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (lastWiFiAttempt != 0 &&
      millis() - lastWiFiAttempt < 10000) {
    return;
  }
  lastWiFiAttempt = millis();
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);
}

// 7. Connect MQTT
void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (mqttClient.connected()) {
    return;
  }
  if (lastMQTTAttempt != 0 &&
      millis() - lastMQTTAttempt < 5000) {
    return;
  }
  lastMQTTAttempt = millis();
  String clientId = "Luminaire4-";
  clientId += String(millis());
  if (mqttClient.connect(
      clientId.c_str(),
      mqttUser,
      mqttPass)) {
    Serial.println("MQTT Connected!");
  } else {
    Serial.print("MQTT Error: ");
    Serial.println(mqttClient.state());
  }
}

// 8. Read data gain from radio sensor
int readAmplitude() {
  int signalMin = 1023;
  int signalMax = 0;
  unsigned long start = millis();
  while (millis() - start < 30) {
    int sample = analogRead(MIC_PIN);
    if (sample < signalMin) {
      signalMin = sample;
    }
    if (sample > signalMax) {
      signalMax = sample;
    }
  }
  return signalMax - signalMin;
}

// 9. Automatic Calibration
void calibrateMicrophone(int amplitude) {
  if (calibrated) return;
  calibrationSum += amplitude;
  calibrationCount++;
  if (millis() - calibrationStart >=
      CALIBRATION_TIME) {
    noiseFloor =
      calibrationSum / calibrationCount;
    peakEnvelope = noiseFloor + MIN_RANGE;
    calibrated = true;
    Serial.println("Calibration Finished!");
    Serial.print("Noise Floor: ");
    Serial.println(noiseFloor);
  }
}

// 10. Automatic Sound Sensitivity
void updateSound(int amplitude) {
  float floorAlpha =
    amplitude < noiseFloor + 10 ? 0.02 : 0.002;
  noiseFloor +=
    (amplitude - noiseFloor) * floorAlpha;
  float corrected =
    max(0.0f, amplitude - noiseFloor);
  if (corrected > peakEnvelope) {
    peakEnvelope = corrected;
  } else {
    peakEnvelope *= 0.997;
  }
  float range =
    max(MIN_RANGE, peakEnvelope * 1.15f);
  float normalized =
    corrected / range;
  normalized =
    constrain(normalized, 0.0f, 1.0f);
  float smoothing =
    normalized > soundLevel ? 0.45 : 0.12;
  soundLevel +=
    (normalized - soundLevel) * smoothing;
  if (soundLevel < 0.025) {
    soundLevel = 0;
  }
}

// 11. Setting LED lights
int pixelIndex(int x, int y) {
  if (SERPENTINE && y % 2 == 1) {
    x = WIDTH - 1 - x;
  }
  return y * WIDTH + x;
}
void setPixel(
  int x,
  int y,
  int r,
  int g,
  int b
) {
  int index = pixelIndex(x, y) * 3;
  RGBpayload[index] =
    constrain(r, 0, 255);
  RGBpayload[index + 1] =
    constrain(g, 0, 255);
  RGBpayload[index + 2] =
    constrain(b, 0, 255);
}

// 12. Setting Color Change
void getSoundColor(
  float level,
  int &r,
  int &g,
  int &b
) {

  const int colors[8][3] = {
    {0,  0,  35},    // Deep Blue
    {0,  50, 160},   // Blue
    {0, 160, 220},   // Cyan
    {0, 200, 140},   // Turquoise
    {90, 220, 50},   // Green
    {255, 180, 0},   // Yellow
    {255, 75, 0},    // Orange
    {255, 10, 70}    // Pink
  };
  float position =
    constrain(level, 0.0f, 1.0f) * 7.0f;
  int index = (int)position;
  int nextIndex = min(index + 1, 7);
  float mix = position - index;
  r = colors[index][0] +
      (colors[nextIndex][0] -
       colors[index][0]) * mix;
  g = colors[index][1] +
      (colors[nextIndex][1] -
       colors[index][1]) * mix;
  b = colors[index][2] +
      (colors[nextIndex][2] -
       colors[index][2]) * mix;
}

// 13. Sound React
void updateAnimation() {
  int amplitude = readAmplitude();
  if (!calibrated) {
    calibrateMicrophone(amplitude);
    // Blue during calibration
    soundLevel = 0;
  } else {
    updateSound(amplitude);
  }
  if (calibrated &&
      soundLevel > 0.40 &&
      previousSound <= 0.40 &&
      waveEnergy < 0.25) {
    waveRadius = 0;
    waveEnergy = 1.0;
  }
  previousSound = soundLevel;
  waveRadius += 0.38;
  waveEnergy *= 0.88;
  if (waveRadius > 8) {
    waveEnergy = 0;
  }
  int baseR, baseG, baseB;
  getSoundColor(
    soundLevel,
    baseR,
    baseG,
    baseB
  );
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      float dx = x - 5.5;
      float dy = y - 2.5;
      float distance =
        sqrt(dx * dx + dy * dy);
      float difference =
        fabs(distance - waveRadius);
      float wave =
        max(0.0f, 1.0f - difference / 1.3f);
      wave *= waveEnergy;
      int r = baseR + wave * 100;
      int g = baseG + wave * 100;
      int b = baseB + wave * 100;
      setPixel(
        x, y,
        constrain(r, 0, 255),
        constrain(g, 0, 255),
        constrain(b, 0, 255)
      );
    }
  }
  Serial.print("RAW: ");
  Serial.print(amplitude);
  Serial.print(" | Noise: ");
  Serial.print(noiseFloor, 1);
  Serial.print(" | Peak: ");
  Serial.print(peakEnvelope, 1);
  Serial.print(" | Sound: ");
  Serial.println(soundLevel, 3);
}

// 14. Send data to Luminaire
void sendLEDs() {
  if (!mqttClient.connected()) return;
  bool success = mqttClient.publish(
    mqttTopic,
    RGBpayload,
    sizeof(RGBpayload)
  );
  if (!success) {
    Serial.println("MQTT Publish Failed");
  }
}

// 15. Setting
void setup() {
  Serial.begin(115200);
  analogReadResolution(10);
  pinMode(MIC_PIN, INPUT);
  memset(RGBpayload, 0, sizeof(RGBpayload));
  mqttClient.setServer(
    mqttServer,
    mqttPort
  );
  mqttClient.setBufferSize(512);
  mqttClient.setSocketTimeout(2);
  calibrationStart = millis();
  Serial.println("======================");
  Serial.println("CASA0014 FINAL");
  Serial.println("MAX4466 Sound Reactive");
  Serial.println("Luminaire 4");
  Serial.println("Auto Calibration");
  Serial.println("======================");
  connectWiFi();
}
// 16. Loop
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  } else {
    if (!mqttClient.connected()) {
      connectMQTT();
    }
    if (mqttClient.connected()) {
      mqttClient.loop();
    }
  }
  if (millis() - lastFrame >= FRAME_INTERVAL) {
    lastFrame = millis();
    updateAnimation();
    sendLEDs();
  }
}
