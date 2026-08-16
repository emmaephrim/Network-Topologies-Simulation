#include <FastLED.h>
#include <IRremote.hpp>

// --- HARDWARE SETTINGS ---
#define LED_PIN     6       
#define IR_PIN      2       
#define BUZZER_PIN  8       
#define NUM_LEDS    188     // Updated to your maximum pixel count (148-188)

CRGB leds[NUM_LEDS];

// --- MESH TOPOLOGY MAPPING ---
// Converting your 1-indexed diagram to 0-indexed C++ arrays (Subtracting 1)

// The designated pixel to light up yellow/green for each PC
// PC1 (1), PC2 (27), PC3 (53), PC4 (78)
const int pcNodes[4] = {0, 26, 52, 77}; 

// Routing Matrix: pathStarts[Source][Destination]
// -1 means it's a self-path (PC1 to PC1) and will trigger an error
const int pathStarts[4][4] = {
  { -1,   0, 104, 103 }, // PC1 to: (PC1, PC2, PC3, PC4)
  { 26,  -1,  27, 187 }, // PC2 to: (PC1, PC2, PC3, PC4)
  {144,  52,  -1,  53 }, // PC3 to: (PC1, PC2, PC3, PC4)
  { 78, 147,  77,  -1 }  // PC4 to: (PC1, PC2, PC3, PC4)
};

// Routing Matrix: pathEnds[Source][Destination]
const int pathEnds[4][4] = {
  { -1,  26, 144,  78 }, // PC1 to: (PC1, PC2, PC3, PC4)
  {  0,  -1,  52, 147 }, // PC2 to: (PC1, PC2, PC3, PC4)
  {104,  27,  -1,  77 }, // PC3 to: (PC1, PC2, PC3, PC4)
  {103, 187,  53,  -1 }  // PC4 to: (PC1, PC2, PC3, PC4)
};

// --- STATE VARIABLES ---
int sourcePC = 0; 
int currentBrightness = 100;    
bool backgroundOn = false;      

// Colors matched to the Ring & Bus topologies
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
  
  Serial.println("System Ready - Mesh Topology (Full Matrix)");
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
          simulateMesh(currentSource, destPC);
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

// --- MESH TOPOLOGY SIMULATION LOGIC ---
void simulateMesh(int src, int dest) {
  updateBackground(); 
  
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  // Look up the exact start and end pixels using our 2D Matrix
  int startPixel = pathStarts[srcIdx][destIdx];
  int endPixel = pathEnds[srcIdx][destIdx];
  
  // Animate the dedicated point-to-point link
  animateSegment(startPixel, endPixel);
  
  // Destination reached
  leds[pcNodes[destIdx]] = CRGB::Green;
  FastLED.show();
  successBeep();
  
  delay(1200); 
  updateBackground(); 
}

// Moves the packet bi-directionally along any branch or diagonal
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