#include <FastLED.h>
#include <IRremote.hpp>

// --- SHARED HARDWARE SETTINGS ---
#define IR_PIN      2       
#define BUZZER_PIN  8       

// --- TOPOLOGY PIN & LED SETTINGS ---
#define RING_PIN    3
#define NUM_RING    45      
CRGB ringLeds[NUM_RING];

#define MESH_PIN    4
#define NUM_MESH    188     
CRGB meshLeds[NUM_MESH];

#define STAR_PIN    5
#define NUM_STAR    61      
CRGB starLeds[NUM_STAR];

#define BUS_PIN     6
#define NUM_BUS     83      
CRGB busLeds[NUM_BUS];

// --- TOPOLOGY MAPPINGS ---

// 1. RING MAPPING
const int ringPcNodes[4] = {0, 11, 22, 33}; 

// 2. MESH MAPPING
const int meshPcNodes[4] = {0, 26, 52, 77}; 
const int meshPathStarts[4][4] = {
  { -1,   0, 104, 103 }, 
  { 26,  -1,  27, 187 }, 
  {144,  52,  -1,  53 }, 
  { 78, 147,  77,  -1 }  
};
const int meshPathEnds[4][4] = {
  { -1,  26, 144,  78 }, 
  {  0,  -1,  52, 147 }, 
  {104,  27,  -1,  77 }, 
  {103, 187,  53,  -1 }  
};

// 3. STAR MAPPING
const int starPcNodes[4]  = {0, 30, 31, 60}; 
const int starHubNodes[4] = {15, 16, 45, 46}; 

// 4. BUS MAPPING
const int busPcNodes[4]    = {2, 67, 68, 52}; 
const int busBranchBase[4] = {15, 53, 82, 40}; 
const int busPoint[4]      = {16, 33, 19, 39}; 

// --- MASTER STATE VARIABLES ---
int activeTopology = 1;         // 1=Ring, 2=Mesh, 3=Star, 4=Bus (Defaults to Ring)
int sourcePC = 0; 
int currentBrightness = 100;    
bool backgroundOn = false;      

CRGB bgColor = CRGB(15, 6, 0);     // Dim Amber/Copper
CRGB packetColor = CRGB::Blue;     // Classic Blue packet

void setup() {
  Serial.begin(9600);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  // Initialize all 4 strips on their respective pins
  FastLED.addLeds<WS2812B, RING_PIN, GRB>(ringLeds, NUM_RING);
  FastLED.addLeds<WS2812B, MESH_PIN, GRB>(meshLeds, NUM_MESH);
  FastLED.addLeds<WS2812B, STAR_PIN, GRB>(starLeds, NUM_STAR);
  FastLED.addLeds<WS2812B, BUS_PIN, GRB>(busLeds, NUM_BUS);
  
  FastLED.setBrightness(currentBrightness); 
  updateBackground();
  
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("System Ready - Unified Master Controller");
  Serial.println("Active Topology: RING");
}

void loop() {
  if (IrReceiver.decode()) {
    
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }

    int command = IrReceiver.decodedIRData.command;
    int action = getActionFromIR(command);

    // --- HANDLE TOPOLOGY SWITCHING (OK BUTTON) ---
    if (action == 8) { 
      if (sourcePC >= 1 && sourcePC <= 4) {
        activeTopology = sourcePC; // Set the new topology based on the preceding number
        sourcePC = 0;              // Clear the PC selection state
        
        FastLED.clear();           // Black out all strips
        updateBackground();        // Apply background to the new active strip
        
        // Mode switch confirmation: Double Beep
        digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100);
        digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
        
        Serial.print("Switched Topology to Mode: "); Serial.println(activeTopology);
      }
    }
    
    // --- HANDLE PC SELECTION & SIMULATION (1-4) ---
    else if (action >= 1 && action <= 4) {
      if (sourcePC == 0) {
        sourcePC = action;
        
        // Light up the source node yellow ONLY on the currently active topology
        if (activeTopology == 1) ringLeds[ringPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 2) meshLeds[meshPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 3) starLeds[starPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 4) busLeds[busPcNodes[sourcePC - 1]] = CRGB::Yellow;
        
        FastLED.show();
      } else {
        int destPC = action;
        int currentSource = sourcePC; 
        sourcePC = 0; // Clear state before animating
        
        if (currentSource != destPC) {
          // Route the simulation request to the active topology's specific logic
          if (activeTopology == 1) simulateRing(currentSource, destPC);
          else if (activeTopology == 2) simulateMesh(currentSource, destPC);
          else if (activeTopology == 3) simulateStar(currentSource, destPC);
          else if (activeTopology == 4) simulateBus(currentSource, destPC);
        } else {
          Serial.println("Error: Source and Destination cannot be the same.");
          errorBeep();
          updateBackground(); 
        }
      }
      delay(300); 
    }
    
    // --- HANDLE VISUAL CONTROLS (5-7) ---
    else if (action == 5) {
      currentBrightness += 25;
      if (currentBrightness > 255) currentBrightness = 255;
      FastLED.setBrightness(currentBrightness);
      FastLED.show();
      delay(150);
    }
    else if (action == 6) {
      currentBrightness -= 25;
      if (currentBrightness < 10) currentBrightness = 10;
      FastLED.setBrightness(currentBrightness);
      FastLED.show();
      delay(150);
    }
    else if (action == 7) {
      backgroundOn = !backgroundOn; 
      updateBackground();
      delay(300);
    }
    
    IrReceiver.resume(); 
  }
}

// ==========================================
//          SIMULATION ROUTINES
// ==========================================

// --- RING LOGIC ---
void simulateRing(int src, int dest) {
  updateBackground(); 
  int startIdx = ringPcNodes[src - 1];
  int endIdx = ringPcNodes[dest - 1];
  int currentIdx = startIdx;
  
  while (currentIdx != endIdx) {
    ringLeds[currentIdx] = packetColor; 
    FastLED.show();
    delay(150); 
    
    ringLeds[currentIdx] = backgroundOn ? bgColor : CRGB::Black;
    currentIdx++; 
    if (currentIdx >= NUM_RING) currentIdx = 0; 
  }
  
  ringLeds[endIdx] = CRGB::Green;
  FastLED.show();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

// --- MESH LOGIC ---
void simulateMesh(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  int startPixel = meshPathStarts[srcIdx][destIdx];
  int endPixel = meshPathEnds[srcIdx][destIdx];
  
  animateSegment(meshLeds, startPixel, endPixel);
  
  meshLeds[meshPcNodes[destIdx]] = CRGB::Green;
  FastLED.show();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

// --- STAR LOGIC ---
void simulateStar(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  animateSegment(starLeds, starPcNodes[srcIdx], starHubNodes[srcIdx]);
  
  for(int i = 0; i < 4; i++) starLeds[starHubNodes[i]] = CRGB::White;
  FastLED.show();
  delay(150); 
  
  for(int i = 0; i < 4; i++) starLeds[starHubNodes[i]] = backgroundOn ? bgColor : CRGB::Black;
  FastLED.show();
  
  animateSegment(starLeds, starHubNodes[destIdx], starPcNodes[destIdx]);
  
  starLeds[starPcNodes[destIdx]] = CRGB::Green;
  FastLED.show();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

// --- BUS LOGIC ---
void simulateBus(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  animateSegment(busLeds, busPcNodes[srcIdx], busBranchBase[srcIdx]);
  
  busLeds[busPoint[srcIdx]] = CRGB::White;
  FastLED.show();
  delay(80);
  busLeds[busPoint[srcIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  if (busPoint[srcIdx] != busPoint[destIdx]) {
    // FIX: Added 'busLeds' pointer to the segment animation
    animateSegment(busLeds, busPoint[srcIdx], busPoint[destIdx]);
  }
  
  busLeds[busPoint[destIdx]] = CRGB::White;
  FastLED.show();
  delay(80);
  busLeds[busPoint[destIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  animateSegment(busLeds, busBranchBase[destIdx], busPcNodes[destIdx]);
  
  busLeds[busPcNodes[destIdx]] = CRGB::Green;
  FastLED.show();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

// ==========================================
//          HELPER FUNCTIONS
// ==========================================

// Universal segment animator - Takes a pointer to the active LED array
void animateSegment(CRGB* activeStrip, int startIdx, int endIdx) {
  int step = (startIdx < endIdx) ? 1 : -1;
  int currentIdx = startIdx;
  
  while (true) {
    activeStrip[currentIdx] = packetColor; 
    FastLED.show();
    delay(100); 
    
    activeStrip[currentIdx] = backgroundOn ? bgColor : CRGB::Black;
    if (currentIdx == endIdx) break;
    currentIdx += step; 
  }
}

void updateBackground() {
  FastLED.clear(); // Clear all strips first
  
  if (backgroundOn) {
    if (activeTopology == 1) fill_solid(ringLeds, NUM_RING, bgColor);
    else if (activeTopology == 2) fill_solid(meshLeds, NUM_MESH, bgColor);
    else if (activeTopology == 3) fill_solid(starLeds, NUM_STAR, bgColor);
    else if (activeTopology == 4) fill_solid(busLeds, NUM_BUS, bgColor);
  }
  
  // Re-apply the yellow source indicator if one is currently selected
  if (sourcePC != 0) {
    if (activeTopology == 1) ringLeds[ringPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 2) meshLeds[meshPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 3) starLeds[starPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 4) busLeds[busPcNodes[sourcePC - 1]] = CRGB::Yellow;
  }
  
  FastLED.show(); // Push the updates to the hardware
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
    case 28: return 8; // OK Button
    default: return -1; 
  }
}