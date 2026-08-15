#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       
#define IR_PIN      2       
#define BUZZER_PIN  8       
#define NUM_LEDS    83      // Updated to 83 to match your highest pixel (69-83)

CRGB leds[NUM_LEDS];

// --- BUS TOPOLOGY MAPPING ---
// Converting your 1-indexed diagram to 0-indexed C++ arrays

// PC Nodes (Where the yellow/green indicators appear)
// PC1 (3), PC2 (68), PC3 (69), PC4 (53)
const int pcNodes[4]  = {2, 67, 68, 52}; 

// Junctions (Where the branches meet the central Hub)
// PC1 (16), PC2 (54), PC3 (83), PC4 (41)
const int pcJunctions[4] = {15, 53, 82, 40}; 

// Which side of the hub is each PC on? (0 = Left, 1 = Right)
// PC1 and PC3 are Left. PC2 and PC4 are Right.
const int pcSide[4] = {0, 1, 0, 1}; 

// The Central Hub (Backbone) indices
const int hubLeft = 16;  // Diagram 17
const int hubRight = 39; // Diagram 40

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
  
  Serial.println("System Ready - Bus Topology");
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
  
  // PHASE 1: Source PC up to its Junction
  animateSegment(pcNodes[srcIdx], pcJunctions[srcIdx]);
  
  // PHASE 2: Traverse the Backbone (if crossing sides)
  if (pcSide[srcIdx] != pcSide[destIdx]) {
    if (pcSide[srcIdx] == 0) {
      // Traveling Left to Right
      animateSegment(hubLeft, hubRight);
    } else {
      // Traveling Right to Left
      animateSegment(hubRight, hubLeft);
    }
  }
  
  // PHASE 3: Junction down to the Destination PC
  animateSegment(pcJunctions[destIdx], pcNodes[destIdx]);
  
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