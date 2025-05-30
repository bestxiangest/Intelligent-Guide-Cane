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

// 百度API凭据
const String client_id = BAIDU_CLIENT_ID;
const String client_secret = BAIDU_CLIENT_SECRET;


//初始化I2S
void set_i2s();

// Play audio data using MAX98357A
// 播放声音
// audioData: PCM原始数据的指针
// audioDataSize: size of PCM data
void playAudio(uint8_t* audioData, size_t audioDataSize);

// 清空I2S DMA缓冲区
void clearAudio(void);

// Play audio data using MAX98357A
// decode_data: base64解码后的数据
// note: base64编码的音频数据
void playAudio_fromBase64(uint8_t* decode_data, const char* note);

// 获取百度云平台的AccessToken
String getAccessToken_baidu();
// 百度语音识别
// accessToken: 百度云平台的AccessToken
// audioData: PCM原始数据的指针
// audioDataSize: size of PCM data
String baidu_voice_recognition(String accessToken, uint8_t *audioData, int audioDataSize);
String send_to_AI(String message);
void baiduTTS_Send(String access_token, String text);

#endif // VOICE_H