#include <SD.h>
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "config.h"

#define BUTTON_PIN 0

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

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, handleButton, FALLING);

  out = new AudioOutputI2S();
  out->SetPinout(I2S_SCLK, I2S_LRCLK, I2S_DOUT);
  mp3 = new AudioGeneratorMP3();
  aFile = new AudioFileSourceFS(SD);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 80000000)) { 
    Serial.println("SD卡挂载失败");
  } else {
    File root = SD.open("/");
    noFiles = countMP3Files(root);
    root.close();
    Serial.printf("找到%d个MP3文件\n", noFiles);
  }
}

bool isMP3File(File &entry) {
  String name = entry.name();
  return name.endsWith(".mp3") && !entry.isDirectory();
}

int countMP3Files(File dir) {
  int count = 0;
  dir.rewindDirectory(); // 重置目录指针
  while (File entry = dir.openNextFile()) {
    if (isMP3File(entry)) count++;
    entry.close();
  }
  return count;
}

String getMP3Filename(int targetNo) {
  int counter = 0;
  File dir = SD.open("/");
  dir.rewindDirectory();
  while (File entry = dir.openNextFile()) {
    if (isMP3File(entry)) {
      if (++counter == targetNo) {
        String filename = "/" + String(entry.name()); // 添加根目录前缀
        entry.close();
        dir.close();
        return filename;
      }
    }
    entry.close();
  }
  dir.close();
  return "";
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

void loop() {
  if (buttonPressed) {
    fileNo = (fileNo % noFiles) + 1;
    buttonPressed = false;
    Serial.printf("切换到文件: %d\n", fileNo);
  }

  String audioFilename = getMP3Filename(fileNo);
  if (!audioFilename.isEmpty()) {
    playAudio(audioFilename);
    while (mp3->isRunning() && !buttonPressed) {
      if (!mp3->loop()) mp3->stop();
      delay(1);
    }
  } else {
    Serial.println("未找到有效文件");
    delay(1000);
  }
}