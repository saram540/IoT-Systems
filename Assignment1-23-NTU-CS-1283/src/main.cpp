// ============================================================================
// Project Title : ESP32 Smart LED Controller with OLED Display & Buzzer
// Name          : SARAM ZAFAR 23-NTU-CS-1283
// Date          : 2024-06-15
// ============================================================================
// Project Title : ESP32 Smart LED Controller with OLED Display & Buzzer
// Modified Version — Variable Names Updated (Functionality Same)
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ----------------------- Pin Mapping -----------------------
#define BUZZER_PIN      27
#define BTN_MODE        13
#define BTN_ACTION      12
#define BTN_BOOT        0

#define LED_R            19
#define LED_G            18
#define LED_Y            5

// ----------------------- PWM Configuration -----------------
#define PWM_BITS         8
#define LED_FREQ         5000
#define CH_BUZZER        0
#define CH_R             1
#define CH_G             2
#define CH_Y             3

// ----------------------- OLED Setup ------------------------
#define OLED_W           128
#define OLED_H           64
#define OLED_I2C_ADDR    0x3C
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// ----------------------- Mode Management -------------------
enum LedEffectMode {
  EFFECT_OFF = 0,
  EFFECT_ALTERNATE,
  EFFECT_ALL_ON,
  EFFECT_FADE,
  EFFECT_TOTAL
};

LedEffectMode currentEffect = EFFECT_OFF;

// ----------------------- Timing Variables ------------------
unsigned long blinkTime = 0;
const unsigned long BLINK_GAP = 400;
bool blinkFlag = false;

unsigned long fadeTime = 0;
unsigned long actionPressStart = 0;
bool actionPressed = false;
bool longPressDone = false;

unsigned long lastModeTime = 0, lastBootTime = 0;
const unsigned long DEBOUNCE_MS = 50;

bool manualMode = false;
bool ledManualState = false;

// ============================================================================
// OLED Utility — Show 2-line message
// ============================================================================
void showDisplay(const char* title, const char* info) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(title);

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(info);
  display.display();
}

// ============================================================================
// Change LED Mode
// ============================================================================
void setMode(LedEffectMode mode) {
  currentEffect = mode;
  manualMode = false;

  switch (mode) {
    case EFFECT_OFF:
      showDisplay("Mode:", "OFF");
      ledcWrite(CH_R, 0);
      ledcWrite(CH_G, 0);
      ledcWrite(CH_Y, 0);
      break;

    case EFFECT_ALTERNATE:
      showDisplay("Mode:", "Alternate");
      blinkTime = millis();
      blinkFlag = false;
      break;

    case EFFECT_ALL_ON:
      showDisplay("Mode:", "All ON");
      ledcWrite(CH_R, 255);
      ledcWrite(CH_G, 255);
      ledcWrite(CH_Y, 255);
      break;

    case EFFECT_FADE:
      showDisplay("Mode:", "Fade");
      fadeTime = millis();
      break;
  }
}

// ============================================================================
// Buzzer Utility
// ============================================================================
void beepTone(unsigned int freq, unsigned long time) {
  ledcWriteTone(CH_BUZZER, freq);
  delay(time);
  ledcWriteTone(CH_BUZZER, 0);
}

// ============================================================================
// SETUP — Initialization
// ============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_ACTION, INPUT_PULLUP);
  pinMode(BTN_BOOT, INPUT_PULLUP);

  ledcSetup(CH_BUZZER, 2000, PWM_BITS);
  ledcAttachPin(BUZZER_PIN, CH_BUZZER);

  ledcSetup(CH_R, LED_FREQ, PWM_BITS);
  ledcAttachPin(LED_R, CH_R);

  ledcSetup(CH_G, LED_FREQ, PWM_BITS);
  ledcAttachPin(LED_G, CH_G);

  ledcSetup(CH_Y, LED_FREQ, PWM_BITS);
  ledcAttachPin(LED_Y, CH_Y);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    while (true) delay(100);
  }

  showDisplay("System:", "Ready");
  setMode(EFFECT_OFF);
}

// ============================================================================
// LOOP — Main Logic
// ============================================================================
void loop() {
  unsigned long now = millis();

  // ---------- MODE BUTTON ----------
  if (digitalRead(BTN_MODE) == LOW) {
    if (now - lastModeTime > DEBOUNCE_MS) {
      currentEffect = (LedEffectMode)((currentEffect + 1) % EFFECT_TOTAL);
      setMode(currentEffect);
      while (digitalRead(BTN_MODE) == LOW) delay(10);
      lastModeTime = millis();
    }
  }

  // ---------- BOOT BUTTON ----------
  if (digitalRead(BTN_BOOT) == LOW) {
    if (now - lastBootTime > DEBOUNCE_MS) {
      setMode(EFFECT_OFF);
      showDisplay("System:", "Reset");
      while (digitalRead(BTN_BOOT) == LOW) delay(10);
      lastBootTime = millis();
    }
  }

  // ---------- ACTION BUTTON ----------
  int actionState = digitalRead(BTN_ACTION);

  if (actionState == LOW && !actionPressed) {
    actionPressed = true;
    actionPressStart = now;
    longPressDone = false;
  }

  if (actionState == LOW && actionPressed && !longPressDone) {
    if (now - actionPressStart >= 1500) {
      showDisplay("Action:", "Long Press");
      beepTone(2500, 300);
      longPressDone = true;
    }
  }

  if (actionState == HIGH && actionPressed) {
    unsigned long pressTime = now - actionPressStart;
    actionPressed = false;

    if (!longPressDone && pressTime < 1500) {
      manualMode = true;
      ledManualState = !ledManualState;

      if (ledManualState) {
        ledcWrite(CH_R, 255);
        ledcWrite(CH_G, 255);
        ledcWrite(CH_Y, 255);
        showDisplay("Action:", "Short: ON");
      } else {
        ledcWrite(CH_R, 0);
        ledcWrite(CH_G, 0);
        ledcWrite(CH_Y, 0);
        showDisplay("Action:", "Short: OFF");
      }
    }
  }

  // ---------- LED BEHAVIOR ----------
  if (!manualMode) {
    switch (currentEffect) {
      case EFFECT_ALTERNATE:
        if (now - blinkTime >= BLINK_GAP) {
          blinkTime = now;
          blinkFlag = !blinkFlag;
          static int ledIndex = 0;
          if (blinkFlag) {
            ledIndex = (ledIndex + 1) % 3;
            ledcWrite(CH_R, (ledIndex == 0) ? 255 : 0);
            ledcWrite(CH_G, (ledIndex == 1) ? 255 : 0);
            ledcWrite(CH_Y, (ledIndex == 2) ? 255 : 0);
          } else {
            ledcWrite(CH_R, 0);
            ledcWrite(CH_G, 0);
            ledcWrite(CH_Y, 0);
          }
        }
        break;

      case EFFECT_FADE: {
        unsigned long elapsed = (now - fadeTime) % 2000;
        float t = elapsed / 2000.0f;
        float rVal = (sin(2 * PI * t) + 1.0f) / 2.0f;
        float gVal = (sin(2 * PI * (t + 1.0 / 3.0)) + 1.0f) / 2.0f;
        float yVal = (sin(2 * PI * (t + 2.0 / 3.0)) + 1.0f) / 2.0f;

        ledcWrite(CH_R, (int)(rVal * 255));
        ledcWrite(CH_G, (int)(gVal * 255));
        ledcWrite(CH_Y, (int)(yVal * 255));
        break;
      }

      default:
        break;
    }
  }

  delay(8);
}
}