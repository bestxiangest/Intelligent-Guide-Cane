/*
 * 智能导盲杖 - 语音处理模块
 * 包含录音、语音识别和大模型交互功能
 */

#ifndef VOICE_H
#define VOICE_H

#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <I2S.h>

// I2S配置 (用于高质量音频录制)
#define I2S_WS_PIN  12
#define I2S_SCK_PIN 13
#define I2S_SD_PIN  MIC_PIN

// 录音缓冲区大小
#define RECORD_BUFFER_SIZE 8192

// 语音识别API端点 (示例使用百度语音识别API)
#define SPEECH_API_ENDPOINT "https://vop.baidu.com/server_api"

// 大模型API端点 (示例使用通用API格式)
#define AI_MODEL_ENDPOINT "https://api.example.com/v1/chat/completions"

// 函数声明
bool initWiFi();
bool initAudio();
bool recordAudio(uint8_t* buffer, size_t* length, uint32_t duration_ms);
String speechToText(uint8_t* audioData, size_t length);
String queryAIModel(String text);
void playResponse(String response);

// WiFi初始化函数
bool initWiFi() {
  Serial.println("正在连接WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // 等待连接，最多等待20秒
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    Serial.print(".");
    timeout--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi连接成功!");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nWiFi连接失败!");
    return false;
  }
}

// 音频初始化函数
bool initAudio() {
  // 配置I2S进行音频录制
  I2S.setAllPins(I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN, -1, -1);
  
  // 配置I2S为录音模式，使用PDM麦克风
  if (!I2S.begin(PDM_MONO_MODE, VOICE_SAMPLE_RATE, 16)) {
    Serial.println("I2S初始化失败!");
    return false;
  }
  
  Serial.println("音频系统初始化成功");
  return true;
}

// 录音函数
bool recordAudio(uint8_t* buffer, size_t* length, uint32_t duration_ms) {
  if (buffer == NULL || length == NULL) {
    return false;
  }
  
  // 清空缓冲区
  memset(buffer, 0, RECORD_BUFFER_SIZE);
  *length = 0;
  
  Serial.println("开始录音...");
  
  // 计算需要录制的样本数
  size_t samples = (VOICE_SAMPLE_RATE * duration_ms) / 1000;
  size_t bytesRead = 0;
  uint32_t startTime = millis();
  
  // 录制指定时长的音频
  while (bytesRead < RECORD_BUFFER_SIZE && (millis() - startTime) < duration_ms) {
    // 使用标准库的min函数
    size_t bytesToRead = std::min(RECORD_BUFFER_SIZE - bytesRead, (size_t)1024);
    size_t read = I2S.read(buffer + bytesRead, bytesToRead);
    
    if (read > 0) {
      bytesRead += read;
    }
  }
  
  *length = bytesRead;
  Serial.printf("录音完成，共录制 %u 字节\n", bytesRead);
  
  return (bytesRead > 0);
}

// 语音识别函数 (将音频转换为文本)
String speechToText(uint8_t* audioData, size_t length) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法进行语音识别");
    return "";
  }
  
  Serial.println("正在进行语音识别...");
  
  HTTPClient http;
  http.begin(SPEECH_API_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  
  // 将音频数据编码为Base64
  String base64Audio = base64::encode(audioData, length);
  
  // 构建请求JSON
  StaticJsonDocument<16384> doc;
  doc["format"] = "pcm";
  doc["rate"] = VOICE_SAMPLE_RATE;
  doc["channel"] = 1;
  doc["token"] = SPEECH_API_KEY;
  doc["speech"] = base64Audio;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  // 发送请求
  int httpCode = http.POST(requestBody);
  String response = "";
  
  if (httpCode == HTTP_CODE_OK) {
    response = http.getString();
    Serial.println("语音识别响应: " + response);
    
    // 解析响应JSON
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("result") && responseDoc["err_no"] == 0) {
        String recognizedText = responseDoc["result"][0];
        Serial.println("识别结果: " + recognizedText);
        return recognizedText;
      }
    }
  } else {
    Serial.printf("语音识别请求失败，错误代码: %d\n", httpCode);
  }
  
  http.end();
  return "";
}

// 大模型查询函数
String queryAIModel(String text) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法查询大模型");
    return "网络连接失败，请稍后再试";
  }
  
  Serial.println("正在查询大模型...");
  
  HTTPClient http;
  http.begin(AI_MODEL_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(AI_MODEL_API_KEY));
  
  // 构建请求JSON
  StaticJsonDocument<1024> doc;
  doc["model"] = "gpt-3.5-turbo";
  
  JsonArray messages = doc.createNestedArray("messages");
  
  JsonObject systemMessage = messages.createNestedObject();
  systemMessage["role"] = "system";
  systemMessage["content"] = "你是一个智能导盲杖上的AI助手，请简洁明了地回答问题，帮助视障人士获取信息。";
  
  JsonObject userMessage = messages.createNestedObject();
  userMessage["role"] = "user";
  userMessage["content"] = text;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  // 发送请求
  int httpCode = http.POST(requestBody);
  String response = "";
  
  if (httpCode == HTTP_CODE_OK) {
    response = http.getString();
    Serial.println("大模型响应: " + response);
    
    // 解析响应JSON
    StaticJsonDocument<4096> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("choices") && responseDoc["choices"].size() > 0) {
        String aiResponse = responseDoc["choices"][0]["message"]["content"];
        Serial.println("AI回复: " + aiResponse);
        return aiResponse;
      }
    }
  } else {
    Serial.printf("大模型请求失败，错误代码: %d\n", httpCode);
  }
  
  http.end();
  return "抱歉，我无法理解您的问题，请重试";
}

// 播放语音响应 (简化版，实际项目中应使用TTS或预录制语音)
void playResponse(String response) {
  // 在实际项目中，这里应该调用TTS服务将文本转换为语音
  // 然后通过I2S或DAC播放出来
  // 简化版仅打印响应
  Serial.println("语音播放: " + response);
  
  // 可以通过蜂鸣器发出提示音，表示有回复
  tone(BUZZER_PIN, 1000, 100);
  delay(200);
  tone(BUZZER_PIN, 1200, 100);
}

#endif // VOICE_H