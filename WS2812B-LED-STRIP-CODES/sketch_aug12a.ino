// /*
//   WS2812B + IR Remote: Segmented Control (1-10, 11-20, etc.)
//   Features non-blocking timing and an integrated IR code reader.
// */

// #include <Adafruit_NeoPixel.h>
// #include <IRremote.h>

// // --- Configuration ---
// #define LED_PIN        6
// #define NUM_LEDS       30  // Set to 30 to handle three segments of 10
// #define IR_RECEIVE_PIN 11

// Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// // --- State Variables (The "Stopwatch" logic) ---
// unsigned long previousMillis = 0; 
// long interval = 1000;         
// int currentMode = 0;          
// int currentStep = 0;          

// void setup() {
//   Serial.begin(115200);
//   IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  
//   strip.begin();
//   strip.setBrightness(50);
//   strip.show(); 
  
//   Serial.println(F("==========================================="));
//   Serial.println(F("System Ready! Press a button on the remote."));
//   Serial.println(F("==========================================="));
// }

// void loop() {
//   // 1. Check for new IR commands
//   if (IrReceiver.decode()) {
//     unsigned long irCode = IrReceiver.decodedIRData.command;
    
//     // --- CRUCIAL STEP: THIS PRINTS YOUR REMOTE'S SECRET CODE ---
//     Serial.print(F("--> You pressed a button! The Hex Code is: 0x"));
//     Serial.println(irCode, HEX);
//     Serial.println(F("--> Copy that code and replace a 0xFFXXXX below!"));
//     Serial.println(F("-------------------------------------------"));
    
//     // --- REPLACE THE 0xFFXXXX CODES BELOW WITH YOUR REAL CODES ---
//     switch(irCode) {
      
//       case 0x45: // REPLACE WITH YOUR CODE FOR BUTTON '1'
//         setMode(1, 1000); // Mode 1, 1-second delay
//         Serial.println(F("Activating Mode 1: LEDs 1-10 Sequential (Red)"));
//         break;
        
//       case 0x46: // REPLACE WITH YOUR CODE FOR BUTTON '2'
//         setMode(2, 1000); // Mode 2, 1-second delay
//         Serial.println(F("Activating Mode 2: LEDs 11-20 Sequential (Green)"));
//         break;

//       case 0x47: // REPLACE WITH YOUR CODE FOR BUTTON '3'
//         setMode(3, 1000); // Mode 3, 1-second delay
//         Serial.println(F("Activating Mode 3: LEDs 21-30 Sequential (Blue)"));
//         break;

//       case 0x19: // REPLACE WITH YOUR CODE FOR BUTTON '0' (Power Off)
//         setMode(0, 0); 
//         Serial.println(F("Turning all LEDs Off"));
//         break;
//     }
//     IrReceiver.resume(); // Ready for next button press
//   }

//   // 2. Update the LEDs based on the active mode
//   unsigned long currentMillis = millis();
//   if (currentMillis - previousMillis >= interval) {
//     previousMillis = currentMillis; 
//     updateLEDs();                   
//   }
// }

// // Helper function to cleanly switch modes
// void setMode(int newMode, long speed) {
//   currentMode = newMode;
//   interval = speed;
//   currentStep = 0;   // Reset the animation step back to the start
//   strip.clear();     // Clear the strip before starting new mode
//   strip.show();
// }

// // The logic for lighting up the specific segments
// void updateLEDs() {
//   switch (currentMode) {
//     case 0: // OFF
//       // Do nothing, strip is already cleared
//       break;

//     case 1: // Button 1: Sequential LEDs 1-10 (Index 0 to 9)
//       if (currentStep < 10) {
//         strip.setPixelColor(currentStep, strip.Color(255, 0, 0)); // Red
//         strip.show();
//         currentStep++;
//       }
//       break;

//     case 2: // Button 2: Sequential LEDs 11-20 (Index 10 to 19)
//       if (currentStep < 10) {
//         strip.setPixelColor(10 + currentStep, strip.Color(0, 255, 0)); // Green
//         strip.show();
//         currentStep++;
//       }
//       break;

//     case 3: // Button 3: Sequential LEDs 21-30 (Index 20 to 29)
//       if (currentStep < 10) {
//         strip.setPixelColor(20 + currentStep, strip.Color(0, 0, 255)); // Blue
//         strip.show();
//         currentStep++;
//       }
//       break;
//   }
// }

















/*
  WS2812B + IR Remote: Advanced Control
  Features: Segmented sequential lights, animations, and brightness control.
*/

#include <Adafruit_NeoPixel.h>
#include <IRremote.h>

// --- Configuration ---
#define LED_PIN        6
#define NUM_LEDS       30  // Set to 30 to handle three segments of 10
#define IR_RECEIVE_PIN 11

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- State Variables (The "Stopwatch" logic) ---
unsigned long previousMillis = 0; 
long interval = 1000;         
int currentMode = 0;          
int currentStep = 0;  
bool bounceDirection = true; // Used for the Cylon bounce effect
int currentBrightness = 50;  // Default brightness

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  
  strip.begin();
  strip.setBrightness(currentBrightness);
  strip.show(); 
  
  Serial.println(F("==========================================="));
  Serial.println(F("Advanced System Ready!"));
  Serial.println(F("==========================================="));
}

void loop() {
  // 1. Check for new IR commands
  if (IrReceiver.decode()) {
    unsigned long irCode = IrReceiver.decodedIRData.command;
    
    Serial.print(F("--> IR Code Received: 0x"));
    Serial.println(irCode, HEX);
    
    // --- IR CODE MAPPING ---
    switch(irCode) {
      
      case 0x45: // Button '1' (Confirmed)
        setMode(1, 1000); 
        Serial.println(F("Mode 1: LEDs 1-10 Sequential (Red)"));
        break;
        
      case 0x46: // Button '2' (Confirmed)
        setMode(2, 1000); 
        Serial.println(F("Mode 2: LEDs 11-20 Sequential (Green)"));
        break;

      case 0x47: // Button '3' (Confirmed)
        setMode(3, 1000); 
        Serial.println(F("Mode 3: LEDs 21-30 Sequential (Blue)"));
        break;

      case 0x44: // Button '4' (Unconfirmed guess)
        setMode(4, 100); 
        Serial.println(F("Mode 4: Theater Chase"));
        break;

      case 0x40: // Button '5' (Unconfirmed guess)
        setMode(5, 20); 
        Serial.println(F("Mode 5: Rainbow Flow"));
        break;

      case 0x43: // Button '6' (Unconfirmed guess)
        setMode(6, 40); 
        Serial.println(F("Mode 6: Cylon Bounce"));
        break;

      case 0x18: // UP Arrow (Unconfirmed guess) - Brightness UP
        adjustBrightness(25);
        break;

      case 0x52: // DOWN Arrow (Unconfirmed guess) - Brightness DOWN
        adjustBrightness(-25);
        break;

      case 0x19: // Button '0' (Confirmed) - Power Off
        setMode(0, 0); 
        Serial.println(F("Turning all LEDs Off"));
        break;
    }
    IrReceiver.resume(); // Ready for next button press
  }

  // 2. Update the LEDs based on the active mode (Non-blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; 
    updateLEDs();                   
  }
}

// Helper function to cleanly switch modes
void setMode(int newMode, long speed) {
  if (currentMode != newMode) {
    currentMode = newMode;
    interval = speed;
    currentStep = 0;             // Reset animation step
    bounceDirection = true;      // Reset direction for bounce effects
    strip.clear();               // Clear strip for new mode
    strip.show();
  }
}

// Helper function to adjust brightness on the fly
void adjustBrightness(int change) {
  currentBrightness += change;
  // Clamp brightness between 5 (min) and 255 (max)
  if (currentBrightness > 255) currentBrightness = 255;
  if (currentBrightness < 5) currentBrightness = 5;
  
  strip.setBrightness(currentBrightness);
  strip.show(); // Apply immediately
  Serial.print(F("Brightness set to: "));
  Serial.println(currentBrightness);
}

// The logic for all lighting effects
void updateLEDs() {
  switch (currentMode) {
    case 0: // OFF
      break;

    case 1: // Button 1: Sequential 1-10
      if (currentStep < 10) {
        strip.setPixelColor(currentStep, strip.Color(255, 0, 0));
        strip.show();
        currentStep++;
      }
      break;

    case 2: // Button 2: Sequential 11-20
      if (currentStep < 10) {
        strip.setPixelColor(10 + currentStep, strip.Color(0, 255, 0));
        strip.show();
        currentStep++;
      }
      break;

    case 3: // Button 3: Sequential 21-30
      if (currentStep < 10) {
        strip.setPixelColor(20 + currentStep, strip.Color(0, 0, 255));
        strip.show();
        currentStep++;
      }
      break;

    case 4: // Button 4: Theater Chase
      strip.clear();
      for (int i = 0; i < NUM_LEDS; i = i + 3) {
        // Draw every 3rd pixel, offset by currentStep
        if (i + (currentStep % 3) < NUM_LEDS) {
          strip.setPixelColor(i + (currentStep % 3), strip.Color(255, 255, 255)); // White chase
        }
      }
      strip.show();
      currentStep++;
      break;

    case 5: // Button 5: Rainbow Flow
      // The library handles the heavy lifting of calculating rainbow colors
      strip.rainbow(currentStep * 256); 
      strip.show();
      currentStep++;
      // currentStep will naturally overflow and loop, which is fine for this effect
      break;

    case 6: // Button 6: Cylon/Scanner Bounce
      strip.clear();
      strip.setPixelColor(currentStep, strip.Color(255, 0, 0)); // Bright Red center
      
      // Add slight glowing tail
      if(currentStep > 0) strip.setPixelColor(currentStep - 1, strip.Color(50, 0, 0)); 
      if(currentStep < NUM_LEDS - 1) strip.setPixelColor(currentStep + 1, strip.Color(50, 0, 0));
      
      strip.show();

      // Handle the bouncing logic
      if (bounceDirection) {
        currentStep++;
        if (currentStep >= NUM_LEDS - 1) bounceDirection = false; // Turn around
      } else {
        currentStep--;
        if (currentStep <= 0) bounceDirection = true; // Turn around
      }
      break;
  }
}