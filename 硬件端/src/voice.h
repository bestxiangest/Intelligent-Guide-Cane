#ifndef VOICE_H
#define VOICE_H

#include <Arduino.h>
#include <string.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UrlEncode.h>

#include "config.h"

void set_i2s();
String getAccessToken_baidu();
String waitForAccessToken_baidu();
String baidu_voice_recognition(String accessToken, uint8_t *audioData, int audioDataSize);
bool baiduTTS_Send(String access_token, String text);
bool playAudioBuffer(uint8_t *audioBuffer, size_t audioLength);
bool playLocalAudioBuffer(uint8_t *audioBuffer, size_t audioLength);
bool playAudioStream(Stream &audioStream, size_t audioLength);

#endif // VOICE_H
