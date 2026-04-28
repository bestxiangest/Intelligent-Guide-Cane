#include "app_state.h"

namespace
{
volatile AppState currentState = BOOTING;
volatile bool navigationActiveFlag = false;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
}

void setAppState(AppState state)
{
  portENTER_CRITICAL(&stateMux);
  currentState = state;
  portEXIT_CRITICAL(&stateMux);
}

AppState getAppState()
{
  portENTER_CRITICAL(&stateMux);
  AppState state = currentState;
  portEXIT_CRITICAL(&stateMux);
  return state;
}

const char *appStateToString(AppState state)
{
  switch (state)
  {
  case BOOTING:
    return "BOOTING";
  case WIFI_CONNECTING:
    return "WIFI_CONNECTING";
  case IDLE:
    return "IDLE";
  case LISTENING:
    return "LISTENING";
  case RECORDING:
    return "RECORDING";
  case ASR_PROCESSING:
    return "ASR_PROCESSING";
  case SERVER_PROCESSING:
    return "SERVER_PROCESSING";
  case SPEAKING:
    return "SPEAKING";
  case NAVIGATING:
    return "NAVIGATING";
  case ERROR_STATE:
    return "ERROR_STATE";
  default:
    return "UNKNOWN";
  }
}

void setAppNavigationActive(bool active)
{
  portENTER_CRITICAL(&stateMux);
  navigationActiveFlag = active;
  portEXIT_CRITICAL(&stateMux);
}

bool getAppNavigationActive()
{
  portENTER_CRITICAL(&stateMux);
  bool active = navigationActiveFlag;
  portEXIT_CRITICAL(&stateMux);
  return active;
}
