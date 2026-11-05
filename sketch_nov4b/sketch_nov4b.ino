#include <TFT_eSPI.h>
TFT_eSPI tft;

void drawPet(bool eyesOpen) {
  tft.fillCircle(160, 130, 60, TFT_PINK);            // body
  if (eyesOpen) {
    tft.fillCircle(140, 120, 8, TFT_BLACK);
    tft.fillCircle(180, 120, 8, TFT_BLACK);
  } else {
    tft.fillRect(132, 119, 16, 3, TFT_BLACK);
    tft.fillRect(172, 119, 16, 3, TFT_BLACK);
  }
  tft.fillCircle(160, 150, 6, TFT_RED);              // mouth
}

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_SKYBLUE);
  tft.setTextColor(TFT_BLACK);
  tft.drawString("Hello!", 130, 210, 4);
}

void loop() {
  drawPet(true);
  delay(3000);
  drawPet(false);
  delay(150);
}