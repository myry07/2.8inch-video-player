#define FPS 24
#define MJPEG_BUFFER_SIZE (320 * 240 * 2 / 8)
#define AUDIOASSIGNCORE 1
#define DECODEASSIGNCORE 0
#define DRAWASSIGNCORE 1
#define BUTTON_PIN 0

#include "config.h"
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <Preferences.h>

#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

Preferences preferences;
#define APP_NAME "video_player"
#define K_VIDEO_INDEX "video_index"
#define BASE_PATH "/Videos_320_240/"
#define MP3_FILENAME "/44100.mp3"
#define MJPEG_FILENAME "/320_240.mjpeg"
#define VIDEO_COUNT 11

/* Arduino_GFX */
#include <Arduino_GFX_Library.h>
Arduino_DataBus *bus = new Arduino_ESP32SPI(DC, CS, SCK, MOSI, MISO, VSPI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, RST, 1, true, 240, 320, 0, 0, 0, 0);

/* MJPEG Video */
#include "mjpeg_decode_draw_task.h"

/* Touch */
#include <CST816S.h>
CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);

/* Variables */
static int next_frame = 0;
static unsigned long start_ms, curr_ms, next_frame_ms;
static unsigned int video_idx = 1;

static AudioGeneratorMP3 *mp3;
static AudioFileSourceFS *aFile;
static AudioOutputI2S *out;

// pixel drawing callback
static int drawMCU(JPEGDRAW *pDraw) {
  unsigned long s = millis();
  gfx->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  total_show_video_ms += millis() - s;
  return 1;
}

void printSDCardType() {
  sdcard_type_t cardType = SD.cardType();
  switch (cardType) {
    case CARD_NONE:
      Serial.println("No SD card attached");
      break;
    case CARD_MMC:
      Serial.println("MMC card detected");
      break;
    case CARD_SD:
      Serial.println("SDSC card detected (<= 2GB)");
      break;
    case CARD_SDHC:
      Serial.println("SDHC card detected (> 2GB)");
      break;
    default:
      Serial.println("Unknown card type");
      break;
  }
}

void setup() {
  disableCore0WDT();
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Init Display
  gfx->begin(80000000);
  gfx->fillScreen(WHITE);
  gfx->setRotation(3);

  pinMode(BLK, OUTPUT);
  digitalWrite(BLK, HIGH);

  out = new AudioOutputI2S();
  out->SetPinout(I2S_SCLK, I2S_LRCLK, I2S_DOUT);
  out->SetGain(0.3);
  mp3 = new AudioGeneratorMP3();
  aFile = new AudioFileSourceFS(SD);

  xTaskCreate(touchTask, "touchTask", 2000, NULL, 1, NULL);
  xTaskCreate(buttonTask, "button", 2000, NULL, 1, NULL);

  SPIClass spi = SPIClass(HSPI);
  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spi, 80000000)) {
    Serial.println("ERROR: File system mount failed!");
    printSDCardType();
    return;
  }

  preferences.begin(APP_NAME, false);
  video_idx = preferences.getUInt(K_VIDEO_INDEX, 1);
  Serial.printf("videoIndex: %d\n", video_idx);

  gfx->setCursor(2, 2);
  gfx->setTextColor(BLUE);
  gfx->setTextSize(1, 1, 0);
  gfx->printf("CH %d", video_idx);
  delay(1000);

  xTaskCreatePinnedToCore(mp3Task, "mp3", 1024 * 3, NULL, 3, NULL, 1);
  playVideo(video_idx);
}

void loop() {
}

void touchTask(void *parameter) {
  touch.begin();
  while (1) {
    if (touch.available()) {
      switch (touch.data.gestureID) {
        case SWIPE_UP:
          videoController(-1);
          break;
        case SWIPE_DOWN:
          videoController(1);
          break;
      }
    }
    vTaskDelay(1000);
  }
}

void mp3Task(void *param) {
  String audioFilename = getMP3Filename(video_idx);
  if (!audioFilename.isEmpty()) {
    playAudio(audioFilename);
    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        break;
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  } else {
    Serial.println("MP3 file not found");
    while (1);
  }
  vTaskDelete(NULL);
}

void buttonTask(void *param) {
  while (1) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(100);
      if (digitalRead(BUTTON_PIN) == LOW) {
        videoController(1);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

String getMP3Filename(int video_idx) {
  char filename[40];
  sprintf(filename, "%s%d%s", BASE_PATH, video_idx, MP3_FILENAME);
  return SD.exists(filename) ? String(filename) : "";
}

void playAudio(String filename) {
  if (mp3->isRunning()) mp3->stop();
  if (aFile->open(filename.c_str())) {
    mp3->begin(aFile, out);
  }
}

void playVideo(int channel) {
  video_idx = channel;
  char vFilePath[40];
  sprintf(vFilePath, "%s%d%s", BASE_PATH, video_idx, MJPEG_FILENAME);

  File vFile = SD.open(vFilePath);
  if (!vFile || vFile.isDirectory()) {
    Serial.printf("ERROR opening: %s\n", vFilePath);
    return;
  }

  mjpeg_setup(&vFile, MJPEG_BUFFER_SIZE, drawMCU, false, DECODEASSIGNCORE, DRAWASSIGNCORE);
  start_ms = millis();
  curr_ms = millis();
  next_frame_ms = start_ms + (++next_frame * 1000 / FPS / 2);
  
  while (vFile.available() && mjpeg_read_frame()) {
    total_read_video_ms += millis() - curr_ms;
    curr_ms = millis();

    if (millis() < next_frame_ms) {
      mjpeg_draw_frame();
      total_decode_video_ms += millis() - curr_ms;
    }

    while (millis() < next_frame_ms) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    curr_ms = millis();
    next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
  }
  vFile.close();
  videoController(1);
}

void videoController(int next) {
  video_idx = (video_idx + next - 1 + VIDEO_COUNT) % VIDEO_COUNT + 1;
  preferences.putUInt(K_VIDEO_INDEX, video_idx);
  esp_restart();
}