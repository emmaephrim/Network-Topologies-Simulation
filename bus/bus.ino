#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       
#define IR_PIN      2       
#define BUZZER_PIN  8       
#define NUM_LEDS    83      

CRGB leds[NUM_LEDS];

// --- BUS TOPOLOGY MAPPING ---
// Converting your 1-indexed diagram to 0-indexed C++ arrays (Subtracting 1)

// 1. PC Nodes (The tips where the PCs sit: 3, 68, 69, 53)
const int pcNodes[4]    = {2, 67, 68, 52}; 

// 2. Branch Bases (The last pixel of the branch before it hits the bus: 16, 54, 83, 41)
const int branchBase[4] = {15, 53, 82, 40}; 

// 3. Bus Entry Points (The exact pixel ON THE BUS where the branch connects)
// PC1 connects directly at 17
// PC2 jumper wire connects at 34
// PC3 jumper wire connects at 20
// PC4 connects directly at 40
const int busPoint[4]   = {16, 33, 19, 39}; 

// --- STATE VARIABLES ---
int sourcePC = 0; 
int currentBrightness = 100;    
bool backgroundOn = false;      

// Colors matched to the Ring topology
CRGB bgColor = CRGB(15, 6, 0);     // Dim Amber/Copper Background
CRGB packetColor = CRGB::Blue;     // Classic Blue packet 

void setup() {
  Serial.begin(9600);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(currentBrightness); 
  updateBackground();
  
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  
  Serial.println("System Ready - Bus Topology (Continuous Flow Fixed)");
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
        
        sourcePC = 0; // Clear state before animating
        
        if (currentSource != destPC) {
          simulateBus(currentSource, destPC);
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

// --- BUS TOPOLOGY SIMULATION LOGIC ---
void simulateBus(int src, int dest) {
  updateBackground(); 
  
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  // PHASE 1: Source PC down to the base of its branch
  animateSegment(pcNodes[srcIdx], branchBase[srcIdx]);
  
  // Flash the exact point where it hits the Main Bus
  leds[busPoint[srcIdx]] = CRGB::White;
  FastLED.show();
  delay(80);
  leds[busPoint[srcIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  // PHASE 2: Travel continuously along the Main Bus (No skipping!)
  if (busPoint[srcIdx] != busPoint[destIdx]) {
    animateSegment(busPoint[srcIdx], busPoint[destIdx]);
  }
  
  // Flash the exact point where it exits the Main Bus
  leds[busPoint[destIdx]] = CRGB::White;
  FastLED.show();
  delay(80);
  leds[busPoint[destIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  // PHASE 3: Base of the Destination branch up to the PC
  animateSegment(branchBase[destIdx], pcNodes[destIdx]);
  
  // Destination reached
  leds[pcNodes[destIdx]] = CRGB::Green;
  FastLED.show();
  successBeep();
  
  delay(1200); 
  updateBackground(); 
}

// Moves the packet bi-directionally along any branch or backbone
void animateSegment(int startIdx, int endIdx) {
  int step = (startIdx < endIdx) ? 1 : -1;
  int currentIdx = startIdx;
  
  while (true) {
    leds[currentIdx] = packetColor; 
    FastLED.show();
    
    delay(100); // Speed of the packet
    
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