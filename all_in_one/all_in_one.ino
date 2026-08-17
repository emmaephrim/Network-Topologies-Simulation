#include <FastLED.h>
#include <IRremote.hpp>

// --- SHARED HARDWARE SETTINGS ---
#define IR_PIN      2       
#define BUZZER_PIN  8       

// --- TOPOLOGY PIN & LED SETTINGS ---
#define RING_PIN    3
#define NUM_RING    45      

#define MESH_PIN    4
#define NUM_MESH    182     

#define STAR_PIN    5
#define NUM_STAR    61      

#define BUS_PIN     6
#define NUM_BUS     83      

// --- MEMORY OPTIMIZATION: SHARED BUFFER ---
// Instead of 4 arrays, we use 1 array sized for the largest strip (Mesh: 182)
#define MAX_LEDS 182
CRGB leds[MAX_LEDS];

// We use controllers to target specific physical pins using the shared buffer
CLEDController* ctrlRing;
CLEDController* ctrlMesh;
CLEDController* ctrlStar;
CLEDController* ctrlBus;

// --- TOPOLOGY MAPPINGS ---

// 1. RING MAPPING
const int ringPcNodes[4] = {0, 11, 22, 33}; 

// 2. MESH MAPPING (Updated Diagonals)
const int meshPcNodes[4] = {0, 26, 52, 77}; 
const int meshPathStarts[4][4] = {
  { -1,   0, 104, 103 }, 
  { 26,  -1,  27, 181 }, 
  {142,  52,  -1,  53 }, 
  { 78, 143,  77,  -1 }  
};
const int meshPathEnds[4][4] = {
  { -1,  26, 142,  78 }, 
  {  0,  -1,  52, 143 }, 
  {104,  27,  -1,  77 }, 
  {103, 181,  53,  -1 }  
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
  
  // Bind all 4 pins to the exact same shared 'leds' buffer
  ctrlRing = &FastLED.addLeds<WS2812B, RING_PIN, GRB>(leds, NUM_RING);
  ctrlMesh = &FastLED.addLeds<WS2812B, MESH_PIN, GRB>(leds, NUM_MESH);
  ctrlStar = &FastLED.addLeds<WS2812B, STAR_PIN, GRB>(leds, NUM_STAR);
  ctrlBus  = &FastLED.addLeds<WS2812B, BUS_PIN, GRB>(leds, NUM_BUS);
  
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
        
        // 1. Black out all strips immediately before switching
        fill_solid(leds, MAX_LEDS, CRGB::Black);
        FastLED.show(); // Pushes black to all 4 strips to clear the old active one
        
        // 2. Switch the routing state
        activeTopology = sourcePC; 
        sourcePC = 0;              
        
        // 3. Draw the background on the NEW active strip
        updateBackground();        
        
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
        
        // Light up the source node yellow ONLY in the shared buffer mapped for the active topology
        if (activeTopology == 1) leds[ringPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 2) leds[meshPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 3) leds[starPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 4) leds[busPcNodes[sourcePC - 1]] = CRGB::Yellow;
        
        showActiveStrip();
      } else {
        int destPC = action;
        int currentSource = sourcePC; 
        sourcePC = 0; // Clear state before animating
        
        if (currentSource != destPC) {
          // Route the simulation request
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
      showActiveStrip(); 
      Serial.print("Brightness: "); Serial.println(currentBrightness);
      delay(150);
    }
    else if (action == 6) {
      currentBrightness -= 25;
      if (currentBrightness < 10) currentBrightness = 10;
      FastLED.setBrightness(currentBrightness);
      showActiveStrip(); 
      Serial.print("Brightness: "); Serial.println(currentBrightness);
      delay(150);
    }
    else if (action == 7) {
      backgroundOn = !backgroundOn; 
      updateBackground();
      Serial.print("Background turned "); 
      Serial.println(backgroundOn ? "ON" : "OFF");
      delay(300);
    }
    
    IrReceiver.resume(); 
  }
}

// ==========================================
//          SIMULATION ROUTINES
// ==========================================

void simulateRing(int src, int dest) {
  updateBackground(); 
  int startIdx = ringPcNodes[src - 1];
  int endIdx = ringPcNodes[dest - 1];
  int currentIdx = startIdx;
  
  while (currentIdx != endIdx) {
    leds[currentIdx] = packetColor; 
    showActiveStrip();
    delay(150); 
    
    leds[currentIdx] = backgroundOn ? bgColor : CRGB::Black;
    currentIdx++; 
    if (currentIdx >= NUM_RING) currentIdx = 0; 
  }
  
  leds[endIdx] = CRGB::Green;
  showActiveStrip();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

void simulateMesh(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  int startPixel = meshPathStarts[srcIdx][destIdx];
  int endPixel = meshPathEnds[srcIdx][destIdx];
  
  animateSegment(startPixel, endPixel);
  
  leds[meshPcNodes[destIdx]] = CRGB::Green;
  showActiveStrip();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

void simulateStar(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  animateSegment(starPcNodes[srcIdx], starHubNodes[srcIdx]);
  
  for(int i = 0; i < 4; i++) leds[starHubNodes[i]] = CRGB::White;
  showActiveStrip();
  delay(150); 
  
  for(int i = 0; i < 4; i++) leds[starHubNodes[i]] = backgroundOn ? bgColor : CRGB::Black;
  showActiveStrip();
  
  animateSegment(starHubNodes[destIdx], starPcNodes[destIdx]);
  
  leds[starPcNodes[destIdx]] = CRGB::Green;
  showActiveStrip();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

void simulateBus(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  animateSegment(busPcNodes[srcIdx], busBranchBase[srcIdx]);
  
  leds[busPoint[srcIdx]] = CRGB::White;
  showActiveStrip();
  delay(80);
  leds[busPoint[srcIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  if (busPoint[srcIdx] != busPoint[destIdx]) {
    animateSegment(busPoint[srcIdx], busPoint[destIdx]);
  }
  
  leds[busPoint[destIdx]] = CRGB::White;
  showActiveStrip();
  delay(80);
  leds[busPoint[destIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  animateSegment(busBranchBase[destIdx], busPcNodes[destIdx]);
  
  leds[busPcNodes[destIdx]] = CRGB::Green;
  showActiveStrip();
  successBeep();
  delay(1200); 
  updateBackground(); 
}

// ==========================================
//          HELPER FUNCTIONS
// ==========================================

// Targets ONLY the physical pin of the currently selected topology
void showActiveStrip() {
  if (activeTopology == 1) ctrlRing->showLeds(currentBrightness);
  else if (activeTopology == 2) ctrlMesh->showLeds(currentBrightness);
  else if (activeTopology == 3) ctrlStar->showLeds(currentBrightness);
  else if (activeTopology == 4) ctrlBus->showLeds(currentBrightness);
}

// Animator uses the shared 'leds' buffer directly
void animateSegment(int startIdx, int endIdx) {
  int step = (startIdx < endIdx) ? 1 : -1;
  int currentIdx = startIdx;
  
  while (true) {
    leds[currentIdx] = packetColor; 
    showActiveStrip();
    delay(100); 
    
    leds[currentIdx] = backgroundOn ? bgColor : CRGB::Black;
    if (currentIdx == endIdx) break;
    currentIdx += step; 
  }
}

void updateBackground() {
  fill_solid(leds, MAX_LEDS, CRGB::Black); // Wipe the shared buffer clean
  
  if (backgroundOn) {
    int numLedsToFill = 0;
    if (activeTopology == 1) numLedsToFill = NUM_RING;
    else if (activeTopology == 2) numLedsToFill = NUM_MESH;
    else if (activeTopology == 3) numLedsToFill = NUM_STAR;
    else if (activeTopology == 4) numLedsToFill = NUM_BUS;
    
    fill_solid(leds, numLedsToFill, bgColor);
  }
  
  // Re-apply the yellow source indicator if one is currently selected
  if (sourcePC != 0) {
    if (activeTopology == 1) leds[ringPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 2) leds[meshPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 3) leds[starPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 4) leds[busPcNodes[sourcePC - 1]] = CRGB::Yellow;
  }
  
  showActiveStrip(); // Push the updates only to the active hardware
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