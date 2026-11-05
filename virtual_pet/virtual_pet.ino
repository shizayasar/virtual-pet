#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "danny_sprites.h"

// Touchscreen pins on the CYD
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TFT_eSPI tft;
TFT_eSprite petSprite = TFT_eSprite(&tft);   // off-screen buffer so Danny doesn't flicker

const uint16_t BG = TFT_SKYBLUE;

// Danny is drawn 3x bigger than the 48x48 sprite
const int PET_SCALE = 3;
const int PET_SIZE  = DANNY_W * PET_SCALE;    // 144
const int PET_X     = (320 - PET_SIZE) / 2;   // centered
const int PET_Y     = 28;

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
  {8,   196, 96, 38, "Feed",  TFT_ORANGE},
  {112, 196, 96, 38, "Play",  TFT_DARKGREEN},
  {216, 196, 96, 38, "Sleep", TFT_NAVY}
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

// One compact stat: label on top, bar underneath
void drawBar(int x, const char* label, int value, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_BLACK, BG);
  tft.drawString(label, x, 0, 2);
  int fill = value * 98 / 100;
  tft.drawRect(x, 17, 100, 9, TFT_BLACK);
  tft.fillRect(x + 1, 18, fill, 7, color);
  tft.fillRect(x + 1 + fill, 18, 98 - fill, 7, TFT_WHITE);
}

void drawStats() {
  drawBar(6,   "Hunger", hunger, TFT_ORANGE);
  drawBar(110, "Fun",    fun,    TFT_GREEN);
  drawBar(214, "Energy", energy, TFT_BLUE);
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

// Draws one 48x48 frame of Danny, scaled up, into the sprite buffer and then onto the screen
void drawDanny(DannyFrame frame) {
  const uint16_t* img = dannyFrames[frame];
  petSprite.fillSprite(BG);
  for (int y = 0; y < DANNY_H; y++) {
    for (int x = 0; x < DANNY_W; x++) {
      uint16_t c = pgm_read_word(&img[y * DANNY_W + x]);
      if (c == DANNY_TRANSPARENT) continue;
      petSprite.fillRect(x * PET_SCALE, y * PET_SCALE, PET_SCALE, PET_SCALE, c);
    }
  }
  petSprite.pushSprite(PET_X, PET_Y);
}

void drawPet(Mood mood, bool blink) {
  DannyFrame frame;
  if (blink && mood != ASLEEP) {
    frame = DANNY_BLINK;
  } else {
    switch (mood) {
      case HAPPY:  frame = DANNY_HAPPY;  break;
      case HUNGRY: frame = DANNY_HUNGRY; break;
      case TIRED:  frame = DANNY_TIRED;  break;
      case SAD:    frame = DANNY_SAD;    break;
      case ASLEEP: frame = DANNY_ASLEEP; break;
      default:     frame = DANNY_OKAY;   break;
    }
  }
  drawDanny(frame);

  // "Zz" next to Danny while he sleeps
  tft.fillRect(PET_X + PET_SIZE + 2, PET_Y, 40, 30, BG);
  if (mood == ASLEEP) {
    tft.setTextColor(TFT_BLACK, BG);
    tft.drawString("Zz", PET_X + PET_SIZE + 4, PET_Y + 4, 4);
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

  petSprite.createSprite(PET_SIZE, PET_SIZE);

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
