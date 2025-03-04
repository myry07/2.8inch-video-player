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
#define VIDEO_COUNT 7

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
static int skipped_frames = 0;
static unsigned long start_ms, curr_ms, next_frame_ms;
static unsigned int video_idx = 1;

static AudioGeneratorMP3 *mp3;
static AudioFileSourceFS *aFile;
static AudioOutputI2S *out;

int noFiles = 0;
int fileNo = 1;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;


void IRAM_ATTR handleButton() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    buttonPressed = true;
    lastDebounceTime = millis();
  }
}


// pixel drawing callback
static int drawMCU(JPEGDRAW *pDraw) {
  unsigned long s = millis();
  gfx->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  total_show_video_ms += millis() - s;
  return 1;
} /* drawMCU() */

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

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // 启用上拉电阻

  // preferences.begin(APP_NAME, false);
  // preferences.clear(); // 清除 Preferences 中的所有数据
  // video_idx = 1; // 重置 video_idx 为 1
  // preferences.end();

  // Init Display
  gfx->begin(80000000);
  gfx->fillScreen(WHITE);
  gfx->setRotation(3);

  pinMode(BLK, OUTPUT);
  digitalWrite(BLK, HIGH);

  out = new AudioOutputI2S();
  out->SetPinout(I2S_SCLK, I2S_LRCLK, I2S_DOUT);
  out->SetGain(0.3);  // 设置为 50% 音量
  mp3 = new AudioGeneratorMP3();
  aFile = new AudioFileSourceFS(SD);

  xTaskCreate(
    touchTask,
    "touchTask",
    2000,
    NULL,
    1,
    NULL);

  xTaskCreate(
    buttonTask,
    "button",
    2000,
    NULL,
    1,
    NULL);


  SPIClass spi = SPIClass(HSPI);
  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spi, 80000000)) {
    Serial.println("ERROR: File system mount failed!");
    gfx->println("ERROR: File system mount failed!");
    printSDCardType();  // 打印 SD 卡类型
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
      Serial.printf("x: %d, y: %d\n", touch.data.x, touch.data.y);
      switch (touch.data.gestureID) {
        case SWIPE_LEFT:
          Serial.println("SWIPE_LEFT");
          break;
        case SWIPE_RIGHT:
          Serial.println("SWIPE_RIGHT");
          break;
        case SWIPE_UP:
          Serial.println("SWIPE_UP (RIGHT)");
          videoController(-1);
          break;
        case SWIPE_DOWN:
          Serial.println("SWIPE_DOWN (LEFT)");
          videoController(1);
          break;
        case SINGLE_CLICK:
          Serial.println("SINGLE_CLICK");
          break;
        case DOUBLE_CLICK:
          Serial.println("DOUBLE_CLICK");
          break;
        case LONG_PRESS:
          Serial.println("LONG_PRESS");
          break;
      }
    }
    vTaskDelay(1000);
  }
}

void mp3Task(void *param) {
  String audioFilename = getMP3Filename(video_idx);                 // 使用正确的视频索引
  Serial.printf("获取到的 MP3 路径: %s\n", audioFilename.c_str());  // 打印路径

  if (!audioFilename.isEmpty()) {
    playAudio(audioFilename);
    while (mp3->isRunning() && !buttonPressed) {
      if (!mp3->loop()) {
        mp3->stop();
        break;  // 退出循环
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  } else {
    Serial.println("未找到 MP3 文件，请检查 SD 卡！");
    while (1)
      ;  // 停止执行
  }
  vTaskDelete(NULL);  // 任务完成后删除
}

void buttonTask(void *param) {
  while (1) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(100);
      if (digitalRead(BUTTON_PIN) == LOW) {
        videoController(1);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 稍微延时，避免占用过多 CPU
  }
}

bool isMP3File(File &entry) {
  String name = entry.name();
  return name.endsWith(".mp3") && !entry.isDirectory();
}

int countMP3Files(File dir) {
  int count = 0;
  dir.rewindDirectory();  // 重置目录指针
  while (File entry = dir.openNextFile()) {
    if (isMP3File(entry)) count++;
    entry.close();
  }
  return count;
}

String getMP3Filename(int video_idx) {
  char filename[40];
  sprintf(filename, "%s%d%s", BASE_PATH, video_idx, MP3_FILENAME);  // 生成路径

  if (SD.exists(filename)) {
    Serial.printf("找到 MP3 文件: %s\n", filename);
    return String(filename);
  } else {
    Serial.printf("MP3 文件不存在: %s\n", filename);
    return "";
  }
}

void playAudio(String filename) {
  if (mp3->isRunning()) mp3->stop();

  if (aFile->open(filename.c_str())) {
    Serial.printf("正在播放: %s\n", filename.c_str());
    mp3->begin(aFile, out);
  } else {
    Serial.printf("无法打开文件: %s\n", filename.c_str());
  }
}


void playVideo(int channel) {
  video_idx = channel;  // 更新 video_idx

  char vFilePath[40];
  sprintf(vFilePath, "%s%d%s", BASE_PATH, video_idx, MJPEG_FILENAME);  // 生成 MJPEG 路径

  File vFile = SD.open(vFilePath);
  if (!vFile || vFile.isDirectory()) {
    Serial.printf("ERROR: 无法打开视频文件 %s\n", vFilePath);
    gfx->printf("ERROR: 无法打开视频文件 %s\n", vFilePath);
    return;
  }

  Serial.println("开始播放视频...");
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
      curr_ms = millis();
    } else {
      ++skipped_frames;
      Serial.println("跳过帧");
    }

    while (millis() < next_frame_ms) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    curr_ms = millis();
    next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
  }
  Serial.println("视频播放结束");
  vFile.close();

  videoController(1);  // 自动播放下一个
}

void videoController(int next) {

  video_idx += next;
  if (video_idx <= 0) {
    video_idx = VIDEO_COUNT;
  } else if (video_idx > VIDEO_COUNT) {
    video_idx = 1;
  }
  Serial.printf("video_idx : %d\n", video_idx);
  preferences.putUInt(K_VIDEO_INDEX, video_idx);
  preferences.end();
  esp_restart();
}