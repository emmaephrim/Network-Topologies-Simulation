#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       
#define IR_PIN      2       
#define BUZZER_PIN  8       
#define NUM_LEDS    45

CRGB leds[NUM_LEDS];

// Map PC numbers (1, 2, 3, 4) to their starting pixel on the strip.
const int pcNodes[4] = {0, 11, 22, 33}; 

// --- STATE VARIABLES ---
int sourcePC = 0; 
int currentBrightness = 100;    // Starting brightness (10 to 255)
bool backgroundOn = false;      // Tracks if the background is currently active

// Updated to a deep "Cyber Purple" - adjust the (Red, Green, Blue) values to your liking!
CRGB bgColor = CRGB(8, 0, 15); 
CRGB packetColor = CRGB::Blue;

void setup() {
  Serial.begin(9600);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(currentBrightness); // Apply initial brightness
  updateBackground();
  
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  
  Serial.println("System Ready.");
}

void loop() {
  if (IrReceiver.decode()) {
    
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }

    int command = IrReceiver.decodedIRData.command;
    Serial.print("IR Command Received: ");
    Serial.println(command);
    
    int action = getActionFromIR(command);

    // Handle PC Selection (1-4)
    if (action >= 1 && action <= 4) {
      if (sourcePC == 0) {
        sourcePC = action;
        leds[pcNodes[sourcePC - 1]] = CRGB::Yellow;
        FastLED.show();
      } else {
        int destPC = action;
        int currentSource = sourcePC; // Save the source temporarily
        
        // --- THE FIX ---
        // Clear the state immediately before animating so it doesn't redraw at the end
        sourcePC = 0; 
        
        if (currentSource != destPC) {
          simulateRing(currentSource, destPC);
        } else {
          Serial.println("Error: Source and Destination cannot be the same.");
          errorBeep();
          updateBackground(); // Reset the display
        }
      }
      delay(300); // Debounce
    }
    
    // Handle Brightness UP (5)
    else if (action == 5) {
      currentBrightness += 25;
      if (currentBrightness > 255) currentBrightness = 255;
      FastLED.setBrightness(currentBrightness);
      FastLED.show();
      Serial.print("Brightness: "); Serial.println(currentBrightness);
      delay(150);
    }
    
    // Handle Brightness DOWN (6)
    else if (action == 6) {
      currentBrightness -= 25;
      if (currentBrightness < 10) currentBrightness = 10;
      FastLED.setBrightness(currentBrightness);
      FastLED.show();
      Serial.print("Brightness: "); Serial.println(currentBrightness);
      delay(150);
    }
    
    // Handle Background Toggle (7)
    else if (action == 7) {
      backgroundOn = !backgroundOn; // Flip the state
      updateBackground();
      Serial.print("Background turned "); 
      Serial.println(backgroundOn ? "ON" : "OFF");
      delay(300);
    }
    
    IrReceiver.resume(); 
  }
}

// --- NETWORK SIMULATION LOGIC ---
void simulateRing(int src, int dest) {
  int startIdx = pcNodes[src - 1];
  int endIdx = pcNodes[dest - 1];
  int currentIdx = startIdx;
  
  updateBackground(); // Ensure clean slate before animation
  
  while (currentIdx != endIdx) {
    leds[currentIdx] = packetColor; 
    FastLED.show();
    
    delay(150); // Slightly faster animation looks better with a background
    
    // Revert the pixel back to the background state instead of turning it black
    if (backgroundOn) {
      leds[currentIdx] = bgColor;
    } else {
      leds[currentIdx] = CRGB::Black;
    }
    
    currentIdx++; 
    if (currentIdx >= NUM_LEDS) {
      currentIdx = 0; 
    }
  }
  
  // Destination reached
  leds[endIdx] = CRGB::Green;
  FastLED.show();
  successBeep();
  
  delay(1200); 
  updateBackground(); // Clear the green success node
}

// --- HELPER FUNCTIONS ---
void updateBackground() {
  if (backgroundOn) {
    fill_solid(leds, NUM_LEDS, bgColor); 
  } else {
    FastLED.clear(); 
  }
  
  // If a source PC is currently selected, keep it lit yellow
  if (sourcePC != 0) {
    leds[pcNodes[sourcePC - 1]] = CRGB::Yellow;
  }
  
  FastLED.show();
}

void successBeep() {
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void errorBeep() {
  digitalWrite(BUZZER_PIN, HIGH); delay(500);
  digitalWrite(BUZZER_PIN, LOW);
}

// --- REMOTE CONTROL MAPPING ---
int getActionFromIR(int command) {
  switch(command) {
    // PCs
    case 69: return 1; 
    case 70: return 2; 
    case 71: return 3; 
    case 68: return 4; 
    
    // Brightness and Background Controls
    case 24: return 5; 
    case 82: return 6; 
    case 25: return 7; 
    
    default: return -1; 
  }
}