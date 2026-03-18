#include <Arduino.h>
#include "AVIParser/AVIParser.h"
#include "VideoPlayer.h"
#include "AudioOutput/AudioOutput.h"
#include "ChannelData/SDCardChannelData.h"
#include "Displays/Display.h"




void VideoPlayer::_framePlayerTask(void *param)
{
  VideoPlayer *player = (VideoPlayer *)param;
  player->framePlayerTask();
}

void VideoPlayer::_audioPlayerTask(void *param)
{
  VideoPlayer *player = (VideoPlayer *)param;
  player->audioPlayerTask();
}

VideoPlayer::VideoPlayer(ChannelData *channelData, Display &display, AudioOutput *audioOutput)
: mChannelData(channelData), mDisplay(display), mState(VideoPlayerState::STOPPED), mAudioOutput(audioOutput)
{
}

void VideoPlayer::start()
{
  // allocate some small initial buffers for jpeg data
  jpegDecodeBuffer = (uint8_t *) malloc(1024);
  jpegDecodeBufferLength = 1024;
  jpegReadBuffer = (uint8_t *) malloc(1024);
  jpegReadBufferLength = 1024;

  // launch the frame player task
  xTaskCreatePinnedToCore(
      _framePlayerTask,
      "Frame Player",
      1024 * 16,
      this,
      1,
      NULL,
      0);
  xTaskCreatePinnedToCore(_audioPlayerTask, "audio_loop", 1024 * 16, this, 1, NULL, 1);
}

void VideoPlayer::drawChannel(int channel)
{
  channelToDraw = channel;
  if (xSemaphoreTake(displayControlMutex, 100)){
    mDisplay.drawChannel(channel);
    xSemaphoreGive(displayControlMutex);
  }
  mChannelVisible = millis();
}

void VideoPlayer::setChannel(int channel)
{
  
  Serial.println("Setting channel in VideoPlayer::setChannel");
  mChannelData->setChannel(channel);
  // set the audio sample to 0 - TODO - move this somewhere else?
  mCurrentAudioSample = 0;
}

void VideoPlayer::play()
{
  Serial.println("VideoPlayer::play()");
  if (mState == VideoPlayerState::PLAYING)
  {
    return;
  }
  mState = VideoPlayerState::PLAYING;
  mCurrentAudioSample = 0;
}

void VideoPlayer::stop()
{
  Serial.println("VideoPlayer::stop()");
  if (mState == VideoPlayerState::STOPPED)
  {
    return;
  }
  mState = VideoPlayerState::STOPPED;
  mCurrentAudioSample = 0;
  if (xSemaphoreTake(displayControlMutex, 100)) {
    mDisplay.fillScreen(DisplayColors::BLACK);
    xSemaphoreGive(displayControlMutex);
  }
}

void VideoPlayer::_setPlayingFinished()
{
  Serial.println("VideoPlayer::_setPlayingFinished()");
  if (mState == VideoPlayerState::PLAYING_FINISHED){
    return;
  }
  // wait for any current frame to finish drawing
  bool stillDrawing = true;
  while (stillDrawing){
    if (xSemaphoreTake(jpegBufferMutex, 1000)){
      stillDrawing = frameReady;
      xSemaphoreGive(jpegBufferMutex);
    }
    else {
      Serial.println("Timeout waiting for jpegBufferMutex in _setPlayingFinished");
      stillDrawing = false;
    }
  }
  Serial.println("Playing finished.");
  mState = VideoPlayerState::PLAYING_FINISHED;
  frameReady = false;
  mCurrentAudioSample = 0;
  if (xSemaphoreTake(displayControlMutex, 100)) {
    mDisplay.fillScreen(DisplayColors::BLACK);
    xSemaphoreGive(displayControlMutex);
  }
}

void VideoPlayer::pause()
{
  Serial.println("VideoPlayer::pause()");
  if (mState == VideoPlayerState::PAUSED)
  {
    return;
  }
  mState = VideoPlayerState::PAUSED;
}

void VideoPlayer::playStatic()
{
  Serial.println("VideoPlayer::playstatic()");
  if (mState == VideoPlayerState::STATIC)
  {
    return;
  }
  mState = VideoPlayerState::STATIC;
}


inline uint16_t swapI16Bytes(int val) {
  return (val >> 8) | (val << 8);
}


int _doDraw(JPEGDRAW *pDraw)
{
  VideoPlayer *player = (VideoPlayer *)pDraw->pUser;
  player->mDisplay.drawPixels(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}


// alternate drawing method that reduces brightness and blue light
int _doDrawNightMode(JPEGDRAW *pDraw)
{
  VideoPlayer *player = (VideoPlayer *)pDraw->pUser;
  // Reduce brightness and blue light by masking out some bits (mask for 16 bit RGB565 ints, with swapped bytes)
  for (int i=0; i<(pDraw->iWidth * pDraw->iHeight); i++){
    // 11111 111111 00000
    // 110 0000011111 111
    uint16_t px = swapI16Bytes(pDraw->pPixels[i]);
    uint16_t r = px >> 11;
    uint16_t g = (px >> 5) & 0b111111;
    uint16_t b = px & 0b11111;
    r /= 2;
    g /= 4;
    // g = g * 2 / 5; // reduce green slightly more than red to prevent a greenish tint that appears otherwise
    b /= 8; // reduce blue the most because it's the hardest on the eyes in low light
    pDraw->pPixels[i] = swapI16Bytes((r << 11) | (g << 5) | b); // combine back into RGB565 format
    // pDraw->pPixels[i] = (pDraw->pPixels[i] & 0b1110000011111111);
  }
  player->mDisplay.drawPixels(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}


// Draw full-screen static noise
void VideoPlayer::_drawStatic()
{
  if (xSemaphoreTake(displayControlMutex, 0)) {
    // Fill the static buffer with random pixels
    for (int i=0; i<staticBufLength; i++){
      staticBuf[i] = random();
    }
    mDisplay.startWrite();
    // Draw static one hline at a time.
    for(int y=0; y<mDisplay.height(); y++){
      // iterate over static buffer to quickly pseudo re-randomize the pixels
      uint32_t lineRandom = random();
      for (int i=0; i<staticBufLength; i++){
        staticBuf[i] = staticBuf[i] ^ lineRandom;
      }
      // Draw hline of static to the display.
      mDisplay.drawPixels(0, y, VIDEO_WIDTH, 1, (uint16_t *)staticBuf);
    }
    // Done drawing static this frame.
    mDisplay.endWrite();
    xSemaphoreGive(displayControlMutex);
  }
}


int _touchJitterXOffset = 0;
int _touchJitterYOffset = 0;
// Update the offsets for the jitter effect used when display is touched.
void _updateTouchJitterOffsets(){
  // Most common behaviour; slightly jitter draw position
  if (random(0, 6) != 0) {
    _touchJitterXOffset = random(-1, 1);
    _touchJitterYOffset = random(-1, 1);
  } 
  // Rarest behaviour; jitter the X axis more (causes major artifacts on screen)
  else if (random(0, 12) == 0) {
    _touchJitterXOffset = random(-5, 5);
    _touchJitterYOffset = random(-1, 1);
  }
  // Less rare behaviour; jitter the Y axis more (causes minimal artifacts)
  else {
    _touchJitterYOffset = random(-8, 8);
    _touchJitterXOffset = random(-1, 1);
  }
}


// Draw sparse noise effect when screen is touched
void VideoPlayer::_drawSparseNoise()
{
  // Previous noise lines may be leftover on the right of the display when x jitter is < 0
  if (_touchJitterXOffset != 0) {
    mDisplay.fillRect(VIDEO_WIDTH - abs(_touchJitterXOffset), 0, abs(_touchJitterXOffset) + 1, VIDEO_HEIGHT, 0);
  }
  // Initialize the static buffer with random pixels.
  for (int i=0; i<staticBufLength; i++){
    staticBuf[i] = random();
  }
  // we don't need to call `mDisplay.startWrite` because it should already be started by the _drawFrame function.
  // Draw static one hline at a time.
  for (int y=2; y<mDisplay.height(); y += 4){
    uint32_t lineRandom = random();
    // Only draw ~1/8 of the lines to give some randomness to the distribution
    if ((lineRandom & 0b111) == 0) {
      // iterate over static buffer to quickly pseudo re-randomize the pixels
      for (int i=0; i<staticBufLength; i++){
        staticBuf[i] = staticBuf[i] ^ lineRandom;
      }
      // Decrease the brightness for each pixel by masking out some bits (pre-calculated 32 bits for two RGB565 ints, with swapped bytes)
      for (int i=0; i<staticBufLength; i++){
        sparseNoiseBuf[i] = (staticBuf[i] & 0b11010111101111011101011110111101); // 10111 101110 10111
      }
      // Draw a double-height hline of static to the display.
      mDisplay.drawPixels(0, y, VIDEO_WIDTH, 1, (uint16_t *)sparseNoiseBuf);
      mDisplay.drawPixels(0, y + 1, VIDEO_WIDTH, 1, (uint16_t *)sparseNoiseBuf);
    }
  }
  // Done drawing static this frame.
  mDisplay.endWrite();
}


void VideoPlayer::_drawFrame()
{
  bool frameDrawn = false;
  // Take the jpeg mutex lock to check if a frame is ready, and draw if it is.
  if (xSemaphoreTake(jpegBufferMutex, 100)){
    // If the frame is ready, also take the display control mutex.
    if (frameReady && xSemaphoreTake(displayControlMutex, 1000)){
      // Draw the frame!
      if (mJpeg.openRAM(jpegDecodeBuffer, jpegDecodeLength, (nightModeEnabled ? _doDrawNightMode : _doDraw)))
      {
        mDisplay.startWrite();
        mJpeg.setUserPointer(this);
        mJpeg.setPixelType(RGB565_BIG_ENDIAN);
        if (drawTouchDistortion){
          // Draw frame with a random offset if screen is being touched
          mJpeg.decode(_touchJitterXOffset, _touchJitterYOffset, 0);
          _updateTouchJitterOffsets();
        } else {
          // Draw normally
          mJpeg.decode(0, 0, 0);
        }
      }
      frameReady = false;
      frameDrawn = true;
    }
    xSemaphoreGive(jpegBufferMutex);
  }

  // If we drew a new frame above, finish any final tasks that dont require the mutex.
  if (frameDrawn){
    frameTimes.push_back(millis());
    // keep the frame rate elapsed time to 5 seconds
    while(frameTimes.size() > 0 && frameTimes.back() - frameTimes.front() > 1000) {
      frameTimes.pop_front();
    }
    // show channel indicator
    if (millis() - mChannelVisible < 2000) {
      mDisplay.drawChannel(channelToDraw);
    }
    #if CORE_DEBUG_LEVEL > 0
    mDisplay.drawFPS(frameTimes.size());
    #endif
    // draw sparse/touch noise effect
    if (drawTouchDistortion) {
      _drawSparseNoise();
    }
    // Close the display write operation (which was opened above with the jpegBufferMutex).
    mDisplay.endWrite();
    // Return display control.
    xSemaphoreGive(displayControlMutex);
    drewFrameLastLoop = true;
  }
  else {
    // Adding a task delay is important for allowing the IDLE task to run (and feed the watchdog timer).
    // however, it can also prevent us from reaching a high (>~15) fps.
    // as a compromise, we can just add a task delay a max of once per frame.
    if (drewFrameLastLoop){
      drewFrameLastLoop = false;
      vTaskDelay(1);
    }
  }
}


void VideoPlayer::framePlayerTask()
{
  size_t staticBufLength = VIDEO_WIDTH / 2;
  uint32_t *staticBuf = (uint32_t*) malloc(VIDEO_WIDTH * 2);
  
  while (true)
  {
    // Draw random static to the display.
    if (mState == VideoPlayerState::STATIC){
      _drawStatic();
      vTaskDelay(4 / portTICK_PERIOD_MS);
      continue;
    }

    // Draw a video frame if one is available.
    if (mState == VideoPlayerState::PLAYING){
      _drawFrame();
      continue;
    }

    // nothing to do - just wait
    vTaskDelay(100 / portTICK_PERIOD_MS);
    continue;
  }
}


void VideoPlayer::audioPlayerTask()
{
  size_t bufferLength = AUDIO_BUFFER_SAMPLES * BYTES_PER_SAMPLE;
  uint8_t *audioData = (uint8_t *)malloc(bufferLength);
  while (true)
  {
    if (mState != VideoPlayerState::PLAYING)
    {
      // nothing to do - just wait
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    // get audio data to play
    int audioLength = _getAudioSamples(&audioData, bufferLength, mCurrentAudioSample);
    // have we reached the end of the channel?
    if (audioLength == 0) {
      // I don't really understand why, but if we MUST give the frame player task time to finish drawing,
      // otherwise, the program will halt.
      vTaskDelay(100 / portTICK_PERIOD_MS);
      _setPlayingFinished();
      continue;
    }
    if (audioLength > 0) {
      // play the audio
      for(int i=0; i<audioLength; i+=AUDIO_BUFFER_SAMPLES) {
        mAudioOutput->write(audioData + i, min(AUDIO_BUFFER_SAMPLES, audioLength - i));
        mCurrentAudioSample += min(AUDIO_BUFFER_SAMPLES, audioLength - i);
        if (mState != VideoPlayerState::PLAYING)
        {
          mCurrentAudioSample = 0;
          break;
        }
      }
    }
    else
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}


int VideoPlayer::_getAudioSamples(uint8_t **buffer, size_t &bufferSize, int currentAudioSample)
{
  // read the audio data into the buffer
  AVIParser *parser = mChannelData->getVideoParser();
  if (parser) {
    ChunkHeader header = {OTHER_CHUNK, 0};
    
    while (header.chunkType != EMPTY_CHUNK)
    {
      header = parser->getNextHeader();

      // skip empty chunks
      if (header.chunkSize == 0) {continue;}

      // read audio data
      if (header.chunkType == AUDIO_CHUNK){
        int audioLength = parser->getNextChunk(header, (uint8_t **) buffer, bufferSize);
        return audioLength;
      }

      // Handle processing video chunks.
      else if (header.chunkType == VIDEO_CHUNK){
        // read video data
        jpegReadLength = parser->getNextChunk(header, (uint8_t **) &jpegReadBuffer, jpegReadBufferLength);

        // Take the mutex lock and swap the read/decode buffers
        if (xSemaphoreTake(jpegBufferMutex, 1000)){
          if (frameReady) {Serial.println("Overwriting video chunk!");}

          uint8_t *tempBuffer = jpegDecodeBuffer;
          size_t tempBufferLength = jpegDecodeBufferLength;
          size_t tempLength = jpegReadLength;

          jpegDecodeBuffer = jpegReadBuffer;
          jpegDecodeBufferLength = jpegReadBufferLength;
          jpegDecodeLength = jpegReadLength;

          jpegReadBuffer = tempBuffer;
          jpegReadBufferLength = tempBufferLength;
          jpegReadLength = tempLength;
          frameReady = true;

          xSemaphoreGive(jpegBufferMutex);
        }
        else{
          Serial.println("_getAudioSamples failed to get semaphore and skipped video chunk.");
        }
      }

      else {
        // this chunk is of no use to us. Skip it!
        Serial.println("Found useless chunk.");
        parser->getNextChunk(header, (uint8_t **) buffer, bufferSize, true);
      }

    }
  }
  Serial.println("No audio or video chunks found!");
  return 0;
}
