#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TFT_eSPI tft;

void setup() {
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Tap anywhere", 10, 10, 2);
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3700, 0, 320);
    int y = map(p.y, 240, 3800, 0, 240);
    tft.fillCircle(x, y, 3, TFT_YELLOW);
  }
}