#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define PIN_LED     21
#define PIN_LED_PWR 38  // Stamp-S3A: LED power is multiplexed with the
                          // reserved LCD backlight signal (net "DISP_BL").
                          // Must be driven HIGH or the WS2812 never lights,
                          // regardless of the data signal on PIN_LED.
#define NUM_LEDS    1

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Hello, World! from M5StampS3A");
  Serial.println("Rainbow pulse running on RGB LED...");

  pinMode(PIN_LED_PWR, OUTPUT);
  digitalWrite(PIN_LED_PWR, HIGH);  // enable BL_3V3 rail feeding the RGB LED
  delay(10);

  strip.begin();
  strip.setBrightness(120);  // gentle, easy-on-the-eyes brightness
}

void loop() {
  static uint16_t hue = 0;

  strip.setPixelColor(0, strip.gamma32(strip.ColorHSV(hue)));
  strip.show();

  hue += 256;  // ColorHSV takes a 16-bit hue (0-65535); full cycle in ~10s
  delay(40);

  static unsigned long lastLog = 0;
  if (millis() - lastLog >= 1000) {
    lastLog = millis();
    Serial.printf("Hello, World! (hue=%u)\n", hue);
  }
}
