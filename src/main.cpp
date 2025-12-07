#include <Arduino.h>
#include "Displays/TFT.h"
#include "Displays/Matrix.h"
#include "VideoPlayer.h"
#include "AudioOutput/I2SOutput.h"
#include "AudioOutput/DACOutput.h"
#include "AudioOutput/PDMTimerOutput.h"
#include "AudioOutput/PWMTimerOutput.h"
#include "AudioOutput/PDMOutput.h"
#include "ChannelData/SDCardChannelData.h"
#include "AVIParser/AVIParser.h"
#include "SDCard.h"
#include "Button.h"
#include <Wire.h>


#ifndef USE_DMA
#warning "No DMA - Drawing may be slower"
#endif

VideoPlayer *videoPlayer = NULL;
AudioOutput *audioOutput = NULL;
ChannelData *channelData = NULL;

TFT display;

void randomChannel(bool drawChannel);
int channel = 99999;


void setupTv()
{
  Serial.begin(115200);

  Serial.printf("CPU freq: %dmhz\n", ESP.getCpuFreqMHz());
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  // initialize the TFT
  display.init();

  Serial.println("Using SD Card");
  // power on the SD card
  #ifdef SD_CARD_PWR
  if (SD_CARD_PWR != GPIO_NUM_NC) {
    pinMode(SD_CARD_PWR, OUTPUT);
    digitalWrite(SD_CARD_PWR, SD_CARD_PWR_ON);
  }
  #endif
  #ifdef USE_SDIO
  SDCard *card = new SDCard(SD_CARD_CLK, SD_CARD_CMD, SD_CARD_D0, SD_CARD_D1, SD_CARD_D2, SD_CARD_D3);
  #else
  SDCard *card = new SDCard(SD_CARD_MISO, SD_CARD_MOSI, SD_CARD_CLK, SD_CARD_CS);
  #endif
  // check that the SD Card has mounted properly
  if (!card->isMounted()) {
    Serial.println("Failed to mount SD Card");
    display.drawSDCardFailed();
    // destroy the card object (call the destructor to hopefully reset a malfunctioning card)
    delete card;
    // Set SDcard pins all low
    pinMode(SD_CARD_MISO, OUTPUT); digitalWrite(SD_CARD_MISO, LOW);
    pinMode(SD_CARD_MOSI, OUTPUT); digitalWrite(SD_CARD_MOSI, LOW);
    pinMode(SD_CARD_CLK, OUTPUT); digitalWrite(SD_CARD_CLK, LOW);
    pinMode(SD_CARD_CS, OUTPUT); digitalWrite(SD_CARD_CS, LOW);
    // Short delay, then reset to try again.
    delay(5000);
    esp_restart();
  }
  channelData = new ChannelData(card, "/", "/bumpers");
  // audioSource = new SDCardAudioSource((ChannelData *) channelData);
  // videoSource = new SDCardVideoSource((ChannelData *) channelData);


  #ifdef USE_DAC_AUDIO
  audioOutput = new DACOutput(I2S_NUM_0);
  audioOutput->start(AUDIO_RATE);
  #endif
  #ifdef PDM_GPIO_NUM
  i2s speaker pins
  i2s_pin_config_t i2s_speaker_pins = {
      .bck_io_num = I2S_PIN_NO_CHANGE,
      .ws_io_num = GPIO_NUM_0,
      .data_out_num = PDM_GPIO_NUM,
      .data_in_num = I2S_PIN_NO_CHANGE};
  audioOutput = new PDMOutput(I2S_NUM_0, i2s_speaker_pins);
  audioOutput->start(AUDIO_RATE);
  #endif
  #ifdef PWM_GPIO_NUM
  audioOutput = new PWMTimerOutput(PWM_GPIO_NUM);
  audioOutput->start(AUDIO_RATE);
  #endif
  #ifdef I2S_SPEAKER_SERIAL_CLOCK
  #ifdef SPK_MODE
  pinMode(SPK_MODE, OUTPUT);
  digitalWrite(SPK_MODE, HIGH);
  #endif
  // i2s speaker pins
  i2s_pin_config_t i2s_speaker_pins = {
      .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
      .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
      .data_out_num = I2S_SPEAKER_SERIAL_DATA,
      .data_in_num = I2S_PIN_NO_CHANGE};

  audioOutput = new I2SOutput(I2S_NUM_1, i2s_speaker_pins);
  audioOutput->start(AUDIO_RATE);
  #endif
  videoPlayer = new VideoPlayer(
    channelData,
    // videoSource,
    // audioSource,
    display,
    audioOutput
  );
  videoPlayer->start();

  // display.drawTuningText();
  // get the channel info
  while(!channelData->fetchChannelData()) {
    Serial.println("Failed to fetch channel data");
    delay(1000);
  }

  videoPlayer->playStatic();
  delay(1000);

  randomChannel(true);
  audioOutput->setVolume(currentVolume);

  #ifdef AUDIO_ENABLE_PIN
  // Enable audio (if required)
  // Enabling audio after playback starts prevents a click on start.
  #ifndef AUDIO_ENABLE_VAL
  #define AUDIO_ENABLE_VAL LOW
  #endif
  delay(10);
  pinMode(AUDIO_ENABLE_PIN, OUTPUT);
  Serial.printf("Enabling audio by setting Pin %d to %d\n", AUDIO_ENABLE_PIN, AUDIO_ENABLE_VAL);
  if (audioOutput->getVolume() == 0){
    digitalWrite(AUDIO_ENABLE_PIN, !AUDIO_ENABLE_VAL);
  }
  else{
    digitalWrite(AUDIO_ENABLE_PIN, AUDIO_ENABLE_VAL);
  }
  #endif
}


void setup()
{
  // init buttons and check if we're turned on or off
  buttonInit();
  buttonLoop();

  if (softPowerEnabled){
    // run initialization for audio and video.
    setupTv();
  }
  else{
    // Powered off. Go to sleep instead.
    Serial.begin(115200);
    Serial.println("Power is off. Sleeping...");
    Serial.flush();
    #ifdef SOFT_POWER_SWITCH_PIN
    esp_sleep_enable_ext0_wakeup(SOFT_POWER_SWITCH_PIN, SOFT_POWER_SWITCH_ON_VAL);
    esp_deep_sleep_start();
    #endif
  }
}



void volumeUp() {
  audioOutput->volumeUp();
  delay(500);
  Serial.println("VOLUME_UP");
}

void volumeDown() {
  audioOutput->volumeDown();
  delay(500);
  Serial.println("VOLUME_DOWN");
}

void channelDown() {
  videoPlayer->playStatic();
  delay(500);
  channel--;
  if (channel < 0) {
    channel = channelData->getChannelCount() - 1;
  }
  videoPlayer->setChannel(channel);
  videoPlayer->play();
  Serial.printf("CHANNEL_DOWN %d\n", channel);
}

void channelUp() {
  videoPlayer->playStatic();
  delay(500);
  channel = (channel + 1) % channelData->getChannelCount();
  videoPlayer->setChannel(channel);
  videoPlayer->play();
  Serial.printf("CHANNEL_UP %d\n", channel);
}


void randomChannel(bool drawChannel) {
  channel = channelData->getNextChannel();

  if (drawChannel){
    // Dont draw the negative bumper channel indexes; instead draw the next channel.
    if (channel < 0){
      videoPlayer->drawChannel(channelData->peekNextChannelNum());
    }
    else {
      videoPlayer->drawChannel(channel);
    }
  }

  videoPlayer->setChannel(channel);
  videoPlayer->play();
  Serial.printf("randomChannel: %d\n", channel);
}


// Returns `current` moved toward `target` by at most `step`, without overshooting. (works with positive step values)
int moveToward(int current, int target, int step)
{
    if (current < target) {
        int next = current + step;
        return (next > target) ? target : next;
    } else if (current > target) {
        int next = current - step;
        return (next < target) ? target : next;
    } else {
        return target; // already there
    }
}


// Put the device into deep sleep
void softPowerOff(){
  videoPlayer->stop();
  channelData->getVideoParser()->storePosition();

  // animate crt power off
  display.fillScreen(65535);
  int i = 100;
  while (i > 0){
    int rectHeight = i * VIDEO_HEIGHT / 100;
    int rectStart = (VIDEO_HEIGHT - rectHeight) / 2;
    // chop off top and bottom of rect
    display.fillRect(0, 0, VIDEO_WIDTH, rectStart, 0);
    display.fillRect(0, rectHeight + rectStart, VIDEO_WIDTH, rectStart, 0);
    i /= 2;
    delay(10);
  }
  // chop off the left and right of the white rect
  display.fillRect(0, 0, VIDEO_WIDTH/3, VIDEO_HEIGHT, 0);
  display.fillRect(VIDEO_WIDTH- VIDEO_WIDTH/3, 0, VIDEO_WIDTH/3, VIDEO_HEIGHT, 0);
  delay(10);
  display.fillScreen(0);

  #ifdef TFT_BL
  digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
  #endif
  #ifdef AUDIO_ENABLE_PIN
  digitalWrite(AUDIO_ENABLE_PIN, !AUDIO_ENABLE_VAL);
  #endif
  #ifdef SOFT_POWER_SWITCH_PIN
  esp_sleep_enable_ext0_wakeup(SOFT_POWER_SWITCH_PIN, SOFT_POWER_SWITCH_ON_VAL);
  esp_deep_sleep_start();
  #endif
}


void loop()
{
  #ifdef SOFT_POWER_SWITCH_PIN
  if (!softPowerEnabled){
    Serial.println("softPowerOff...");
    softPowerOff();
  }
  #endif

  if (changeChannelPressed || videoPlayer->isFinished()){
    Serial.println("Setting random channel.");
    videoPlayer->stop();
    delay(100);
    randomChannel(changeChannelPressed);
    videoPlayer->play();
  }

  #ifdef VOLUME_POT_PIN
  if (currentVolume != audioOutput->getVolume()){
    // Set volume, but with a limited step size (reduces popping as volume changes)
    int maxStep = max(audioOutput->getVolume() / 10, 3);
    audioOutput->setVolume(moveToward(audioOutput->getVolume(), currentVolume, maxStep));
    #ifdef AUDIO_ENABLE_PIN
    if (audioOutput->getVolume() == 0){
      digitalWrite(AUDIO_ENABLE_PIN, !AUDIO_ENABLE_VAL);
    }
    else{
      digitalWrite(AUDIO_ENABLE_PIN, AUDIO_ENABLE_VAL);
    }
    #endif
  }
  #endif

  buttonLoop();

  delay(20);

}
