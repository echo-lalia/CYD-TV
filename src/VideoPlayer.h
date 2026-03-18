#pragma once
#include <Arduino.h>
// #ifdef LED_MATRIX
// struct File; // fixes weird compilation error
// #endif
#include "JPEGDEC.h"
#include "ChannelData/SDCardChannelData.h"
#include "VideoPlayerState.h"
#include <list>


#ifndef AUDIO_RATE
#define AUDIO_RATE 16000
#endif

#ifndef AUDIO_BUFFER_SAMPLES
#define AUDIO_BUFFER_SAMPLES 1000
#endif
#define BYTES_PER_SAMPLE 1

#ifndef VIDEO_WIDTH
  #if TFT_ROTATION == 0 | TFT_ROTATION == 2
  #define VIDEO_WIDTH TFT_WIDTH
  #else
  #define VIDEO_WIDTH TFT_HEIGHT
  #endif
#endif
#ifndef VIDEO_HEIGHT
  #if TFT_ROTATION == 0 | TFT_ROTATION == 2
  #define VIDEO_HEIGHT TFT_HEIGHT
  #else
  #define VIDEO_HEIGHT TFT_WIDTH
  #endif
#endif


class Display;
class AudioOutput;

// class VideoSource;
// class AudioSource;

class VideoPlayer {
  private:
    int mChannelVisible = 0;
    VideoPlayerState mState = VideoPlayerState::STOPPED;

    // Mutex for ensuring one-at-a-time access to display communication.
    SemaphoreHandle_t displayControlMutex = xSemaphoreCreateMutex();
    JPEGDEC mJpeg = JPEGDEC();

    // channel information
    ChannelData *mChannelData = NULL;
    // The channel we should be drawing (if any) (might differ from played channel for bumpers)
    int channelToDraw = 0;

    // audio playing
    int mCurrentAudioSample = 0;
    AudioOutput *mAudioOutput = NULL;

    // Buffer used for quickly drawing "static" (random noise) to the display.
    uint32_t *staticBuf = (uint32_t*) malloc(VIDEO_WIDTH * 2);
    size_t staticBufLength = VIDEO_WIDTH / 2;
    // Matching buffer for storing modified sparse noise line
    uint32_t *sparseNoiseBuf = (uint32_t*) malloc(VIDEO_WIDTH * 2);

    // The buffer to use for jpeg frame decoding.
    uint8_t *jpegDecodeBuffer = NULL;
    // The total size of the jpeg decode buffer
    size_t jpegDecodeBufferLength = 0;
    // The real length of jpeg data stored in the buffer (might be less than buffer length)
    size_t jpegDecodeLength = 0;
    // The buffer to read jpeg data into
    uint8_t *jpegReadBuffer = NULL;
    // The total size of the jpeg decode buffer
    size_t jpegReadBufferLength = 0;
    // The real length of jpeg data stored in the buffer (might be less than buffer length)
    size_t jpegReadLength = 0;
    bool frameReady = false;
    // Mutex used to lock jpeg decoding while the audio task is swapping the buffers (and setting frameReady).
    // Otherwise the audio task only writes to the "read" buffer, and the frame task only reads from the "decode" buffer.
    SemaphoreHandle_t jpegBufferMutex = xSemaphoreCreateMutex();
  
    // Track whether or not the previous _drawFrame loop drew a frame
    // (used for limiting the amount of vTaskDelay calls we make)
    bool drewFrameLastLoop = false;

    // used for calculating frame rate
    std::list<int> frameTimes;

    static void _framePlayerTask(void *param);
    static void _audioPlayerTask(void *param);

    void _drawSparseNoise();
    void _drawStatic();
    void _drawFrame();
    void framePlayerTask();
    void audioPlayerTask();
    int _getAudioSamples(uint8_t **buffer, size_t &bufferSize, int currentAudioSample);

    friend int _doDraw(JPEGDRAW *pDraw);

  public:
    VideoPlayer(ChannelData *channelData, Display &display, AudioOutput *audioOutput);
    void drawChannel(int channelIndex);
    void setChannel(int channelIndex);
    void start();
    void play();
    void _setPlayingFinished();
    bool isFinished() {return (mState == VideoPlayerState::PLAYING_FINISHED);}
    void stop();
    void pause();
    void playStatic();

    // Display to draw video frames on
    Display &mDisplay;
    // Enable/disable drawing a distortion/noise (when display is touched)
    bool drawTouchDistortion = false;
    // Whether or not to use an alternate drawing function to reduce brightness and blue light.
    bool nightModeEnabled = false;
};