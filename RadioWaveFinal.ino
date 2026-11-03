#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"
#include <math.h>

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqttUser = SECRET_MQTTUSER;
const char* mqttPass = SECRET_MQTTPASS;
const char* mqttServer = "mqtt.cetools.org";
const int mqttPort = 1884;
const char* mqttTopic = "student/CASA0014/luminaire/13";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const int MIC_PIN = A0;
const int WIDTH = 12;
const int HEIGHT = 6;
const int NUM_LEDS = WIDTH * HEIGHT;
byte rgbPayload[NUM_LEDS * 3];
const bool SERPENTINE = false;

// The influence of amplifying the volume on color changes
const float SOUND_GAIN = 2.2f;       
const float MIN_DYNAMIC_RANGE = 18; // Sets the minimum normalisation range
const float CLAP_THRESHOLD = 0.72f;  // Sets the threshold for sudden-sound effects
const unsigned long FRAME_MS = 55; // Updates lighting approximately 18 times per second
const unsigned long CALIBRATION_MS = 3000; // Calibrates background noise for 3 seconds
const unsigned long FLASH_MS = 520; // Sets the duration of the light pulse
const unsigned long CLAP_COOLDOWN_MS = 700; // Prevents repeated triggering too quickly

float noiseFloor = 0;
float calibrationSum = 0;
unsigned int calibrationSamples = 0;
unsigned long calibrationStart = 0;
bool calibrated = false;
float dynamicPeak = MIN_DYNAMIC_RANGE;
float level = 0;
float lastInstantLevel = 0;
unsigned long flashStart = 0;
unsigned long lastClap = 0;
bool flashActive = false;
unsigned long lastFrame = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

int ledIndex(int x, int y) {
  if (SERPENTINE && y % 2 == 1) x = WIDTH - 1 - x;
  return (y * WIDTH + x) * 3;
}

void setPixel(int x, int y, int r, int g, int b) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  int k = ledIndex(x, y);
  rgbPayload[k] = (byte)constrain(r, 0, 255);
  rgbPayload[k + 1] = (byte)constrain(g, 0, 255);
  rgbPayload[k + 2] = (byte)constrain(b, 0, 255);
}

int readAmplitude() {
  int low = 1023;
  int high = 0;
  unsigned long start = millis();
  while (millis() - start < 25) {
    int v = analogRead(MIC_PIN);
    if (v < low) low = v;
    if (v > high) high = v;
  }
  return high - low;
}

void readAndProcessSound() {
  int raw = readAmplitude();
  if (!calibrated) {
    if (calibrationStart == 0) calibrationStart = millis();
    calibrationSum += raw;
    calibrationSamples++;
    if (millis() - calibrationStart >= CALIBRATION_MS) {
      noiseFloor = calibrationSum / max(1U, calibrationSamples);
      dynamicPeak = MIN_DYNAMIC_RANGE;
      calibrated = true;
      Serial.print("Calibration finished. Floor=");
      Serial.println(noiseFloor, 1);
    }
    level = 0;
    return;
  }
  
  // First 3 seconds, distinguish meaningful sound activity from the room's normal ambient noise
  float floorRate = (raw < noiseFloor + 5) ? 0.015f : 0.0004f;
  noiseFloor += (raw - noiseFloor) * floorRate;
  float activity = max(0.0f, raw - noiseFloor);

  if (activity > dynamicPeak) dynamicPeak = activity;
  else dynamicPeak = max(MIN_DYNAMIC_RANGE, dynamicPeak * 0.992f);

  // Using Smoothing makes the light responding quickly 
  float instant = constrain(activity * SOUND_GAIN / max(MIN_DYNAMIC_RANGE, dynamicPeak), 0.0f, 1.0f);
  // Sudden Sound Detection
  bool sudden = instant >= CLAP_THRESHOLD &&
                (instant - lastInstantLevel > 0.22f) &&
                (lastClap == 0 || millis() - lastClap > CLAP_COOLDOWN_MS);
  if (sudden) {
    flashStart = millis();
    lastClap = millis();
    flashActive = true;
  }
  lastInstantLevel = instant;

  float smoothRate = instant > level ? 0.42f : 0.10f;
  level += (instant - level) * smoothRate;
  if (level < 0.035f) level = 0;
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 220) {
    lastPrint = millis();
    Serial.print("RAW="); Serial.print(raw);
    Serial.print(" FLOOR="); Serial.print(noiseFloor, 1);
    Serial.print(" PEAK="); Serial.print(dynamicPeak, 1);
    Serial.print(" LEVEL="); Serial.println(level, 2);
  }
}

// 12-column sound meter with a distinctly different colour in each section.
void drawLights() {
  const byte palette[12][3] = {
    {0, 45, 210}, {0, 95, 240}, {0, 190, 235}, {0, 220, 190},
    {0, 230, 105}, {90, 235, 25}, {175, 220, 0}, {235, 205, 0},
    {255, 150, 0}, {255, 78, 0}, {255, 30, 75}, {255, 0, 155}
  };

  // Mapping Sound to LED Columns
  float activeColumns = level * WIDTH;
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      float fraction = constrain(activeColumns - x, 0.0f, 1.0f);
      if (fraction > 0.01f) {
        int r = (int)(palette[x][0] * fraction);
        int g = (int)(palette[x][1] * fraction);
        int b = (int)(palette[x][2] * fraction);
        setPixel(x, y, r, g, b);
      } else {
        setPixel(x, y, 0, 0, 12); 
      }
    }
  }

  // Expanding Light Pulse
  if (flashActive) {
    unsigned long elapsed = millis() - flashStart;
    if (elapsed >= FLASH_MS) {
      flashActive = false;
    } else {
      float progress = (float)elapsed / FLASH_MS;
      float radius = progress * 7.0f;
      float intensity = 1.0f - progress;
      for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
          float d = sqrtf((x - 5.5f)*(x - 5.5f) + (y - 2.5f)*(y - 2.5f));
          float ring = constrain(1.0f - fabsf(d - radius) / 1.0f, 0.0f, 1.0f);
          float brightness = ring * intensity;
          if (brightness > 0.01f) {
            setPixel(x, y, (int)(255 * brightness),
                            (int)(180 * brightness), (int)(255 * brightness));
          }
        }
      }
    }
  }
}

void maintainConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (lastWiFiAttempt == 0 || millis() - lastWiFiAttempt >= 10000) {
      lastWiFiAttempt = millis();
      Serial.println("Connecting WiFi...");
      WiFi.begin(ssid, password);
    }
    return;
  }
  if (!mqttClient.connected()) {
    if (lastMQTTAttempt == 0 || millis() - lastMQTTAttempt >= 5000) {
      lastMQTTAttempt = millis();
      String id = String("SoundTranslator13-") + String(millis());
      if (mqttClient.connect(id.c_str(), mqttUser, mqttPass))
        Serial.println("MQTT connected - Luminaire 13");
      else {
        Serial.print("MQTT state: ");
        Serial.println(mqttClient.state());
      }
    }
  } else {
    mqttClient.loop();
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(10);
  pinMode(MIC_PIN, INPUT);
  memset(rgbPayload, 0, sizeof(rgbPayload));
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setBufferSize(512);
  mqttClient.setSocketTimeout(2);
  Serial.println("Sound Translator - Luminaire 13");
  Serial.println("Keep the room quiet for the first 3 seconds.");
  maintainConnection();
}

void loop() {
  maintainConnection();
  if (millis() - lastFrame >= FRAME_MS) {
    lastFrame = millis();
    readAndProcessSound();
    drawLights();
    if (mqttClient.connected() &&
        !mqttClient.publish(mqttTopic, rgbPayload, sizeof(rgbPayload))) {
      Serial.println("MQTT publish failed");
    }
  }
}

