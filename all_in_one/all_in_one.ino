#include <FastLED.h>
#include <IRremote.hpp>
#include <avr/pgmspace.h>

// ============================================================
//                    HARDWARE SETTINGS
// ============================================================

#define IR_PIN      2
#define BUZZER_PIN  8

#define RING_PIN    3
#define NUM_RING    45

#define MESH_PIN    4
#define NUM_MESH    188

#define STAR_PIN    5
#define NUM_STAR    61

#define BUS_PIN     6
#define NUM_BUS     83


// ============================================================
//                    LED ARRAYS
// ============================================================

CRGB ringLeds[NUM_RING];
CRGB meshLeds[NUM_MESH];
CRGB starLeds[NUM_STAR];
CRGB busLeds[NUM_BUS];


// ============================================================
//                  TOPOLOGY MAPPINGS
//
// IMPORTANT:
// These values never change while the program is running,
// so store them in FLASH instead of SRAM.
// ============================================================

// -------------------- RING --------------------

const uint8_t ringPcNodes[4] PROGMEM = {
  0, 11, 22, 33
};


// -------------------- MESH --------------------

const uint8_t meshPcNodes[4] PROGMEM = {
  0, 26, 52, 77
};

const int8_t meshPathStarts[4][4] PROGMEM = {
  { -1,   0, 104, 103 },
  { 26,  -1,  27, 187 },
  {144,  52,  -1,  53 },
  { 78, 147,  77,  -1 }
};

const int8_t meshPathEnds[4][4] PROGMEM = {
  { -1,  26, 144,  78 },
  {  0,  -1,  52, 147 },
  {104,  27,  -1,  77 },
  {103, 187,  53,  -1 }
};


// -------------------- STAR --------------------

const uint8_t starPcNodes[4] PROGMEM = {
  0, 30, 31, 60
};

const uint8_t starHubNodes[4] PROGMEM = {
  15, 16, 45, 46
};


// -------------------- BUS --------------------

const uint8_t busPcNodes[4] PROGMEM = {
  2, 67, 68, 52
};

const uint8_t busBranchBase[4] PROGMEM = {
  15, 53, 82, 40
};

const uint8_t busPoint[4] PROGMEM = {
  16, 33, 19, 39
};


// ============================================================
//                    MASTER STATE
// ============================================================

uint8_t activeTopology = 1;
// 1 = Ring
// 2 = Mesh
// 3 = Star
// 4 = Bus

uint8_t sourcePC = 0;

uint8_t currentBrightness = 100;

bool backgroundOn = false;

const CRGB bgColor = CRGB(15, 6, 0);
const CRGB packetColor = CRGB::Blue;


// ============================================================
//                         SETUP
// ============================================================

void setup() {

  Serial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize LED strips
  FastLED.addLeds<WS2812B, RING_PIN, GRB>(
    ringLeds,
    NUM_RING
  );

  FastLED.addLeds<WS2812B, MESH_PIN, GRB>(
    meshLeds,
    NUM_MESH
  );

  FastLED.addLeds<WS2812B, STAR_PIN, GRB>(
    starLeds,
    NUM_STAR
  );

  FastLED.addLeds<WS2812B, BUS_PIN, GRB>(
    busLeds,
    NUM_BUS
  );

  FastLED.setBrightness(currentBrightness);

  updateBackground();

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  Serial.println(F("System Ready - Unified Master Controller"));
  Serial.println(F("Active Topology: RING"));
}


// ============================================================
//                         MAIN LOOP
// ============================================================

void loop() {

  if (IrReceiver.decode()) {

    // Ignore repeated IR commands
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }

    uint8_t command = IrReceiver.decodedIRData.command;
    int8_t action = getActionFromIR(command);


    // ========================================================
    //              TOPOLOGY SWITCHING - OK
    // ========================================================

    if (action == 8) {

      if (sourcePC >= 1 && sourcePC <= 4) {

        activeTopology = sourcePC;
        sourcePC = 0;

        FastLED.clear();
        updateBackground();

        // Double beep
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);

        delay(100);

        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);

        Serial.print(F("Switched Topology to Mode: "));
        Serial.println(activeTopology);
      }
    }


    // ========================================================
    //                   PC SELECTION
    // ========================================================

    else if (action >= 1 && action <= 4) {

      // First number = source PC
      if (sourcePC == 0) {

        sourcePC = action;

        showSourcePC(sourcePC);
      }

      // Second number = destination PC
      else {

        uint8_t destPC = action;
        uint8_t currentSource = sourcePC;

        sourcePC = 0;

        if (currentSource != destPC) {

          if (activeTopology == 1) {
            simulateRing(currentSource, destPC);
          }

          else if (activeTopology == 2) {
            simulateMesh(currentSource, destPC);
          }

          else if (activeTopology == 3) {
            simulateStar(currentSource, destPC);
          }

          else if (activeTopology == 4) {
            simulateBus(currentSource, destPC);
          }
        }

        else {

          Serial.println(
            F("Error: Source and Destination cannot be the same.")
          );

          errorBeep();
          updateBackground();
        }
      }

      delay(300);
    }


    // ========================================================
    //                 BRIGHTNESS UP
    // ========================================================

    else if (action == 5) {

      currentBrightness += 25;

      if (currentBrightness > 255) {
        currentBrightness = 255;
      }

      FastLED.setBrightness(currentBrightness);
      FastLED.show();

      delay(150);
    }


    // ========================================================
    //                 BRIGHTNESS DOWN
    // ========================================================

    else if (action == 6) {

      if (currentBrightness > 25) {
        currentBrightness -= 25;
      }
      else {
        currentBrightness = 10;
      }

      FastLED.setBrightness(currentBrightness);
      FastLED.show();

      delay(150);
    }


    // ========================================================
    //                    BACKGROUND
    // ========================================================

    else if (action == 7) {

      backgroundOn = !backgroundOn;

      updateBackground();

      delay(300);
    }

    IrReceiver.resume();
  }
}


// ============================================================
//             DISPLAY CURRENT SOURCE PC
// ============================================================

void showSourcePC(uint8_t pc) {

  uint8_t index = pc - 1;

  if (activeTopology == 1) {

    uint8_t node =
      pgm_read_byte(&ringPcNodes[index]);

    ringLeds[node] = CRGB::Yellow;
  }

  else if (activeTopology == 2) {

    uint8_t node =
      pgm_read_byte(&meshPcNodes[index]);

    meshLeds[node] = CRGB::Yellow;
  }

  else if (activeTopology == 3) {

    uint8_t node =
      pgm_read_byte(&starPcNodes[index]);

    starLeds[node] = CRGB::Yellow;
  }

  else if (activeTopology == 4) {

    uint8_t node =
      pgm_read_byte(&busPcNodes[index]);

    busLeds[node] = CRGB::Yellow;
  }

  FastLED.show();
}


// ============================================================
//                       RING LOGIC
// ============================================================

void simulateRing(uint8_t src, uint8_t dest) {

  updateBackground();

  uint8_t startIdx =
    pgm_read_byte(&ringPcNodes[src - 1]);

  uint8_t endIdx =
    pgm_read_byte(&ringPcNodes[dest - 1]);

  uint8_t currentIdx = startIdx;

  while (currentIdx != endIdx) {

    ringLeds[currentIdx] = packetColor;

    FastLED.show();

    delay(150);

    ringLeds[currentIdx] =
      backgroundOn ? bgColor : CRGB::Black;

    currentIdx++;

    if (currentIdx >= NUM_RING) {
      currentIdx = 0;
    }
  }

  ringLeds[endIdx] = CRGB::Green;

  FastLED.show();

  successBeep();

  delay(1200);

  updateBackground();
}


// ============================================================
//                       MESH LOGIC
// ============================================================

void simulateMesh(uint8_t src, uint8_t dest) {

  updateBackground();

  uint8_t srcIdx = src - 1;
  uint8_t destIdx = dest - 1;

  int8_t startPixel =
    (int8_t)pgm_read_byte(
      &meshPathStarts[srcIdx][destIdx]
    );

  int8_t endPixel =
    (int8_t)pgm_read_byte(
      &meshPathEnds[srcIdx][destIdx]
    );

  animateSegment(
    meshLeds,
    startPixel,
    endPixel
  );

  uint8_t destinationNode =
    pgm_read_byte(&meshPcNodes[destIdx]);

  meshLeds[destinationNode] = CRGB::Green;

  FastLED.show();

  successBeep();

  delay(1200);

  updateBackground();
}


// ============================================================
//                       STAR LOGIC
// ============================================================

void simulateStar(uint8_t src, uint8_t dest) {

  updateBackground();

  uint8_t srcIdx = src - 1;
  uint8_t destIdx = dest - 1;

  uint8_t srcPCNode =
    pgm_read_byte(&starPcNodes[srcIdx]);

  uint8_t srcHubNode =
    pgm_read_byte(&starHubNodes[srcIdx]);

  uint8_t destPCNode =
    pgm_read_byte(&starPcNodes[destIdx]);

  uint8_t destHubNode =
    pgm_read_byte(&starHubNodes[destIdx]);


  // PC -> Hub
  animateSegment(
    starLeds,
    srcPCNode,
    srcHubNode
  );


  // Flash all hub nodes
  for (uint8_t i = 0; i < 4; i++) {

    uint8_t hub =
      pgm_read_byte(&starHubNodes[i]);

    starLeds[hub] = CRGB::White;
  }

  FastLED.show();

  delay(150);


  // Restore hubs
  for (uint8_t i = 0; i < 4; i++) {

    uint8_t hub =
      pgm_read_byte(&starHubNodes[i]);

    starLeds[hub] =
      backgroundOn ? bgColor : CRGB::Black;
  }

  FastLED.show();


  // Hub -> Destination PC
  animateSegment(
    starLeds,
    destHubNode,
    destPCNode
  );


  starLeds[destPCNode] = CRGB::Green;

  FastLED.show();

  successBeep();

  delay(1200);

  updateBackground();
}


// ============================================================
//                       BUS LOGIC
// ============================================================

void simulateBus(uint8_t src, uint8_t dest) {

  updateBackground();

  uint8_t srcIdx = src - 1;
  uint8_t destIdx = dest - 1;


  uint8_t srcPCNode =
    pgm_read_byte(&busPcNodes[srcIdx]);

  uint8_t srcBranch =
    pgm_read_byte(&busBranchBase[srcIdx]);

  uint8_t srcPoint =
    pgm_read_byte(&busPoint[srcIdx]);


  uint8_t destPCNode =
    pgm_read_byte(&busPcNodes[destIdx]);

  uint8_t destBranch =
    pgm_read_byte(&busBranchBase[destIdx]);

  uint8_t destPoint =
    pgm_read_byte(&busPoint[destIdx]);


  // Source PC -> branch
  animateSegment(
    busLeds,
    srcPCNode,
    srcBranch
  );


  // Source branch point
  busLeds[srcPoint] = CRGB::White;

  FastLED.show();

  delay(80);

  busLeds[srcPoint] =
    backgroundOn ? bgColor : CRGB::Black;


  // Move along the bus
  if (srcPoint != destPoint) {

    animateSegment(
      busLeds,
      srcPoint,
      destPoint
    );
  }


  // Destination point
  busLeds[destPoint] = CRGB::White;

  FastLED.show();

  delay(80);

  busLeds[destPoint] =
    backgroundOn ? bgColor : CRGB::Black;


  // Destination branch -> PC
  animateSegment(
    busLeds,
    destBranch,
    destPCNode
  );


  // Destination PC
  busLeds[destPCNode] = CRGB::Green;

  FastLED.show();

  successBeep();

  delay(1200);

  updateBackground();
}


// ============================================================
//                  GENERIC SEGMENT ANIMATOR
// ============================================================

void animateSegment(
  CRGB* activeStrip,
  int8_t startIdx,
  int8_t endIdx
) {

  int8_t step =
    (startIdx < endIdx) ? 1 : -1;

  int8_t currentIdx = startIdx;

  while (true) {

    activeStrip[currentIdx] = packetColor;

    FastLED.show();

    delay(100);

    activeStrip[currentIdx] =
      backgroundOn ? bgColor : CRGB::Black;

    if (currentIdx == endIdx) {
      break;
    }

    currentIdx += step;
  }
}


// ============================================================
//                    BACKGROUND UPDATE
// ============================================================

void updateBackground() {

  FastLED.clear();


  if (backgroundOn) {

    if (activeTopology == 1) {

      fill_solid(
        ringLeds,
        NUM_RING,
        bgColor
      );
    }

    else if (activeTopology == 2) {

      fill_solid(
        meshLeds,
        NUM_MESH,
        bgColor
      );
    }

    else if (activeTopology == 3) {

      fill_solid(
        starLeds,
        NUM_STAR,
        bgColor
      );
    }

    else if (activeTopology == 4) {

      fill_solid(
        busLeds,
        NUM_BUS,
        bgColor
      );
    }
  }


  // Restore source indicator
  if (sourcePC != 0) {
    showSourceWithoutRefresh();
  }

  FastLED.show();
}


// ============================================================
//          RE-APPLY SOURCE WITHOUT FastLED.show()
// ============================================================

void showSourceWithoutRefresh() {

  uint8_t index = sourcePC - 1;

  if (activeTopology == 1) {

    uint8_t node =
      pgm_read_byte(&ringPcNodes[index]);

    ringLeds[node] = CRGB::Yellow;
  }

  else if (activeTopology == 2) {

    uint8_t node =
      pgm_read_byte(&meshPcNodes[index]);

    meshLeds[node] = CRGB::Yellow;
  }

  else if (activeTopology == 3) {

    uint8_t node =
      pgm_read_byte(&starPcNodes[index]);

    starLeds[node] = CRGB::Yellow;
  }

  else if (activeTopology == 4) {

    uint8_t node =
      pgm_read_byte(&busPcNodes[index]);

    busLeds[node] = CRGB::Yellow;
  }
}


// ============================================================
//                         BEEP
// ============================================================

void successBeep() {

  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);

  digitalWrite(BUZZER_PIN, LOW);
  delay(100);

  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);

  digitalWrite(BUZZER_PIN, LOW);
}


void errorBeep() {

  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);

  digitalWrite(BUZZER_PIN, LOW);
}


// ============================================================
//                   IR REMOTE MAPPING
// ============================================================

int8_t getActionFromIR(uint8_t command) {

  switch (command) {

    case 69:
      return 1;

    case 70:
      return 2;

    case 71:
      return 3;

    case 68:
      return 4;

    case 24:
      return 5;

    case 82:
      return 6;

    case 25:
      return 7;

    case 28:
      return 8;

    default:
      return -1;
  }
}