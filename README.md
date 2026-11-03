# Sound Translator
**Making classroom sound visible through responsive light.**  

Sound Translator is an interactive classroom installation that converts changes in surrounding sound into colour, brightness, and movement. A **MAX4466 microphone** captures audio amplitude; an **Arduino MKR WiFi 1010** processes it and sends RGB frames over **Wi-Fi and MQTT** to **Luminaire 13**, a **12 × 6 (72 LED)** lighting grid.

## How It Works
```text
Classroom sound 
          ↓
MAX4466 microphone (analogue OUT → A0)
          ↓
Arduino MKR WiFi 1010
  • peak-to-peak amplitude (RAW)
  • background-noise calibration (FLOOR)
  • adaptive scaling + smoothing (LEVEL, 0–1)
          ↓
12 × 6 RGB frame (72 LEDs × 3 bytes = 216 bytes)
          ↓
Wi-Fi → MQTT broker → Luminaire 13
```

Important!!!!!!The system maps **sound amplitude**, not speech content, sound frequency, or calibrated decibels. It cannot distinguish typing from conversation or identify individual speakers.

## Lighting Behaviour 

| Sound activity| Lighting response |
|---|---|
| Quiet | Dim blue background|
| Low | First blue and cyan columns activate|
| Medium | More green and yellow columns appear|
| High | Orange and pink columns become visible|
| Sudden strong peak | Short pink-white expanding ripple |

The 12 columns have **fixed RGB palette colours**; the sound level controls how many columns illuminate and the brightness of the last partially lit column. It does **not** change every LED to a single colour. A sudden amplitude spike can temporarily overlay the sound meter with a ripple.

## Hardware

- Arduino MKR WiFi 1010
- MAX4466 electret microphone amplifier module
- Luminaire 13 / 12 × 6 RGB LED installation
- USB cable and a suitable Wi-Fi connection

### Wiring

| MAX4466 pin | Arduino MKR WiFi 1010 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| OUT | A0 |

## Project structure 

```text
RadioWaveFinal/
├── RadioWaveFinal.ino   # Main program 
├── arduino_secrets.h   # Local Wi-Fi + MQTT credentials 
└── README.md
```

### Local secrets file 
Create `arduino_secrets.h` alongside the `.ino` file:
```cpp
#ifndef ARDUINO_SECRETS_H
#define ARDUINO_SECRETS_H

#define SECRET_SSID "YOUR_WIFI_NAME"
#define SECRET_PASS "YOUR_WIFI_PASSWORD"
#define SECRET_MQTTUSER "YOUR_MQTT_USERNAME"
#define SECRET_MQTTPASS "YOUR_MQTT_PASSWORD"

#endif
```

## Setup 
1. Wire the MAX4466 to **3.3V**, **GND**, and **A0**. 
2. Install the required Arduino libraries.
3. Create `arduino_secrets.h` with your own Wi-Fi and MQTT credentials.
4. Select **Arduino MKR WiFi 1010** in Arduino IDE, then upload the sketch. 
5. Open **Serial Monitor at 115200 baud**. 
6. Keep the room relatively quiet for the first **3 seconds** to establish the background-noise baseline. 
7. Test silence, speaking, and a single clap while watching Luminaire 13. 

## Design Intention
The project turns an invisible part of the classroom environment into shared visual feedback. Its purpose is to help people notice collective sound changes.


## Limitations and Future Development | 局限与改进
- **Not a decibel meter ** Raw MAX4466 amplitudes are not calibrated sound-pressure levels.
- **No source recognition ** Sound amplitude alone cannot reliably distinguish talking, typing, and clapping.
- **Environment-dependent ** Microphone gain, distance, and background noise affect sensitivity.

