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
#define MAX_LEDS 182
CRGB leds[MAX_LEDS];

CLEDController* ctrlRing;
CLEDController* ctrlMesh;
CLEDController* ctrlStar;
CLEDController* ctrlBus;

// --- TOPOLOGY MAPPINGS ---

// 1. RING MAPPING
const int ringPcNodes[4] = {0, 11, 22, 33}; 

// 2. MESH MAPPING 
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
int activeTopology = 1;         
int sourcePC = 0; 
int currentBrightness = 100;    
bool backgroundOn = false;      
bool randomMode = false;        
bool demoBuzzerOn = false;      
volatile bool isResetTriggered = false; // Flag to instantly abort animations

CRGB bgColor = CRGB(15, 6, 0);     
CRGB packetColor = CRGB::Blue;     

void setup() {
  Serial.begin(9600);
  
  randomSeed(analogRead(0));
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  ctrlRing = &FastLED.addLeds<WS2812B, RING_PIN, GRB>(leds, NUM_RING);
  ctrlMesh = &FastLED.addLeds<WS2812B, MESH_PIN, GRB>(leds, NUM_MESH);
  ctrlStar = &FastLED.addLeds<WS2812B, STAR_PIN, GRB>(leds, NUM_STAR);
  ctrlBus  = &FastLED.addLeds<WS2812B, BUS_PIN, GRB>(leds, NUM_BUS);
  
  FastLED.setBrightness(currentBrightness); 
  updateBackground();
  
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("System Ready - Master Controller (Interrupts Enabled)");
}

void loop() {
  
  // 0. Instantly handle a system reset if the flag was tripped during an animation
  if (isResetTriggered) {
    executeReset();
  }

  // 1. Process incoming IR commands
  if (IrReceiver.decode()) {
    
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }

    int command = IrReceiver.decodedIRData.command;
    int action = getActionFromIR(command);

    // --- HANDLE SYSTEM RESET (STAR BUTTON) ---
    if (action == 11) {
      executeReset();
    }
    
    // --- HANDLE RANDOM MODE TOGGLE ('5' BUTTON) ---
    else if (action == 9) {
      randomMode = !randomMode;
      sourcePC = 0; 
      
      if (!randomMode) {
        demoBuzzerOn = false; 
        updateBackground();
      }
      Serial.print("Random Demo Mode: "); Serial.println(randomMode ? "ON" : "OFF");
      
      digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW);
      delay(300);
    }
    
    // --- HANDLE DEMO BUZZER TOGGLE (# BUTTON) ---
    else if (action == 10) {
      if (randomMode) { 
        demoBuzzerOn = !demoBuzzerOn;
        Serial.print("Demo Mode Buzzer: "); Serial.println(demoBuzzerOn ? "ON" : "OFF");
        if (demoBuzzerOn) {
          digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW);
        }
        delay(300);
      }
    }
    
    // --- HANDLE TOPOLOGY SWITCHING (OK BUTTON) ---
    else if (action == 8) { 
      if (sourcePC >= 1 && sourcePC <= 4) {
        randomMode = false; 
        demoBuzzerOn = false;
        
        fill_solid(leds, MAX_LEDS, CRGB::Black);
        FastLED.show(); 
        
        activeTopology = sourcePC; 
        sourcePC = 0;              
        
        updateBackground();        
        
        digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100);
        digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
      }
    }
    
    // --- HANDLE PC SELECTION (1-4) ---
    else if (action >= 1 && action <= 4) {
      randomMode = false; 
      demoBuzzerOn = false;
      
      if (sourcePC == 0) {
        sourcePC = action;
        
        if (activeTopology == 1) leds[ringPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 2) leds[meshPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 3) leds[starPcNodes[sourcePC - 1]] = CRGB::Yellow;
        else if (activeTopology == 4) leds[busPcNodes[sourcePC - 1]] = CRGB::Yellow;
        
        showActiveStrip();
      } else {
        int destPC = action;
        int currentSource = sourcePC; 
        sourcePC = 0; 
        
        if (currentSource != destPC) {
          if (activeTopology == 1) simulateRing(currentSource, destPC);
          else if (activeTopology == 2) simulateMesh(currentSource, destPC);
          else if (activeTopology == 3) simulateStar(currentSource, destPC);
          else if (activeTopology == 4) simulateBus(currentSource, destPC);
        } else {
          errorBeep();
          updateBackground(); 
        }
      }
      delay(300); 
    }
    
    // --- HANDLE VISUAL CONTROLS (UP/DOWN/BG_TOGGLE) ---
    else if (action == 5) {
      currentBrightness += 25;
      if (currentBrightness > 255) currentBrightness = 255;
      FastLED.setBrightness(currentBrightness);
      showActiveStrip(); 
      delay(150);
    }
    else if (action == 6) {
      currentBrightness -= 25;
      if (currentBrightness < 10) currentBrightness = 10;
      FastLED.setBrightness(currentBrightness);
      showActiveStrip(); 
      delay(150);
    }
    else if (action == 7) {
      backgroundOn = !backgroundOn; 
      updateBackground();
      delay(300);
    }
    
    IrReceiver.resume(); 
  }

  // 2. Execute Random Demo Mode (If active)
  if (randomMode && !isResetTriggered) {
    int nextTopology = random(1, 5); 
    
    if (nextTopology != activeTopology) {
      fill_solid(leds, MAX_LEDS, CRGB::Black);
      FastLED.show(); 
      activeTopology = nextTopology;
      updateBackground();
    }
    
    int randSrc = random(1, 5); 
    int randDest = random(1, 5);
    
    while (randDest == randSrc) {
      randDest = random(1, 5);
    }

    if (activeTopology == 1) simulateRing(randSrc, randDest);
    else if (activeTopology == 2) simulateMesh(randSrc, randDest);
    else if (activeTopology == 3) simulateStar(randSrc, randDest);
    else if (activeTopology == 4) simulateBus(randSrc, randDest);

    // Wait before jumping to next topology, but allow interruption
    checkInterrupt(600); 
  }
}

// ==========================================
//          INTERRUPT & RESET LOGIC
// ==========================================

// Replaces standard delay(). Polls for the Star key (22) during animations.
// Returns true if a reset was requested, forcing animations to abort.
bool checkInterrupt(int ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    if (IrReceiver.decode()) {
      if (IrReceiver.decodedIRData.command == 22) { // Star Key Detected
        isResetTriggered = true;
        IrReceiver.resume();
        return true; 
      }
      // Swallow other random button presses during animations to prevent glitches
      IrReceiver.resume(); 
    }
    delay(5);
  }
  return false;
}

// Restores system to factory default
void executeReset() {
  activeTopology = 1;
  sourcePC = 0;
  backgroundOn = false;
  randomMode = false;
  demoBuzzerOn = false;
  currentBrightness = 100;
  isResetTriggered = false;
  
  FastLED.setBrightness(currentBrightness);
  fill_solid(leds, MAX_LEDS, CRGB::Black);
  FastLED.show(); 
  updateBackground();
  
  Serial.println(">>> SYSTEM RESET TO DEFAULT <<<");
  
  // Reset Confirmation Tone (Long solid beep)
  digitalWrite(BUZZER_PIN, HIGH); delay(400); digitalWrite(BUZZER_PIN, LOW);
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
    if (isResetTriggered) return;
    
    leds[currentIdx] = packetColor; 
    showActiveStrip();
    if (checkInterrupt(150)) return;
    
    leds[currentIdx] = backgroundOn ? bgColor : CRGB::Black;
    currentIdx++; 
    if (currentIdx >= NUM_RING) currentIdx = 0; 
  }
  
  leds[endIdx] = CRGB::Green;
  showActiveStrip();
  successBeep();
  if (checkInterrupt(1200)) return;
  updateBackground(); 
}

void simulateMesh(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  int startPixel = meshPathStarts[srcIdx][destIdx];
  int endPixel = meshPathEnds[srcIdx][destIdx];
  
  animateSegment(startPixel, endPixel);
  if (isResetTriggered) return;
  
  leds[meshPcNodes[destIdx]] = CRGB::Green;
  showActiveStrip();
  successBeep();
  if (checkInterrupt(1200)) return;
  updateBackground(); 
}

void simulateStar(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  animateSegment(starPcNodes[srcIdx], starHubNodes[srcIdx]);
  if (isResetTriggered) return;
  
  for(int i = 0; i < 4; i++) leds[starHubNodes[i]] = CRGB::White;
  showActiveStrip();
  if (checkInterrupt(150)) return; 
  
  for(int i = 0; i < 4; i++) leds[starHubNodes[i]] = backgroundOn ? bgColor : CRGB::Black;
  showActiveStrip();
  
  animateSegment(starHubNodes[destIdx], starPcNodes[destIdx]);
  if (isResetTriggered) return;
  
  leds[starPcNodes[destIdx]] = CRGB::Green;
  showActiveStrip();
  successBeep();
  if (checkInterrupt(1200)) return;
  updateBackground(); 
}

void simulateBus(int src, int dest) {
  updateBackground(); 
  int srcIdx = src - 1;
  int destIdx = dest - 1;
  
  animateSegment(busPcNodes[srcIdx], busBranchBase[srcIdx]);
  if (isResetTriggered) return;
  
  leds[busPoint[srcIdx]] = CRGB::White;
  showActiveStrip();
  if (checkInterrupt(80)) return;
  leds[busPoint[srcIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  if (busPoint[srcIdx] != busPoint[destIdx]) {
    animateSegment(busPoint[srcIdx], busPoint[destIdx]);
    if (isResetTriggered) return;
  }
  
  leds[busPoint[destIdx]] = CRGB::White;
  showActiveStrip();
  if (checkInterrupt(80)) return;
  leds[busPoint[destIdx]] = backgroundOn ? bgColor : CRGB::Black;
  
  animateSegment(busBranchBase[destIdx], busPcNodes[destIdx]);
  if (isResetTriggered) return;
  
  leds[busPcNodes[destIdx]] = CRGB::Green;
  showActiveStrip();
  successBeep();
  if (checkInterrupt(1200)) return;
  updateBackground(); 
}

// ==========================================
//          HELPER FUNCTIONS
// ==========================================

void showActiveStrip() {
  if (activeTopology == 1) ctrlRing->showLeds(currentBrightness);
  else if (activeTopology == 2) ctrlMesh->showLeds(currentBrightness);
  else if (activeTopology == 3) ctrlStar->showLeds(currentBrightness);
  else if (activeTopology == 4) ctrlBus->showLeds(currentBrightness);
}

void animateSegment(int startIdx, int endIdx) {
  int step = (startIdx < endIdx) ? 1 : -1;
  int currentIdx = startIdx;
  
  while (true) {
    if (isResetTriggered) return; 
    
    leds[currentIdx] = packetColor; 
    showActiveStrip();
    
    if (checkInterrupt(100)) return; 
    
    leds[currentIdx] = backgroundOn ? bgColor : CRGB::Black;
    if (currentIdx == endIdx) break;
    currentIdx += step; 
  }
}

void updateBackground() {
  fill_solid(leds, MAX_LEDS, CRGB::Black); 
  
  if (backgroundOn) {
    int numLedsToFill = 0;
    if (activeTopology == 1) numLedsToFill = NUM_RING;
    else if (activeTopology == 2) numLedsToFill = NUM_MESH;
    else if (activeTopology == 3) numLedsToFill = NUM_STAR;
    else if (activeTopology == 4) numLedsToFill = NUM_BUS;
    
    fill_solid(leds, numLedsToFill, bgColor);
  }
  
  if (sourcePC != 0) {
    if (activeTopology == 1) leds[ringPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 2) leds[meshPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 3) leds[starPcNodes[sourcePC - 1]] = CRGB::Yellow;
    else if (activeTopology == 4) leds[busPcNodes[sourcePC - 1]] = CRGB::Yellow;
  }
  
  showActiveStrip(); 
}

void successBeep() {
  if (randomMode && !demoBuzzerOn) return; 
  digitalWrite(BUZZER_PIN, HIGH); if (checkInterrupt(100)) return;
  digitalWrite(BUZZER_PIN, LOW);  if (checkInterrupt(100)) return;
  digitalWrite(BUZZER_PIN, HIGH); if (checkInterrupt(100)) return;
  digitalWrite(BUZZER_PIN, LOW);
}

void errorBeep() {
  if (randomMode && !demoBuzzerOn) return; 
  digitalWrite(BUZZER_PIN, HIGH); checkInterrupt(500);
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
    case 28: return 8;  // OK Button
    
    case 64: return 9;  // '5' Key -> Random Demo Mode Toggle
    case 13: return 10; // '#' Key -> Demo Buzzer Toggle
    case 22: return 11; // '*' Key -> Immediate System Reset
    
    default: return -1; 
  }
}