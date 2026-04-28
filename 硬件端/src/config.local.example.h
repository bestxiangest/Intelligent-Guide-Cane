#pragma once

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define SERVER_BASE_URL "http://192.168.1.100:12345"
#define DEVICE_ID "guide-cane-001"

#define BAIDU_CLIENT_ID "YOUR_BAIDU_CLIENT_ID"
#define BAIDU_CLIENT_SECRET "YOUR_BAIDU_CLIENT_SECRET"
#define BAIDU_CUID "YOUR_BAIDU_CUID"
#define BAIDU_TTS_PER 4146
#define BAIDU_TTS_SPD 6
#define BAIDU_TTS_PIT 5
#define BAIDU_TTS_VOL 5
#define BAIDU_TTS_AUE 6

// Optional: seed a known-valid Baidu access token into ESP32 NVS on first boot.
// Leave empty in shared configs.
#define BAIDU_ACCESS_TOKEN_SEED ""
#define BAIDU_ACCESS_TOKEN_EXPIRES_AT_SEED 0ULL
