#include "gps_manager.h"

#include "gps.h"

void gpsManagerInit()
{
  gpsInit();
}

void gpsManagerTask(void *pvParameters)
{
  gpsTask(pvParameters);
}
