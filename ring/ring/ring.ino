#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       // Pin connected to the WS2812B Data In
#define IR_PIN      2       // Pin connected to the IR Receiver
#define BUZZER_PIN  8       // Pin connected to the Active Buzzer
#define NUM_LEDS    45

CRGB leds[NUM_LEDS];

// Map PC numbers (1, 2, 3, 4) to their starting pixel on the strip.
// Adjusted to match the ~11 pixel gaps shown in your diagram.
const int pcNodes[4] = {0, 11, 22, 33}; 

// --- STATE VARIABLES ---
int sourcePC = 0; // Stores the first key press (0 means nothing pressed yet)

void setup() {
  Serial.begin(9600);
  
  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure it's off at startup
  
  // Initialize LEDs
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
  
  // Initialize IR Receiver
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  
  Serial.println("System Ready.");
  Serial.println("Enter Source PC (1-4) followed by Destination PC (1-4).");
}

void loop() {
  if (IrReceiver.decode()) {
    
    // Ignore automatic repeat signals sent by holding the button down
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }

    int command = IrReceiver.decodedIRData.command;
    
    // Print the received command to the serial monitor so you can map your remote
    Serial.print("IR Command Received: ");
    Serial.println(command);
    
    int pressedNumber = getNumberFromIR(command);

    if (pressedNumber >= 1 && pressedNumber <= 4) {
      if (sourcePC == 0) {
        // First button press: Set the Source
        sourcePC = pressedNumber;
        Serial.print("Source PC selected: "); 
        Serial.println(sourcePC);
        
        // Light up the source PC yellow to indicate it's waiting for a destination
        leds[pcNodes[sourcePC - 1]] = CRGB::Yellow;
        FastLED.show();
        
      } else {
        // Second button press: Set the Destination and animate
        int destPC = pressedNumber;
        Serial.print("Destination PC selected: "); 
        Serial.println(destPC);
        
        if (sourcePC != destPC) {
          simulateRing(sourcePC, destPC);
        } else {
          Serial.println("Error: Source and Destination cannot be the same.");
          
          // Error sound: long buzz
          digitalWrite(BUZZER_PIN, HIGH);
          delay(500);
          digitalWrite(BUZZER_PIN, LOW);
          
          FastLED.clear();
          FastLED.show();
        }
        
        // Reset for the next packet simulation
        sourcePC = 0; 
      }
      
      // Add a debounce delay to give you time to lift your finger 
      // before it accepts the next command
      delay(300); 
    }
    
    IrReceiver.resume(); // Ready for the next button press
  }
}

// --- NETWORK SIMULATION LOGIC ---
void simulateRing(int src, int dest) {
  // Convert PC numbers (1-4) to their actual array indexes (0-3)
  int startIdx = pcNodes[src - 1];
  int endIdx = pcNodes[dest - 1];
  
  int currentIdx = startIdx;
  FastLED.clear();
  
  // Loop until the packet reaches the destination node
  while (currentIdx != endIdx) {
    leds[currentIdx] = CRGB::Blue; // Packet color
    FastLED.show();
    
    delay(200); // Speed of the packet flowing
    
    leds[currentIdx] = CRGB::Black; // Turn off the trail behind it
    
    currentIdx++; 
    
    // RING LOGIC: If we reach the end of the 45 pixels, wrap back to 0
    if (currentIdx >= NUM_LEDS) {
      currentIdx = 0; 
    }
  }
  
  // Success! Blink the destination PC green
  leds[endIdx] = CRGB::Green;
  FastLED.show();
  
  // Success sound: Two quick beeps
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

  // Replace the digitalWrite success sound with this:
  // tone(BUZZER_PIN, 1000); // Play a 1000Hz tone
  // delay(100);
  // noTone(BUZZER_PIN);     // Stop the tone
  // delay(100);
  // tone(BUZZER_PIN, 1000);
  // delay(100);
  // noTone(BUZZER_PIN);
  
  delay(1200); // Remaining delay before clearing
  
  FastLED.clear();
  FastLED.show();
}

// --- REMOTE CONTROL MAPPING ---
int getNumberFromIR(int command) {
  switch(command) {
    case 69: return 1; 
    case 70: return 2; 
    case 71: return 3; 
    case 68: return 4; 
    default: return -1; // Unrecognized button
  }
}