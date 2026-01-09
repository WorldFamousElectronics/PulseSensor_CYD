/*
 * PulseSensor on Cheap Yellow Display (CYD)
 * ESP32-2432S028R
 * 
 * Standalone heartbeat monitor with:
 * - Large BPM display
 * - Smooth scrolling waveform
 * - IBI (inter-beat interval)
 * - RGB LED heartbeat indicator
 * - Auto-scaling signal
 * - No-finger timeout
 * 
 * Hardware Setup:
 *   PulseSensor Signal → GPIO 35 (P3 connector)
 *   PulseSensor VCC    → 3.3V (CN1 connector)
 *   PulseSensor GND    → GND (P3 or CN1)
 * 
 * Libraries Required:
 *   - TFT_eSPI (configured for CYD)
 *   - PulseSensorPlayground
 * 
 * Board: ESP32 Dev Module
 * 
 * More info: https://pulsesensor.com/pages/cyd
 * GitHub: https://github.com/WorldFamousElectronics/PulseSensor_CYD
 * 
 * License: MIT
 * (c) 2026 World Famous Electronics LLC
 */

#include <TFT_eSPI.h>
#include <PulseSensorPlayground.h>

// ============== PIN DEFINITIONS ==============
#define PULSE_SENSOR_PIN  35   // GPIO35 on P3 connector (ADC, input only)
#define BACKLIGHT_PIN     21   // Display backlight
#define RGB_LED_R         4    // Onboard RGB LED - Red
#define RGB_LED_G         16   // Onboard RGB LED - Green
#define RGB_LED_B         17   // Onboard RGB LED - Blue

// ============== DISPLAY SETUP ==============
TFT_eSPI tft = TFT_eSPI();

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ============== LAYOUT ==============
#define BPM_X             10
#define BPM_Y             5
#define BEAT_X            270
#define BEAT_Y            35
#define BEAT_R            25

#define WAVEFORM_TOP      80
#define WAVEFORM_HEIGHT   100
#define WAVEFORM_BOTTOM   (WAVEFORM_TOP + WAVEFORM_HEIGHT)

#define BOTTOM_Y          190
#define SIGNAL_X          10
#define IBI_X             170

// ============== COLORS (Customizable) ==============
#define COLOR_BG          0x18C3    // Dark charcoal
#define COLOR_WAVEFORM    0xE1E9    // Salmon red
#define COLOR_BPM         0x4F10    // Green
#define COLOR_SIGNAL      0xFE60    // Yellow  
#define COLOR_IBI         0x269E    // Cyan
#define COLOR_HEART       0xEA29    // Red
#define COLOR_HEART_DIM   0x4208    // Dark gray (inactive)
#define COLOR_LABEL       0x6B6D    // Gray
#define COLOR_GRID        0x3A0A    // Dark gray

// ============== WAVEFORM SETTINGS ==============
#define WAVEFORM_THICKNESS  3
#define WAVEFORM_SPEED      20  // ms between draws

// ============== PULSESENSOR ==============
PulseSensorPlayground pulseSensor;
const int THRESHOLD = 550;

// ============== NO-FINGER TIMEOUT ==============
#define NO_BEAT_TIMEOUT   3000

// ============== WAVEFORM BUFFER ==============
#define WAVEFORM_SAMPLES  SCREEN_WIDTH
int waveformBuffer[WAVEFORM_SAMPLES];
int waveformIndex = 0;
int minSignal = 4095;
int maxSignal = 0;

// ============== TIMING ==============
unsigned long lastDrawTime = 0;
unsigned long lastBeatTime = 0;
bool beatFlashActive = false;
bool fingerPresent = false;

// ============== RGB LED ==============
int ledFadeValue = 0;
unsigned long lastLedUpdate = 0;

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
void drawHeart(uint16_t color);
void drawWaveformColumn();
void updateRgbLed();
void checkFingerTimeout();
void resetDisplay();

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== PulseSensor CYD ===");
  Serial.println("pulsesensor.com/pages/cyd");
  
  // Backlight
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  
  // RGB LED
  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  analogWrite(RGB_LED_R, 0);
  analogWrite(RGB_LED_G, 0);
  analogWrite(RGB_LED_B, 0);
  
  // Display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  
  drawLabels();
  drawHeart(COLOR_HEART_DIM);
  
  // PulseSensor
  pulseSensor.analogInput(PULSE_SENSOR_PIN);
  pulseSensor.setThreshold(THRESHOLD);
  
  if (pulseSensor.begin()) {
    Serial.println("PulseSensor ready");
  } else {
    Serial.println("PulseSensor FAILED");
    tft.setTextColor(TFT_RED);
    tft.drawString("Sensor Error!", 100, 120, 4);
  }
  
  // Initialize waveform buffer
  for (int i = 0; i < WAVEFORM_SAMPLES; i++) {
    waveformBuffer[i] = 2048;
  }
  
  lastBeatTime = millis();
}

void loop() {
  // Read sensor
  currentSignal = pulseSensor.getLatestSample();
  
  // Check for beat
  if (pulseSensor.sawStartOfBeat()) {
    currentBPM = pulseSensor.getBeatsPerMinute();
    currentIBI = pulseSensor.getInterBeatIntervalMs();
    lastBeatTime = millis();
    fingerPresent = true;
    beatFlashActive = true;
    ledFadeValue = 255;
    
    // Update displays
    drawBPM(currentBPM);
    drawIBI(currentIBI);
    drawHeart(COLOR_HEART);
    
    Serial.print("BPM: ");
    Serial.print(currentBPM);
    Serial.print(" | IBI: ");
    Serial.println(currentIBI);
  }
  
  // Update waveform at interval
  if (millis() - lastDrawTime >= WAVEFORM_SPEED) {
    lastDrawTime = millis();
    
    // Store sample
    waveformBuffer[waveformIndex] = currentSignal;
    
    // Update min/max for scaling
    if (currentSignal < minSignal) minSignal = currentSignal;
    if (currentSignal > maxSignal) maxSignal = currentSignal;
    
    // Slowly drift min/max toward center
    minSignal += 1;
    maxSignal -= 1;
    if (minSignal > 1800) minSignal = 1800;
    if (maxSignal < 2200) maxSignal = 2200;
    
    // Draw single column (flicker-free)
    drawWaveformColumn();
    
    // Advance index
    waveformIndex = (waveformIndex + 1) % SCREEN_WIDTH;
    
    // Update signal display
    drawSignal(currentSignal);
  }
  
  // Heart fade
  if (beatFlashActive && millis() - lastBeatTime > 100) {
    beatFlashActive = false;
    drawHeart(COLOR_HEART_DIM);
  }
  
  // RGB LED fade
  updateRgbLed();
  
  // Check for no-finger timeout
  checkFingerTimeout();
}

// ============== DRAWING FUNCTIONS ==============

void drawLabels() {
  tft.setTextColor(COLOR_LABEL);
  tft.setTextSize(1);
  tft.drawString("BPM", BPM_X, BPM_Y + 50, 2);
  tft.drawString("Signal", SIGNAL_X, BOTTOM_Y, 2);
  tft.drawString("IBI", IBI_X, BOTTOM_Y, 2);
}

void drawBPM(int bpm) {
  if (bpm == lastDisplayedBPM) return;
  lastDisplayedBPM = bpm;
  
  // Clear old value
  tft.fillRect(BPM_X, BPM_Y, 150, 50, COLOR_BG);
  
  // Draw new value
  tft.setTextColor(COLOR_BPM);
  tft.setTextSize(1);
  tft.drawNumber(bpm, BPM_X, BPM_Y, 7);  // Font 7 = 7-segment, 48px
}

void drawSignal(int signal) {
  if (signal == lastDisplayedSignal) return;
  lastDisplayedSignal = signal;
  
  tft.fillRect(SIGNAL_X, BOTTOM_Y + 16, 80, 24, COLOR_BG);
  tft.setTextColor(COLOR_SIGNAL);
  tft.drawNumber(signal, SIGNAL_X, BOTTOM_Y + 16, 4);
}

void drawIBI(int ibi) {
  if (ibi == lastDisplayedIBI) return;
  lastDisplayedIBI = ibi;
  
  tft.fillRect(IBI_X, BOTTOM_Y + 16, 100, 24, COLOR_BG);
  tft.setTextColor(COLOR_IBI);
  
  char buf[10];
  sprintf(buf, "%dms", ibi);
  tft.drawString(buf, IBI_X, BOTTOM_Y + 16, 4);
}

void drawHeart(uint16_t color) {
  int x = BEAT_X;
  int y = BEAT_Y;
  int r = BEAT_R;
  
  // Simple heart using circles and triangle
  tft.fillCircle(x - r/2, y, r/2, color);
  tft.fillCircle(x + r/2, y, r/2, color);
  tft.fillTriangle(x - r, y, x + r, y, x, y + r, color);
}

void drawWaveformColumn() {
  int x = waveformIndex;
  
  // Clear this column
  tft.drawFastVLine(x, WAVEFORM_TOP, WAVEFORM_HEIGHT, COLOR_BG);
  
  // Get current and previous sample
  int currentSample = waveformBuffer[waveformIndex];
  int prevIndex = (waveformIndex - 1 + WAVEFORM_SAMPLES) % WAVEFORM_SAMPLES;
  int prevSample = waveformBuffer[prevIndex];
  
  // Scale to display
  int range = maxSignal - minSignal;
  if (range < 100) range = 100;
  
  int y1 = map(prevSample, minSignal, maxSignal, WAVEFORM_BOTTOM - 5, WAVEFORM_TOP + 5);
  int y2 = map(currentSample, minSignal, maxSignal, WAVEFORM_BOTTOM - 5, WAVEFORM_TOP + 5);
  
  y1 = constrain(y1, WAVEFORM_TOP, WAVEFORM_BOTTOM);
  y2 = constrain(y2, WAVEFORM_TOP, WAVEFORM_BOTTOM);
  
  // Draw thick line
  for (int t = 0; t < WAVEFORM_THICKNESS; t++) {
    tft.drawLine(x - 1, y1 + t, x, y2 + t, COLOR_WAVEFORM);
  }
  
  // Draw erase cursor (vertical line ahead)
  int eraseX = (x + 10) % SCREEN_WIDTH;
  tft.drawFastVLine(eraseX, WAVEFORM_TOP, WAVEFORM_HEIGHT, COLOR_BG);
}

void updateRgbLed() {
  if (ledFadeValue > 0) {
    if (millis() - lastLedUpdate > 10) {
      lastLedUpdate = millis();
      ledFadeValue -= 8;
      if (ledFadeValue < 0) ledFadeValue = 0;
      analogWrite(RGB_LED_R, ledFadeValue);
    }
  }
}

void checkFingerTimeout() {
  if (fingerPresent && (millis() - lastBeatTime > NO_BEAT_TIMEOUT)) {
    fingerPresent = false;
    resetDisplay();
  }
}

void resetDisplay() {
  // Clear values
  lastDisplayedBPM = -1;
  lastDisplayedSignal = -1;
  lastDisplayedIBI = -1;
  
  // Reset display
  tft.fillRect(BPM_X, BPM_Y, 150, 50, COLOR_BG);
  tft.fillRect(IBI_X, BOTTOM_Y + 16, 100, 24, COLOR_BG);
  
  // Show prompt
  tft.setTextColor(COLOR_LABEL);
  tft.drawString("Place finger", BPM_X, BPM_Y + 10, 2);
  tft.drawString("on sensor", BPM_X, BPM_Y + 28, 2);
  
  drawHeart(COLOR_HEART_DIM);
  
  // Reset scaling
  minSignal = 1800;
  maxSignal = 2200;
  
  Serial.println("Waiting for finger...");
}
