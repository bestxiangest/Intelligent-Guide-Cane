#include "light_manager.h"

#include <Arduino.h>

#include "config.h"

void lightManagerInitPins()
{
  pinMode(LIGHT_SENSOR_DO_PIN, INPUT);
  pinMode(LIGHT_CONTROL_PIN, OUTPUT);
  digitalWrite(LIGHT_CONTROL_PIN, LOW);
}
