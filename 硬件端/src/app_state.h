#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

enum AppState
{
  BOOTING,
  WIFI_CONNECTING,
  IDLE,
  LISTENING,
  RECORDING,
  ASR_PROCESSING,
  SERVER_PROCESSING,
  SPEAKING,
  NAVIGATING,
  ERROR_STATE
};

void setAppState(AppState state);
AppState getAppState();
const char *appStateToString(AppState state);

void setAppNavigationActive(bool active);
bool getAppNavigationActive();

#endif // APP_STATE_H
