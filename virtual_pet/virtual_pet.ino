#include <SPI.h>
#include <Wire.h>
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
bool sleptFromDark = false;      // true when the dark room put him to sleep

// Light sensor built into the CYD. Higher number = darker room.
#define LIGHT_PIN 34
const int DARK_LEVEL  = 300;     // above this, the room counts as dark
const int LIGHT_LEVEL = 100;     // below this, the room counts as lit again
const unsigned long LIGHT_CHANGE_MS = 3000;   // has to stay dark or lit this long
bool roomDark = false;
unsigned long lightChangeSince = 0;
unsigned long lastLightCheck = 0;

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

enum Mood { HAPPY, OKAY, HUNGRY, TIRED, SAD, ASLEEP, DIZZY };
unsigned long dizzyUntil = 0;   // Danny stays dizzy until this time
int shownMood = -1;
bool shownBlink = false;

Mood getMood() {
  if (sleeping)    return ASLEEP;
  if (millis() < dizzyUntil) return DIZZY;
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
  if (blink && mood != ASLEEP && mood != DIZZY) {
    frame = DANNY_BLINK;
  } else {
    switch (mood) {
      case HAPPY:  frame = DANNY_HAPPY;  break;
      case HUNGRY: frame = DANNY_HUNGRY; break;
      case TIRED:  frame = DANNY_TIRED;  break;
      case SAD:    frame = DANNY_SAD;    break;
      case ASLEEP: frame = DANNY_ASLEEP; break;
      case DIZZY:  frame = DANNY_DIZZY;  break;
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

void setSleeping(bool s) {
  sleeping = s;
  if (!s) sleptFromDark = false;
  drawButtons();
}

void pressButton(int i) {
  if (i == 2) {                  // Sleep / Wake
    setSleeping(!sleeping);
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
    if (energy >= 100 && !sleptFromDark) {   // wakes up when fully rested (unless it's still dark)
      setSleeping(false);
    }
  } else {
    hunger -= 2;
    fun    -= 1;
    energy -= 1;
  }
  clampStats();
  drawStats();
}

// ---------- Shake to play (MPU-6050 accelerometer on CN1) ----------

#define I2C_SDA 22
#define I2C_SCL 27
#define MPU_ADDR 0x68

const long SHAKE_FORCE = 9000;            // how hard a jolt has to be (16384 = 1g)
const long HARD_SHAKE_FORCE = 22000;      // shakes harder than this make Danny dizzy
const unsigned long DIZZY_MS = 3000;      // how long he stays dizzy
const int SHAKES_NEEDED = 3;              // jolts in a row that count as a shake
const unsigned long SHAKE_WINDOW_MS = 800;
const unsigned long SHAKE_COOLDOWN_MS = 1500;

bool mpuFound = false;
int shakeCount = 0;
unsigned long firstShakeAt = 0;
unsigned long lastShakeAt = 0;
unsigned long lastMpuRead = 0;
bool wasJolted = false;
long shakePeak = 0;                       // strongest jolt in the current shake

void setupAccelerometer() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);   // power register
  Wire.write(0);      // wake the MPU-6050 up
  mpuFound = (Wire.endTransmission() == 0);   // if it's unplugged, the rest of the pet still works
}

// Returns how far the total acceleration is from normal gravity
long readJolt() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);   // first acceleration register
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6) return 0;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  float total = sqrt((float)ax * ax + (float)ay * ay + (float)az * az);
  return labs((long)total - 16384);
}

void checkShake(unsigned long now) {
  if (!mpuFound || now - lastMpuRead < 20) return;
  lastMpuRead = now;

  long jolt = readJolt();
  bool jolted = jolt > SHAKE_FORCE;
  if (jolted) shakePeak = max(shakePeak, jolt);
  if (jolted && !wasJolted) {                 // count each jolt once
    if (now - firstShakeAt > SHAKE_WINDOW_MS) {
      shakeCount = 0;
      shakePeak = jolt;
      firstShakeAt = now;
    }
    shakeCount++;
    if (shakeCount >= SHAKES_NEEDED && now - lastShakeAt > SHAKE_COOLDOWN_MS) {
      lastShakeAt = now;
      shakeCount = 0;
      Serial.printf("Shake strength: %ld\n", shakePeak);
      if (sleeping) {
        // shaking doesn't do anything while he's asleep
      } else if (shakePeak > HARD_SHAKE_FORCE) {   // hard shake: dizzy
        dizzyUntil = now + DIZZY_MS;
        fun -= 5;
        energy -= 5;
        clampStats();
        drawStats();
      } else {                                // gentle shake: same as tapping Play
        pressButton(1);
      }
    }
  }
  wasJolted = jolted;
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

// Danny falls asleep when the room gets dark and wakes up when the lights come back on
void checkLight(unsigned long now) {
  if (now - lastLightCheck < 200) return;
  lastLightCheck = now;

  int light = analogRead(LIGHT_PIN);
  bool looksDark = roomDark ? (light > LIGHT_LEVEL) : (light > DARK_LEVEL);
  if (looksDark == roomDark) {   // nothing changed
    lightChangeSince = now;
    return;
  }
  if (now - lightChangeSince < LIGHT_CHANGE_MS) return;   // wait until it's been dark/lit for a bit

  roomDark = looksDark;
  if (roomDark && !sleeping) {
    setSleeping(true);
    sleptFromDark = true;
  } else if (!roomDark && sleeping && sleptFromDark) {
    setSleeping(false);
  }
}

// ---------- Setup and loop ----------

void setup() {
  Serial.begin(115200);   // prints shake strength so the thresholds can be tuned
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG);

  petSprite.createSprite(PET_SIZE, PET_SIZE);
  setupAccelerometer();

  analogSetAttenuation(ADC_0db);  // makes the light sensor more sensitive
  randomSeed(analogRead(LIGHT_PIN));
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

  checkLight(now);
  checkShake(now);
  handleTouch();
}
