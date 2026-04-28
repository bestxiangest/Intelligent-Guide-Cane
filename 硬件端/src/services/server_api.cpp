#include "server_api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "config.h"

namespace ServerApi
{
String postJson(const String &path, const String &body, int *httpStatus)
{
  String serverUrl = String(SERVER_BASE_URL) + path;
  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(SERVER_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-ID", DEVICE_ID);

  int status = http.POST(body);
  if (httpStatus)
  {
    *httpStatus = status;
  }

  String payload = "";
  if (status > 0)
  {
    payload = http.getString();
  }
  else
  {
    Serial.printf("[ServerApi] POST %s failed, http=%d\n", path.c_str(), status);
  }

  http.end();
  return payload;
}

String postAiText(const String &text)
{
  DynamicJsonDocument doc(1024);
  doc["device_id"] = DEVICE_ID;
  doc["message"] = text;
  String body;
  serializeJson(doc, body);
  return postJson("/ai", body);
}

bool postGps(double latitude, double longitude)
{
  DynamicJsonDocument doc(256);
  doc["device_id"] = DEVICE_ID;
  doc["latitude"] = latitude;
  doc["longitude"] = longitude;
  String body;
  serializeJson(doc, body);

  int status = 0;
  String payload = postJson("/gps", body, &status);
  bool ok = status >= 200 && status < 300;
  if (ok)
  {
    Serial.printf("[GPS] Server acknowledged upload | http=%d body=%s\n", status, payload.c_str());
  }
  else
  {
    Serial.printf("[GPS] Server rejected upload | http=%d body=%s\n", status, payload.c_str());
  }
  return ok;
}

String postNavigationUpdate(const String &destination)
{
  DynamicJsonDocument doc(512);
  doc["device_id"] = DEVICE_ID;
  doc["destination"] = destination;
  String body;
  serializeJson(doc, body);
  return postJson("/navigation_update", body);
}

bool postExitNavigation()
{
  DynamicJsonDocument doc(256);
  doc["device_id"] = DEVICE_ID;
  doc["action"] = "exit_navigation";
  String body;
  serializeJson(doc, body);

  int status = 0;
  postJson("/exit_navigation", body, &status);
  return status >= 200 && status < 300;
}
}
