#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       
#define IR_PIN      2       
#define BUZZER_PIN  8       
#define NUM_LEDS    61      

CRGB leds[NUM_LEDS];

// --- STAR TOPOLOGY MAPPING ---
// PC1 (1-16):   Outer=1,  Inner=16  -> indices 0  and 15
// PC2 (17-31):  Outer=31, Inner=17  -> indices 30 and 16
// PC3 (32-46):  Outer=32, Inner=46  -> indices 31 and 45  <-- FIXED DIRECTION
// PC4 (47-61):  Outer=61, Inner=47  -> indices 60 and 46
const int pcNodes[4]  = {0, 30, 31, 60}; 
const int hubNodes[4] = {15, 16, 45, 46}; 

// --- STATE VARIABLES ---
int sourcePC = 0; 
int currentBrightness = 100;    
bool backgroundOn = false;      

CRGB bgColor = CRGB(15, 6, 0);     // Dim Amber/Copper Background
CRGB packetColor = CRGB::White;    // Pure White packet 

void setup() {
  Serial.begin(9600);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(currentBrightness); 
  updateBackground();
  
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  
  Serial.println("System Ready - Star Topology");
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
        int currentSource = sourcePC; 
        
        sourcePC = 0; 
        
        if (currentSource != destPC) {
          simulateStar(currentSource, destPC);
        } else {
          Serial.println("Error: Source and Destination cannot be the same.");
          errorBeep();
          updateBackground(); 
        }
      }
      delay(300); 
    }
    
    // Handle Brightness UP (5)
    else if (action == 5) {
      currentBrightness += 25;
      if (currentBrightness > 255) currentBrightness = 255;
      FastLED.setBrightness(currentBrightness);
      FastLED.show();
      delay(150);
    }
    
    // Handle Brightness DOWN (6)
    else if (action == 6) {
      currentBrightness -= 25;
      if (currentBrightness < 10) currentBrightness = 10;
      FastLED.setBrightness(currentBrightness);
      FastLED.show();
      delay(150);
    }
    
    // Handle Background Toggle (7)
    else if (action == 7) {
      backgroundOn = !backgroundOn; 
      updateBackground();
      delay(300);
    }
    
    IrReceiver.resume(); 
  }
}

// --- STAR TOPOLOGY SIMULATION LOGIC ---
void simulateStar(int src, int dest) {
  updateBackground(); 
  
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  // PHASE 1: Source PC -> Router
  animateSegment(pcNodes[srcIdx], hubNodes[srcIdx]);
  
  // ROUTER PROCESSING EFFECT: Flash the center hub pixels white
  for(int i = 0; i < 4; i++) {
    leds[hubNodes[i]] = CRGB::White;
  }
  FastLED.show();
  delay(150); 
  
  // Restore center pixels to background before moving on
  for(int i = 0; i < 4; i++) {
    leds[hubNodes[i]] = backgroundOn ? bgColor : CRGB::Black;
  }
  FastLED.show();
  
  // PHASE 2: Router -> Destination PC
  animateSegment(hubNodes[destIdx], pcNodes[destIdx]);
  
  // Destination reached
  leds[pcNodes[destIdx]] = CRGB::Green;
  FastLED.show();
  successBeep();
  
  delay(1200); 
  updateBackground(); 
}

// Moves the packet bi-directionally along any branch
void animateSegment(int startIdx, int endIdx) {
  int step = (startIdx < endIdx) ? 1 : -1;
  int currentIdx = startIdx;
  
  while (true) {
    leds[currentIdx] = packetColor; 
    FastLED.show();
    
    delay(100); 
    
    if (backgroundOn) {
      leds[currentIdx] = bgColor;
    } else {
      leds[currentIdx] = CRGB::Black;
    }
    
    if (currentIdx == endIdx) break;
    currentIdx += step; 
  }
}

// --- HELPER FUNCTIONS ---
void updateBackground() {
  if (backgroundOn) {
    fill_solid(leds, NUM_LEDS, bgColor); 
  } else {
    FastLED.clear(); 
  }
  
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
    case 69: return 1; 
    case 70: return 2; 
    case 71: return 3; 
    case 68: return 4; 
    case 24: return 5; 
    case 82: return 6; 
    case 25: return 7; 
    default: return -1; 
  }
}