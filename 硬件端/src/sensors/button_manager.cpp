#include "button_manager.h"

#include <Arduino.h>

#include "config.h"

void buttonManagerInitPins()
{
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
}
