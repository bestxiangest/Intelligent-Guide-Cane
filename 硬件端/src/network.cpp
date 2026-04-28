#include "network.h"
#include "services/server_api.h"

static const char *wifiStatusToString(wl_status_t status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

static void logMatchingWifiNetworks()
{
  Serial.println("  Scanning nearby WiFi networks for diagnostics...");
  int networkCount = WiFi.scanNetworks(false, true);
  if (networkCount < 0)
  {
    Serial.printf("  WiFi scan failed: %d\n", networkCount);
    return;
  }

  bool foundTarget = false;
  for (int i = 0; i < networkCount; ++i)
  {
    String ssid = WiFi.SSID(i);
    if (ssid == WIFI_SSID)
    {
      foundTarget = true;
      Serial.printf("  Found target SSID: channel=%d rssi=%d dBm auth=%d\n",
                    WiFi.channel(i),
                    WiFi.RSSI(i),
                    WiFi.encryptionType(i));
    }
  }

  if (!foundTarget)
  {
    Serial.printf("  Target SSID was not found in %d scanned networks.\n", networkCount);
  }

  WiFi.scanDelete();
}

void initWiFi()
{
  Serial.println("Starting WiFi connection...");
  Serial.printf("  SSID: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.disconnect(false, true);
  delay(250);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  const int maxAttempts = 20;

  Serial.print("  Connecting");
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    delay(1000);
    Serial.print(".");
    attempts++;

    if (attempts % 5 == 0)
    {
      wl_status_t status = WiFi.status();
      Serial.printf(" [%d s, status %d/%s] ",
                    attempts,
                    status,
                    wifiStatusToString(status));
    }
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi connected");
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("  DNS1: %s\n", WiFi.dnsIP(0).toString().c_str());
    Serial.printf("  DNS2: %s\n", WiFi.dnsIP(1).toString().c_str());
    isConnectedToWifi = true;
  }
  else
  {
    Serial.println("WiFi connection failed");
    wl_status_t status = WiFi.status();
    Serial.printf("  Final status: %d/%s\n", status, wifiStatusToString(status));
    logMatchingWifiNetworks();
    isConnectedToWifi = false;
  }
}

String sendTextToServer(String text)
{
  return ServerApi::postAiText(text);
}

bool sendGpsData(double latitude, double longitude)
{
  Serial.printf("[GPS] POST %s | payload={\"latitude\":%.6f,\"longitude\":%.6f}\n",
                (String(SERVER_BASE_URL) + "/gps").c_str(), latitude, longitude);
  return ServerApi::postGps(latitude, longitude);
}
