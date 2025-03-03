#include <SD.h>
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

#include "config.h"


/* MP3 Audio */
static AudioGeneratorMP3 *mp3 = NULL;
static AudioFileSourceFS *aFile = NULL;
static AudioOutputI2S *out = NULL;

void setup() {
  Serial.begin(115200);

  // 初始化 SD 卡
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 80000000)) {
    Serial.println(F("ERROR: SD card mount failed!"));
    return;
  }
  Serial.println(F("SD card mounted successfully!"));

  // 初始化音频输出
  out = new AudioOutputI2S(0, 1, 128);  // I2S 配置
  mp3 = new AudioGeneratorMP3();
  aFile = new AudioFileSourceFS(SD);

  out = new AudioOutputI2S();
  out->SetPinout(I2S_LRCLK, I2S_SCLK, I2S_DOUT);

  // 尝试播放 MP3 文件
  String audioFilename = "/test.mp3";  // 请确保 SD 卡上有该文件
  if (!aFile->open(audioFilename.c_str())) {
    Serial.println(F("Failed to open audio file"));
    return;
  }
  // Serial.println(F("Playing: ") + audioFilename);

  if (!mp3->begin(aFile, out)) {
    Serial.println(F("Failed to start MP3 playback!"));
    return;
  }
}

void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println(F("MP3 Playback finished."));
    }
  }
}