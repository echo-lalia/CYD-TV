#include <Arduino.h>
#include "../SDCard.h"
#include "SDCardChannelData.h"
#include "../AVIParser/AVIParser.h"


// Store some channel information in RTC ram so it persists through deep sleep.
// The seed previously used to shuffle the channel list.
RTC_DATA_ATTR uint32_t shuffleSeed = 0;
// The index of the current channel in the shuffle
RTC_DATA_ATTR uint32_t shuffledChannelIndex = 0;


ChannelData::ChannelData(SDCard *sdCard, const char *aviPath, const char *bumperPath): mSDCard(sdCard), mAviPath(aviPath), mBumperPath(bumperPath) {

}

bool ChannelData::fetchChannelData() {
  // check the the sd card is mounted
  if (!mSDCard->isMounted()) {
    Serial.println("SD card is not mounted");
    return false;
  }

  // get the list of AVI files
  mAviFiles = mSDCard->listFiles(mAviPath, ".avi");
  mBumperFiles = mSDCard->listFiles(mBumperPath, ".avi");

  

  
  if (shuffleSeed == 0){
    _resetShuffleSeed();
  }
  else {
    Serial.printf("Stored shuffle seed: %u\n", shuffleSeed);
  }
  if (shuffledChannelIndex != 0){
    Serial.printf("Stored shuffled channel index: %u\n", shuffledChannelIndex);
    // The channel index is incremented each time it's accessed, so moving back one starts us where we were pre-deepsleep
    shuffledChannelIndex--;
  }
  reshuffleChannels();

  if (mAviFiles.size() == 0) {
    Serial.println("No AVI files found");
    return false;
  }
  return true;
}


void ChannelData::setChannel(int channel) {
  if (!mSDCard->isMounted()) {
    Serial.println("SD card is not mounted");
    return;
  }
  // check that the channel is valid
  if (channel < 0 && abs(channel) > mBumperFiles.size()){
    Serial.printf("Invalid bumper channel %d (%d)\n", channel, abs(channel) - 1);
    return;
  }
  if (channel >= 0 && channel >= mAviFiles.size()) {
    Serial.printf("Invalid channel %d\n", channel);
    return;
  }
  // close any open AVI files
  if (mCurrentChannelVideoParser) {
    delete mCurrentChannelVideoParser;
    mCurrentChannelVideoParser = NULL;
  }
  // open the AVI file
  std::string aviFilename;
  // Channels from -1 and below are bumpers
  if (channel < 0 && mBumperFiles.size() > 0){
    aviFilename = mBumperFiles[abs(channel) - 1];
  }
  // Otherwise, it's a normal channel
  else {
    aviFilename = mAviFiles[channel];
  }
  Serial.printf("Opening AVI file %s\n", aviFilename.c_str());
  mCurrentChannelVideoParser = new AVIParser(aviFilename, AVIChunkType::VIDEO);
  if (!mCurrentChannelVideoParser->open()) {
    Serial.printf("Failed to open AVI file %s\n", aviFilename.c_str());
    delete mCurrentChannelVideoParser;
    mCurrentChannelVideoParser = NULL;
  }
  mChannelNumber = channel;
}


void ChannelData::_initShuffledChannels(){
  if (mShuffledChannels.size() != mAviFiles.size()){
    mShuffledChannels.resize(mAviFiles.size());
  }
  for (int i = 0; i < mAviFiles.size(); i++){
    mShuffledChannels[i] = i;
  }
}

void ChannelData::_resetShuffleSeed(){
  shuffleSeed = esp_random();
  Serial.printf("New shuffle seed: %u\n", shuffleSeed);
}


void ChannelData::reshuffleChannels(){
  Serial.println("Reshuffling channels...");
  _initShuffledChannels();
  // Seed the pseudo rng with the stored value
  randomSeed(shuffleSeed);

  if (mAviFiles.size() <= 1){
    Serial.println("No channels to reshuffle.");
    return;
  }

  for (int idx = 0; idx < mShuffledChannels.size() - 2; idx++){
    int idx2 = random(idx, mShuffledChannels.size());
    int tmp = mShuffledChannels[idx];
    mShuffledChannels[idx] = mShuffledChannels[idx2];
    mShuffledChannels[idx2] = tmp;
  }

  #if CORE_DEBUG_LEVEL > 2
  Serial.println("Shuffled order:");
  for (int i = 0; i < mShuffledChannels.size(); i++){
    Serial.printf("%d ", mShuffledChannels[i]);
  }
  Serial.print("\n");
  #endif
  Serial.println("Channels reshuffled.");
}


int ChannelData::getNextChannel(){
  // If it's currently a normal channel, return a random bumper next (if we have any)
  if (mChannelNumber >= 0 && mBumperFiles.size() > 0){
    randomSeed(esp_random());
    int bumperIndex = random(0, mBumperFiles.size());
    return -1 * (bumperIndex + 1);
  }
  // If we previously played a bumper, or we have no bumpers, return the next normal channel
  shuffledChannelIndex++;
  if (shuffledChannelIndex >= mShuffledChannels.size()){
    shuffledChannelIndex = 0;
    _resetShuffleSeed();
    reshuffleChannels();
  }
  return mShuffledChannels[shuffledChannelIndex];
}


int ChannelData::peekNextChannelNum(){
  // Attempt to peek at the next channel, without iterating.
  // This won't work correctly if the channels would need to be reshuffled,
  // but that doesn't matter much, since this is only for displaying the next channel number.
  if (mShuffledChannels.size() == 0){
    return 0;
  }
  uint32_t wrappedIndex = (shuffledChannelIndex + 1) % mShuffledChannels.size();
  return mShuffledChannels[wrappedIndex];
}
