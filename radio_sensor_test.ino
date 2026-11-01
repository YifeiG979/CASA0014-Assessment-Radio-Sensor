const int micPin = A0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
}

void loop() {
  int minimum = 4095;
  int maximum = 0;

  unsigned long start = millis();

  while (millis() - start < 50) {
    int value = analogRead(micPin);

    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
  }

  int amplitude = maximum - minimum;

  Serial.print("Amplitude: ");
  Serial.println(amplitude);

  delay(50);
}
