#ifndef SERVER_API_H
#define SERVER_API_H

#include <Arduino.h>

namespace ServerApi
{
String postJson(const String &path, const String &body, int *httpStatus = nullptr);
String postAiText(const String &text);
bool postGps(double latitude, double longitude);
String postNavigationUpdate(const String &destination);
bool postExitNavigation();
}

#endif // SERVER_API_H
