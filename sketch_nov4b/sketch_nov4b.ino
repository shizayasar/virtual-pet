#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Touchscreen pins on the CYD
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TFT_eSPI tft;

const uint16_t BG = TFT_SKYBLUE;

// Pet stats: 100 = full, 0 = empty
int hunger = 80;
int fun = 80;
int energy = 80;
bool sleeping = false;

// Stats drop every 5 seconds (short for testing, make it longer later)
const unsigned long TICK_MS = 5000;
unsigned long lastTick = 0;

// Blinking
unsigned long nextBlink = 3000;
unsigned long blinkUntil = 0;

// Touch
bool wasTouched = false;

struct Button { int x, y, w, h; const char* label; uint16_t color; };
Button buttons[3] = {
  {8,   192, 96, 40, "Feed",  TFT_ORANGE},
  {112, 192, 96, 40, "Play",  TFT_DARKGREEN},
  {216, 192, 96, 40, "Sleep", TFT_NAVY}
};

enum Mood { HAPPY, OKAY, HUNGRY, TIRED, SAD, ASLEEP };
int shownMood = -1;
bool shownBlink = false;

Mood getMood() {
  if (sleeping)    return ASLEEP;
  if (hunger < 25) return HUNGRY;
  if (energy < 25) return TIRED;
  if (fun < 25)    return SAD;
  if (fun > 60)    return HAPPY;
  return OKAY;
}

// ---------- Drawing ----------

void drawBar(int y, const char* label, int value, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_BLACK, BG);
  tft.drawString(label, 8, y, 2);
  tft.drawRect(90, y + 2, 202, 12, TFT_BLACK);
  tft.fillRect(91, y + 3, value * 2, 10, color);
  tft.fillRect(91 + value * 2, y + 3, 200 - value * 2, 10, TFT_WHITE);
}

void drawStats() {
  drawBar(6,  "Hunger", hunger, TFT_ORANGE);
  drawBar(24, "Fun",    fun,    TFT_GREEN);
  drawBar(42, "Energy", energy, TFT_BLUE);
}

void drawButtons() {
  buttons[2].label = sleeping ? "Wake" : "Sleep";
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 3; i++) {
    Button &b = buttons[i];
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, b.color);
    tft.setTextColor(TFT_WHITE, b.color);
    tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2, 2);
  }
  tft.setTextDatum(TL_DATUM);
}

void drawPet(Mood mood, bool blink) {
  tft.fillRect(100, 66, 140, 118, BG);       // clear the pet area
  tft.fillCircle(160, 120, 45, TFT_PINK);    // body

  // Eyes
  if (mood == ASLEEP || blink) {
    tft.fillRect(136, 107, 14, 3, TFT_BLACK);
    tft.fillRect(170, 107, 14, 3, TFT_BLACK);
  } else if (mood == TIRED) {
    tft.fillRect(137, 106, 12, 5, TFT_BLACK);
    tft.fillRect(171, 106, 12, 5, TFT_BLACK);
  } else {
    tft.fillCircle(143, 108, 6, TFT_BLACK);
    tft.fillCircle(177, 108, 6, TFT_BLACK);
  }

  // Mouth
  switch (mood) {
    case HAPPY:   // smile
      tft.fillCircle(160, 130, 14, TFT_BLACK);
      tft.fillCircle(160, 124, 14, TFT_PINK);
      break;
    case SAD:     // frown
      tft.fillCircle(160, 146, 12, TFT_BLACK);
      tft.fillCircle(160, 151, 12, TFT_PINK);
      break;
    case HUNGRY:  // open mouth
      tft.fillCircle(160, 138, 8, TFT_BLACK);
      break;
    default:      // flat mouth
      tft.fillRect(150, 138, 20, 3, TFT_BLACK);
      break;
  }

  if (mood == ASLEEP) {
    tft.setTextColor(TFT_BLACK, BG);
    tft.drawString("Zz", 205, 70, 4);
  }
}

// ---------- Game logic ----------

void clampStats() {
  hunger = constrain(hunger, 0, 100);
  fun    = constrain(fun, 0, 100);
  energy = constrain(energy, 0, 100);
}

void pressButton(int i) {
  if (i == 2) {                  // Sleep / Wake
    sleeping = !sleeping;
    drawButtons();
  } else if (sleeping) {
    return;                      // can't feed or play while asleep
  } else if (i == 0) {           // Feed
    hunger += 20;
  } else if (i == 1) {           // Play
    fun += 15;
    energy -= 10;
    hunger -= 5;
  }
  clampStats();
  drawStats();
}

void tick() {
  if (sleeping) {
    energy += 5;
    hunger -= 1;
    if (energy >= 100) {         // wakes up when fully rested
      sleeping = false;
      drawButtons();
    }
  } else {
    hunger -= 2;
    fun    -= 1;
    energy -= 1;
  }
  clampStats();
  drawStats();
}

void handleTouch() {
  bool touched = ts.tirqTouched() && ts.touched();
  if (touched && !wasTouched) {          // only react to a new tap
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3700, 0, 320);
    int y = map(p.y, 240, 3800, 0, 240);
    for (int i = 0; i < 3; i++) {
      Button &b = buttons[i];
      if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
        pressButton(i);
      }
    }
  }
  wasTouched = touched;
}

// ---------- Setup and loop ----------

void setup() {
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG);

  randomSeed(analogRead(34));
  drawStats();
  drawButtons();
}

void loop() {
  unsigned long now = millis();

  if (now - lastTick >= TICK_MS) {
    lastTick = now;
    tick();
  }

  if (!sleeping && now >= nextBlink) {
    blinkUntil = now + 150;
    nextBlink = now + random(2500, 5000);
  }

  Mood mood = getMood();
  bool blinking = !sleeping && now < blinkUntil;
  if (mood != shownMood || blinking != shownBlink) {
    drawPet(mood, blinking);
    shownMood = mood;
    shownBlink = blinking;
  }

  handleTouch();
}