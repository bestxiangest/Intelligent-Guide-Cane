#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ArduinoJson.h>

#include "config.h"     // 项目配置文件

void initWiFi();     // 初始化WiFi连接

// 上传PCM数据到服务端
void uploadPcmData(String serverUrl, uint8_t *audioData, int audioDataSize);
// 发送文本到服务端AI接口
String sendTextToServer(String text);
void sendGpsData(float latitude, float longitude); // 新增：发送GPS数据的函数声明
#endif