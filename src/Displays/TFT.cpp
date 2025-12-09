#ifndef LED_MATRIX
#include <Arduino.h>
#include "TFT.h"


#ifndef TFT_ROTATION
#define TFT_ROTATION 1
#endif


#ifndef SD_MOUNT_FAILED_MSG
#define SD_MOUNT_FAILED_MSG "Failed to mount SD Card"
#endif


TFT::TFT(): tft(new TFT_eSPI()) {}

void TFT::init(){
  // power on the tft
  #ifdef TFT_POWER
  if (TFT_POWER != GPIO_NUM_NC) {
    Serial.println("Powering on TFT");
    pinMode(TFT_POWER, OUTPUT);
    digitalWrite(TFT_POWER, TFT_POWER_ON);
  }
  #endif
  tft->init();
  tft->setRotation(TFT_ROTATION);
  tft->fillScreen(TFT_BLACK);
  #ifdef USE_DMA
  tft->initDMA();
  #endif
  tft->fillScreen(TFT_BLACK);
  tft->setTextFont(2);
  tft->setTextSize(2);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
}

void TFT::drawPixels(int x, int y, int width, int height, uint16_t *pixels) {
  int numPixels = width * height;
  if (dmaBuffer[dmaBufferIndex] == NULL) {
    dmaBuffer[dmaBufferIndex] = (uint16_t *)malloc(numPixels * 2);
  }
  else {
    dmaBuffer[dmaBufferIndex] = (uint16_t *)realloc(dmaBuffer[dmaBufferIndex], numPixels * 2);
  }
  memcpy(dmaBuffer[dmaBufferIndex], pixels, numPixels * 2);
  #ifdef USE_DMA
  tft->dmaWait();
  #endif
  tft->setAddrWindow(x, y, width, height);
  #ifdef USE_DMA
  tft->pushPixelsDMA(dmaBuffer[dmaBufferIndex], numPixels);
  #else
  tft->pushPixels(dmaBuffer[dmaBufferIndex], numPixels);
  #endif
  dmaBufferIndex = (dmaBufferIndex + 1) % 2;
}

void TFT::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
  tft->fillRect(x, y, w, h, color);
}

void TFT::drawPixel(int x, int y, uint16_t color){
  tft->drawPixel(x, y, color);
}

void TFT::startWrite() {
  tft->startWrite();
}

void TFT::endWrite() {
  tft->endWrite();
}

int TFT::width() {
  return tft->width();
}

int TFT::height() {
  return tft->height();
}

void TFT::fillScreen(uint16_t color) {
  tft->fillScreen(color);
}

void TFT::drawChannel(int channelIndex) {
  tft->setCursor(20, 20);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->printf("%d", channelIndex);
}

void TFT::drawTuningText() {
  tft->setCursor(20, 20);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->println("TUNING...");
}

void TFT::drawSDCardFailed() {
  tft->fillScreen(TFT_RED);
  tft->setCursor(0, 20);
  tft->setTextColor(TFT_WHITE);
  tft->setTextSize(2);
  tft->println(SD_MOUNT_FAILED_MSG);
}

void TFT::drawFPS(int fps) {
    // show the frame rate in the top right
    tft->setCursor(VIDEO_WIDTH - 50, 20);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->printf("%02d", fps);
}
#endif