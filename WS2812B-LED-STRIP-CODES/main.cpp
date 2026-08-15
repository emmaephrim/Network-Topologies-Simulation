#include <Adafruit_NeoPixel.h>

#define PIN 6
#define NUMPIXELS 188
Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.setBrightness(50); // Keep at 50 to avoid overloading the Arduino
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  // 1. Color Wipes (fills the strip one LED at a time)
  colorWipe(strip.Color(255,   0,   0), 10); // Red
  colorWipe(strip.Color(  0, 255,   0), 10); // Green
  colorWipe(strip.Color(  0,   0, 255), 10); // Blue

  // 2. Theater Chase (crawling lights)
  theaterChase(strip.Color(127, 127, 127), 30); // White
  theaterChase(strip.Color(127,   0,   0), 30); // Red
  theaterChase(strip.Color(  0,   0, 127), 30); // Blue

  // 3. Flowing Rainbows
  rainbow(10);
  theaterChaseRainbow(30);
}

// --- ANIMATION HELPER FUNCTIONS ---

// Fill strip pixels one after another with a color
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

// Theater-marquee-style chasing lights
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times
    for(int b=0; b<3; b++) { // 'b' counts from 0 to 2
      strip.clear();         // Turn all LEDs off
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Turn every 3rd pixel on
      }
      strip.show();
      delay(wait);
    }
  }
}

// Rainbow cycle along whole strip
void rainbow(int wait) {
  // Hue of first pixel runs 3 complete loops through the color wheel.
  for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(wait);
  }
}

// Rainbow-enhanced theater chase
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;
  for(int a=0; a<30; a++) {
    for(int b=0; b<3; b++) {
      strip.clear();
      for(int c=b; c<strip.numPixels(); c += 3) {
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color);
      }
      strip.show();
      delay(wait);
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}
