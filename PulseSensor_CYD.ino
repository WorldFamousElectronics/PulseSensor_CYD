/*
 * PulseSensor on Cheap Yellow Display (CYD)
 * ESP32-2432S028R
 * 
 * FLICKER-FREE scrolling waveform + RGB LED heartbeat
 */

#include <TFT_eSPI.h>
#include <PulseSensorPlayground.h>

// ============== PIN DEFINITIONS ==============
#define PULSE_SENSOR_PIN  35
#define BACKLIGHT_PIN     21

// RGB LED pins (directly on CYD board)
#define LED_RED_PIN       4
#define LED_GREEN_PIN     16
#define LED_BLUE_PIN      17

// ============== DISPLAY SETUP ==============
TFT_eSPI tft = TFT_eSPI();

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ============== LAYOUT ==============
#define BPM_X             10
#define BPM_Y             5
#define BEAT_X            270
#define BEAT_Y            35

#define WAVEFORM_TOP      80
#define WAVEFORM_HEIGHT   100
#define WAVEFORM_BOTTOM   (WAVEFORM_TOP + WAVEFORM_HEIGHT)

#define BOTTOM_Y          190
#define SIGNAL_X          10
#define IBI_X             170

// ============== PULSESENSOR.COM BRAND COLORS ==============
#define COLOR_BG          0x18C3
#define COLOR_WAVEFORM    0xFE60
#define COLOR_BPM         0x4F10
#define COLOR_SIGNAL      0xFE60
#define COLOR_IBI         0xE1E9
#define COLOR_HEART       0xE1E9
#define COLOR_HEART_DIM   0x4208
#define COLOR_LABEL       0x6B6D
#define COLOR_GRID        0x3A0A
#define COLOR_BORDER      0x4A69

// ============== BRANDING SETTINGS ==============
#define BRAND_TEXT        "PulseSensor.com"
#define BRAND_COLOR       0xFFFF    // White (change to any RGB565 color)
#define BRAND_SIZE        1         // 1=small, 2=medium, 3=large
#define BRAND_X           110       // Between BPM and heart
#define BRAND_Y           2         // Very top edge

// ============== WAVEFORM SETTINGS ==============
#define WAVEFORM_THICKNESS  4
#define WAVEFORM_SPEED      5

// ============== LED SETTINGS ==============
#define LED_FADE_SPEED      4     // How fast LED fades (higher = faster)
#define LED_MAX_BRIGHTNESS  255   // 0-255

// ============== PULSESENSOR ==============
PulseSensorPlayground pulseSensor;
const int THRESHOLD = 550;

// ============== NO-FINGER TIMEOUT ==============
#define NO_BEAT_TIMEOUT   3000

// ============== WAVEFORM BUFFER ==============
#define WAVEFORM_SAMPLES  SCREEN_WIDTH
int waveformBuffer[WAVEFORM_SAMPLES];
int waveformX = 0;
int minSignal = 4095;
int maxSignal = 0;

// ============== TIMING ==============
unsigned long lastDrawTime = 0;
unsigned long lastBeatTime = 0;
unsigned long lastLedUpdate = 0;
bool beatFlashActive = false;
bool fingerPresent = false;

// ============== LED STATE ==============
int ledBrightness = 0;  // Current brightness (0 = off, 255 = full)

// ============== CURRENT VALUES ==============
int currentBPM = 0;
int currentSignal = 0;
int currentIBI = 0;
int lastDisplayedBPM = -1;
int lastDisplayedSignal = -1;
int lastDisplayedIBI = -1;

// ============== FUNCTION DECLARATIONS ==============
void drawLabels();
void drawBPM(int bpm);
void drawSignal(int signal);
void drawIBI(int ibi);
void drawBeatIndicator(bool active);
void drawHeart(uint16_t color);
void drawWaveformColumn();
void updateMinMax();
void checkFingerTimeout();
void setupLED();
void flashLED();
void updateLED();
void setLED(int brightness);


void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== PulseSensor CYD Dashboard ===");
  
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  
  // Setup RGB LED
  setupLED();
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  
  drawLabels();
  tft.fillRect(1, WAVEFORM_TOP, SCREEN_WIDTH - 2, WAVEFORM_HEIGHT, COLOR_BG);
  
  pulseSensor.analogInput(PULSE_SENSOR_PIN);
  pulseSensor.setThreshold(THRESHOLD);
  
  if (pulseSensor.begin()) {
    Serial.println("PulseSensor initialized!");
  } else {
    Serial.println("PulseSensor FAILED!");
  }
  
  for (int i = 0; i < WAVEFORM_SAMPLES; i++) {
    waveformBuffer[i] = 2048;
  }
  
  drawBPM(0);
  drawSignal(0);
  drawIBI(0);
  drawBeatIndicator(false);
}

void loop() {
  unsigned long now = millis();
  
  currentSignal = pulseSensor.getLatestSample();
  
  if (pulseSensor.sawStartOfBeat()) {
    currentBPM = pulseSensor.getBeatsPerMinute();
    currentIBI = pulseSensor.getInterBeatIntervalMs();
    lastBeatTime = now;
    beatFlashActive = true;
    fingerPresent = true;
    
    // Flash the LED!
    flashLED();
    
    if (currentBPM != lastDisplayedBPM) {
      drawBPM(currentBPM);
      lastDisplayedBPM = currentBPM;
    }
    if (currentIBI != lastDisplayedIBI) {
      drawIBI(currentIBI);
      lastDisplayedIBI = currentIBI;
    }
    drawBeatIndicator(true);
    
    Serial.printf("%d,%d,%d,1\n", currentSignal, currentBPM, currentIBI);
  }
  
  if (beatFlashActive && (now - lastBeatTime > 150)) {
    beatFlashActive = false;
    drawBeatIndicator(false);
  }
  
  // Update LED fade
  updateLED();
  
  checkFingerTimeout();
  
  if (now - lastDrawTime >= WAVEFORM_SPEED) {
    lastDrawTime = now;
    
    waveformBuffer[waveformX] = currentSignal;
    updateMinMax();
    drawWaveformColumn();
    
    waveformX++;
    if (waveformX >= SCREEN_WIDTH) {
      waveformX = 0;
    }
    
    if (waveformX % 10 == 0) {
      if (abs(currentSignal - lastDisplayedSignal) > 30) {
        drawSignal(currentSignal);
        lastDisplayedSignal = currentSignal;
      }
    }
  }
}

// ============== RGB LED FUNCTIONS ==============

void setupLED() {
  // New ESP32 Arduino Core 3.x API
  ledcAttach(LED_RED_PIN, 5000, 8);    // Pin, frequency, resolution
  ledcAttach(LED_GREEN_PIN, 5000, 8);
  ledcAttach(LED_BLUE_PIN, 5000, 8);
  
  // Start with LED off
  setLED(0);
}

void flashLED() {
  ledBrightness = LED_MAX_BRIGHTNESS;
  setLED(ledBrightness);
}

void updateLED() {
  unsigned long now = millis();
  
  if (now - lastLedUpdate >= 10) {
    lastLedUpdate = now;
    
    if (ledBrightness > 0) {
      ledBrightness -= LED_FADE_SPEED;
      if (ledBrightness < 0) ledBrightness = 0;
      setLED(ledBrightness);
    }
  }
}

void setLED(int brightness) {
  // Active LOW: 255 = off, 0 = full bright
  int pwmValue = 255 - brightness;
  
  // Red only for heartbeat
  ledcWrite(LED_RED_PIN, pwmValue);    // Red ON
  ledcWrite(LED_GREEN_PIN, 255);       // Green OFF
  ledcWrite(LED_BLUE_PIN, 255);        // Blue OFF
}
// ============== NO-FINGER TIMEOUT ==============
void checkFingerTimeout() {
  unsigned long now = millis();
  
  if (fingerPresent && (now - lastBeatTime > NO_BEAT_TIMEOUT)) {
    fingerPresent = false;
    currentBPM = 0;
    currentIBI = 0;
    
    if (lastDisplayedBPM != 0) {
      drawBPM(0);
      lastDisplayedBPM = 0;
    }
    if (lastDisplayedIBI != 0) {
      drawIBI(0);
      lastDisplayedIBI = 0;
    }
    
    Serial.println("No finger detected - values reset");
  }
}

// ============== DRAWING FUNCTIONS ==============

void drawLabels() {
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setTextSize(2);
  
  tft.setCursor(BPM_X, BPM_Y);
  tft.print("BPM");
  
  tft.setCursor(SIGNAL_X, BOTTOM_Y);
  tft.print("Signal");
  
  tft.setCursor(IBI_X, BOTTOM_Y);
  tft.print("IBI");
  
  tft.drawRect(0, WAVEFORM_TOP - 1, SCREEN_WIDTH, WAVEFORM_HEIGHT + 2, COLOR_BORDER);
  
  // PulseSensor.com branding - top center
  tft.setTextColor(BRAND_COLOR, COLOR_BG);
  tft.setTextSize(BRAND_SIZE);
  tft.setCursor(BRAND_X, BRAND_Y);
  tft.print(BRAND_TEXT);
}

void drawBPM(int bpm) {
  tft.fillRect(BPM_X, BPM_Y + 20, 150, 55, COLOR_BG);
  
  tft.setTextSize(7);
  if (bpm > 30 && bpm < 220) {
    tft.setTextColor(COLOR_BPM, COLOR_BG);
    tft.setCursor(BPM_X, BPM_Y + 20);
    tft.print(bpm);
  } else {
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(BPM_X, BPM_Y + 20);
    tft.print("--");
  }
}

void drawSignal(int signal) {
  tft.fillRect(SIGNAL_X, BOTTOM_Y + 18, 120, 35, COLOR_BG);
  
  tft.setTextSize(4);
  tft.setTextColor(COLOR_SIGNAL, COLOR_BG);
  tft.setCursor(SIGNAL_X, BOTTOM_Y + 20);
  tft.print(signal);
}

void drawIBI(int ibi) {
  tft.fillRect(IBI_X, BOTTOM_Y + 18, 140, 35, COLOR_BG);
  
  tft.setTextSize(4);
  tft.setTextColor(COLOR_IBI, COLOR_BG);
  tft.setCursor(IBI_X, BOTTOM_Y + 20);
  if (ibi > 0) {
    tft.print(ibi);
    tft.setTextSize(2);
    tft.print("ms");
  } else {
    tft.print("--");
  }
}

void drawBeatIndicator(bool active) {
  tft.fillRect(BEAT_X - 30, BEAT_Y - 30, 65, 65, COLOR_BG);
  
  if (active) {
    drawHeart(COLOR_HEART);
  } else {
    drawHeart(COLOR_HEART_DIM);
  }
}

void drawHeart(uint16_t color) {
  tft.fillCircle(BEAT_X - 12, BEAT_Y - 8, 14, color);
  tft.fillCircle(BEAT_X + 12, BEAT_Y - 8, 14, color);
  tft.fillTriangle(BEAT_X - 26, BEAT_Y - 2, BEAT_X + 26, BEAT_Y - 2, BEAT_X, BEAT_Y + 28, color);
}

void drawWaveformColumn() {
  int x = waveformX;
  
  tft.drawFastVLine(x, WAVEFORM_TOP + 1, WAVEFORM_HEIGHT - 2, COLOR_BG);
  
  int cursorX = (x + 4) % SCREEN_WIDTH;
  tft.drawFastVLine(cursorX, WAVEFORM_TOP + 1, WAVEFORM_HEIGHT - 2, COLOR_GRID);
  
  int range = max(maxSignal - minSignal, 100);
  float scale = (float)(WAVEFORM_HEIGHT - 10) / range;
  
  int y = WAVEFORM_BOTTOM - 5 - (int)((waveformBuffer[x] - minSignal) * scale);
  y = constrain(y, WAVEFORM_TOP + 2, WAVEFORM_BOTTOM - 2);
  
  for (int t = 0; t < WAVEFORM_THICKNESS; t++) {
    tft.drawPixel(x, y + t, COLOR_WAVEFORM);
    tft.drawPixel(x, y - t, COLOR_WAVEFORM);
  }
  
  if (x > 0) {
    int prevY = WAVEFORM_BOTTOM - 5 - (int)((waveformBuffer[x - 1] - minSignal) * scale);
    prevY = constrain(prevY, WAVEFORM_TOP + 2, WAVEFORM_BOTTOM - 2);
    
    for (int t = 0; t < WAVEFORM_THICKNESS; t++) {
      tft.drawLine(x - 1, prevY + t, x, y + t, COLOR_WAVEFORM);
      tft.drawLine(x - 1, prevY - t, x, y - t, COLOR_WAVEFORM);
    }
  }
}

void updateMinMax() {
  static unsigned long lastDecay = 0;
  unsigned long now = millis();
  
  if (now - lastDecay > 100) {
    lastDecay = now;
    minSignal = min(minSignal + 5, 2048);
    maxSignal = max(maxSignal - 5, 2048);
  }
  
  minSignal = min(minSignal, currentSignal);
  maxSignal = max(maxSignal, currentSignal);
}
