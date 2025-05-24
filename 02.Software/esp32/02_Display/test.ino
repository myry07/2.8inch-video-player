#include "config.h"
#include <Arduino_GFX_Library.h>
Arduino_DataBus *bus = new Arduino_ESP32SPI(DC, CS, SCK, MOSI, MISO, VSPI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, RST, 1, true, 320, 240, 0, 0, 0, 0);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // Init Display
  gfx->begin(80000000);
  gfx->fillScreen(RED);
  pinMode(BLK, OUTPUT);
  digitalWrite(BLK, HIGH);

  gfx->setCursor(10, 10);
  gfx->setTextColor(BLUE);
  gfx->setTextSize(2, 2, 0);
  gfx->setRotation(3);

  gfx->println("Hello World!");

  Serial.println("display done");
}

void loop() {
  // put your main code here, to run repeatedly:
}

