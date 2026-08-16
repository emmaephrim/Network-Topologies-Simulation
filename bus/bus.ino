#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       
#define IR_PIN      2       
#define BUZZER_PIN  8       
#define NUM_LEDS    83      

CRGB leds[NUM_LEDS];

// --- BUS TOPOLOGY MAPPING ---
// Converting your 1-indexed diagram to 0-indexed C++ arrays

// PC Nodes (Where the yellow/green indicators appear)
// PC1 (3), PC2 (68), PC3 (69), PC4 (53)
const int pcNodes[4]  = {2, 67, 68, 52}; 

// Junctions (The physical end of each branch before it jumps to the bus)
// PC1 (16), PC2 (54), PC3 (83), PC4 (41)
const int pcJunctions[4] = {15, 53, 82, 40}; 

// T-Connectors (The exact pixels on the main bus where data transfers)
// PC1 & PC3 meet at Pixel 20 (Index 19). PC2 & PC4 meet at Pixel 34 (Index 33).
const int tConnectors[4] = {19, 33, 19, 33}; 

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
  
  Serial.println("System Ready - Bus Topology (T-Connector Update)");
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
  
  // PHASE 1: Source PC down to its branch Junction
  animateSegment(pcNodes[srcIdx], pcJunctions[srcIdx]);
  
  // Optional Visual Effect: Flash the Source T-Connector white for a split second 
  // to show the data packet hitting the main bus line
  leds[tConnectors[srcIdx]] = CRGB::White;
  FastLED.show();
  delay(80);
  leds[tConnectors[srcIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  // PHASE 2: Travel the Main Bus (Only if the T-Connectors are different)
  if (tConnectors[srcIdx] != tConnectors[destIdx]) {
    animateSegment(tConnectors[srcIdx], tConnectors[destIdx]);
    
    // Flash the Destination T-Connector
    leds[tConnectors[destIdx]] = CRGB::White;
    FastLED.show();
    delay(80);
    leds[tConnectors[destIdx]] = backgroundOn ? bgColor : CRGB::Black;
  }
  
  // PHASE 3: Destination Junction to the Destination PC
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