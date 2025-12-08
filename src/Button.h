#include <Arduino.h>

// setup change_channel pin.
#ifdef CHANGE_CHANNEL_PIN
  #ifndef CHANGE_CHANNEL_PIN_MODE
  #define CHANGE_CHANNEL_PIN_MODE INPUT
  #endif
  #ifndef CHANGE_CHANNEL_TRIGGER_VAL
  #define CHANGE_CHANNEL_TRIGGER_VAL LOW
  #endif
#endif

// setup audio potentiometer pin
#ifdef VOLUME_POT_PIN
  #ifndef VOLUME_POT_MAX
  #define VOLUME_POT_MAX 4095
  #endif
#endif

// setup soft power switch pin
#ifdef SOFT_POWER_SWITCH_PIN
  #ifndef SOFT_POWER_SWITCH_MODE
  #define SOFT_POWER_SWITCH_MODE INPUT
  #endif
  #ifndef SOFT_POWER_SWITCH_ON_VAL
  #define SOFT_POWER_SWITCH_ON_VAL HIGH
  #endif
#endif

// setup touch pin
#ifdef TOUCH_PIN
  #ifndef TOUCH_PIN_TRIGGER_VAL
  #define TOUCH_PIN_TRIGGER_VAL LOW
  #endif
  #ifndef TOUCH_PIN_MODE
  #define TOUCH_PIN_MODE INPUT_PULLUP
  #endif
#endif


bool changeChannelPressed = false;
int currentVolume = 255;
bool softPowerEnabled = true;
bool screenTouched = false;


bool buttonRight(){
#ifdef BUTTON_R
  return (_btn_right == 0);
#endif
  return false;
}

bool buttonLeft(){
#ifdef BUTTON_L
  return (_btn_left == 0);
#endif
  return false;
}

bool buttonUp(){
  return false;
}

bool buttonDown(){
  return false;
}

bool buttonPowerOff() {
#ifdef BUTTON_L
  #ifdef BUTTON_R
    return (_btn_left == 0 && _btn_right == 0);
  #endif
#endif
  return false;
}

void buttonInit(){
  #ifdef CHANGE_CHANNEL_PIN
  pinMode(CHANGE_CHANNEL_PIN, CHANGE_CHANNEL_PIN_MODE);
  #endif
  #ifdef VOLUME_POT_PIN
  pinMode(VOLUME_POT_PIN, INPUT);
  #endif
  #ifdef SOFT_POWER_SWITCH_PIN
  pinMode(SOFT_POWER_SWITCH_PIN, SOFT_POWER_SWITCH_MODE);
  #endif

  #ifdef TOUCH_PIN
  pinMode(TOUCH_PIN, TOUCH_PIN_MODE);
  #endif

  #ifdef BUTTON_L
  #ifdef BUTTON_R
  pinMode(BUTTON_L, INPUT_PULLUP);
  pinMode(BUTTON_R, INPUT);
  #endif
  #endif
}

void buttonLoop(){
  #ifdef CHANGE_CHANNEL_PIN
  changeChannelPressed = (digitalRead(CHANGE_CHANNEL_PIN) == CHANGE_CHANNEL_TRIGGER_VAL);
  #endif
  #ifdef VOLUME_POT_PIN
  currentVolume = analogRead(VOLUME_POT_PIN) / (VOLUME_POT_MAX/255);
  #endif
  #ifdef SOFT_POWER_SWITCH_PIN
  softPowerEnabled = (digitalRead(SOFT_POWER_SWITCH_PIN) == SOFT_POWER_SWITCH_ON_VAL);
  #endif
  #ifdef TOUCH_PIN
  screenTouched = (digitalRead(TOUCH_PIN) == TOUCH_PIN_TRIGGER_VAL);
  #endif
}


