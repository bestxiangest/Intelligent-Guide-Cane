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
#include "weather.h"

// I2S配置 (用于高质量音频录制)
#define I2S_WS_PIN 12
#define I2S_SCK_PIN 13
#define I2S_SD_PIN MIC_PIN

// 录音缓冲区大小
#define RECORD_BUFFER_SIZE 8192

// 语音识别API端点 (示例使用百度语音识别API)
#define SPEECH_API_ENDPOINT "https://vop.baidu.com/server_api"

// 大模型API端点 (示例使用通用API格式)
#define AI_MODEL_ENDPOINT "http://localhost:3001/api/v1/workspace/532b7ee0-3f6a-4f8c-982d-2fcae7d4597b/chat"
#define AI_MODEL_API_KEY "AZFAJKM-K58MAEK-HXXX4F1-3E4JE8Y"

// 函数声明
bool initWiFi();                                                         // WiFi初始化函数
bool initAudio();                                                        // 音频初始化函数
bool recordAudio(uint8_t *buffer, size_t *length, uint32_t duration_ms); // 录音函数
String speechToText(uint8_t *audioData, size_t length);                  // 语音识别函数
String queryAIModel(String text);                                        // 大模型查询函数
void playResponse(String response);                                      // 播放语音响应函数
String getBaiduAccessToken();                                            // 获取百度语音API的访问令牌
String baiduSpeechToText(uint8_t *audioData, size_t length);             // 使用百度语音API进行语音识别

// WiFi初始化函数
bool initWiFi()
{
  Serial.println("正在连接WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // 等待连接，最多等待20秒
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout > 0)
  {
    delay(1000);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi连接成功!");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  else
  {
    Serial.println("\nWiFi连接失败!");
    return false;
  }
}

// 音频初始化函数
bool initAudio()
{
  // 配置I2S进行音频录制
  I2S.setAllPins(I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN, -1, -1);

  // 配置I2S为录音模式，使用PDM麦克风
  if (!I2S.begin(PDM_MONO_MODE, VOICE_SAMPLE_RATE, 16))
  {
    Serial.println("I2S初始化失败!");
    return false;
  }

  Serial.println("音频系统初始化成功");
  return true;
}

// 录音函数
bool recordAudio(uint8_t *buffer, size_t *length, uint32_t duration_ms)
{
  if (buffer == NULL || length == NULL)
  {
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
  while (bytesRead < RECORD_BUFFER_SIZE && (millis() - startTime) < duration_ms)
  {
    // 使用标准库的min函数
    size_t bytesToRead = std::min(RECORD_BUFFER_SIZE - bytesRead, (size_t)1024);
    size_t read = I2S.read(buffer + bytesRead, bytesToRead);

    if (read > 0)
    {
      bytesRead += read;
    }
  }

  *length = bytesRead;
  Serial.printf("录音完成，共录制 %u 字节\n", bytesRead);

  return (bytesRead > 0);
}

// 语音识别函数 (将音频转换为文本)
String speechToText(uint8_t *audioData, size_t length)
{
  if (WiFi.status() != WL_CONNECTED)
  {
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

  if (httpCode == HTTP_CODE_OK)
  {
    response = http.getString();
    Serial.println("语音识别响应: " + response);

    // 解析响应JSON
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error)
    {
      if (responseDoc.containsKey("result") && responseDoc["err_no"] == 0)
      {
        String recognizedText = responseDoc["result"][0];
        Serial.println("识别结果: " + recognizedText);
        return recognizedText;
      }
    }
  }
  else
  {
    Serial.printf("语音识别请求失败，错误代码: %d\n", httpCode);
  }

  http.end();
  return "";
}

// 大模型查询函数
String queryAIModel(String text)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi未连接，无法查询大模型");
    return "网络连接失败，请稍后再试";
  }

  Serial.println("正在查询AnythingLLM大模型...");

  HTTPClient http;
  http.begin("http://localhost:3001/api/v1/workspace/<Your Key>/chat");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("accept", "application/json");
  http.addHeader("Authorization", "Bearer <Your Key>");

  // 构建请求JSON
  StaticJsonDocument<2048> doc;
  doc["message"] = text;
  doc["mode"] = "chat";

  // 使用设备ID作为会话ID，确保对话连续性
  String deviceId = String(ESP.getEfuseMac(), HEX);
  doc["sessionId"] = "esp32-" + deviceId;

  String requestBody;
  serializeJson(doc, requestBody);

  Serial.println("发送请求: " + requestBody);

  // 发送请求
  int httpCode = http.POST(requestBody);
  String response = "";

  if (httpCode == HTTP_CODE_OK)
  {
    response = http.getString();
    Serial.println("大模型响应: " + response);

    // 解析响应JSON
    StaticJsonDocument<4096> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error)
    {
      // 检查是否有错误
      if (responseDoc.containsKey("error") && responseDoc["error"] != nullptr && responseDoc["error"] != "null")
      {
        String errorMsg = responseDoc["error"];
        Serial.println("大模型返回错误: " + errorMsg);
        return "系统错误: " + errorMsg;
      }

      // 获取文本响应
      if (responseDoc.containsKey("textResponse"))
      {
        String aiResponse = responseDoc["textResponse"];
        Serial.println("AI回复: " + aiResponse);

        // 处理特殊指令
        if (aiResponse.startsWith("导航_"))
        {
          String destination = aiResponse.substring(3); // 去掉"导航_"前缀
          Serial.println("检测到导航指令，目的地: " + destination);

          // 获取当前位置
          String currentLocation = getCurrentLocation();

          // 地理编码获取目的地坐标
          GeocodingResult geocodeResult = geocodeAddress(destination);

          if (geocodeResult.location.length() > 0)
          {
            // 获取导航路线
            currentRoute = getWalkingRoute(currentLocation, geocodeResult.location);

            if (currentRoute.steps.size() > 0)
            {
              navigationActive = true;
              currentStepIndex = 0;

              // 播报导航指令
              speakNavigationInstructions(currentRoute);

              return "正在为您导航到" + destination + "，请听取语音指引";
            }
            else
            {
              return "抱歉，无法规划到" + destination + "的路线";
            }
          }
          else
          {
            return "抱歉，找不到" + destination + "的位置信息";
          }
        }
        else if (aiResponse.startsWith("天气_"))
        {
          // 解析天气查询指令
          int firstUnderscorePos = aiResponse.indexOf('_');
          int secondUnderscorePos = aiResponse.indexOf('_', firstUnderscorePos + 1);

          if (firstUnderscorePos > 0 && secondUnderscorePos > firstUnderscorePos)
          {
            String city = aiResponse.substring(firstUnderscorePos + 1, secondUnderscorePos);
            String timeFrame = aiResponse.substring(secondUnderscorePos + 1);

            Serial.println("检测到天气查询指令，城市: " + city + ", 时间: " + timeFrame);

            // 调用天气API获取实际天气数据
            WeatherResult weatherData = getWeather(city);
            
            // 根据时间帧选择天气信息
            int dayIndex = 0;
            if (timeFrame.indexOf("明天") >= 0) {
              dayIndex = 1;
            } else if (timeFrame.indexOf("后天") >= 0) {
              dayIndex = 2;
            }
            
            // 转换为语音播报文本
            return weatherToSpeech(weatherData, dayIndex);
          }
        }

        // 如果不是特殊指令，直接返回AI回复
        return aiResponse;
      }
    }
    else
    {
      Serial.println("JSON解析错误: " + String(error.c_str()));
    }
  }
  else
  {
    Serial.printf("大模型请求失败，错误代码: %d\n", httpCode);
    Serial.println("错误响应: " + http.getString());
  }

  http.end();
  return "抱歉，我无法理解您的问题，请重试";
}
// 播放语音响应 (简化版，实际项目中应使用TTS或预录制语音)
void playResponse(String response)
{
  // 在实际项目中，这里应该调用TTS服务将文本转换为语音
  // 然后通过I2S或DAC播放出来
  // 简化版仅打印响应
  Serial.println("语音播放: " + response);

  // 可以通过蜂鸣器发出提示音，表示有回复
  tone(BUZZER_PIN, 1000, 100);
  delay(200);
  tone(BUZZER_PIN, 1200, 100);
}

// 获取百度语音API的访问令牌
String getBaiduAccessToken()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi未连接，无法获取访问令牌");
    return "";
  }

  Serial.println("正在获取百度语音API访问令牌...");

  HTTPClient http;
  http.begin("https://aip.baidubce.com/oauth/2.0/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Accept", "application/json");

  String requestData = "grant_type=client_credentials&client_id=" + String(BAIDU_API_KEY) +
                       "&client_secret=" + String(BAIDU_SECRET_KEY);

  int httpCode = http.POST(requestData);
  String result = "";

  if (httpCode == HTTP_CODE_OK)
  {
    result = http.getString();
    Serial.println("获取访问令牌响应: " + result);

    // 解析JSON响应
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, result);

    if (!error && doc.containsKey("access_token"))
    {
      String accessToken = doc["access_token"].as<String>();
      Serial.println("成功获取访问令牌: " + accessToken);
      return accessToken;
    }
  }
  else
  {
    Serial.printf("获取访问令牌失败，错误代码: %d\n", httpCode);
  }

  http.end();
  return "";
}

// 使用百度语音API进行语音识别
String baiduSpeechToText(uint8_t *audioData, size_t length)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi未连接，无法进行语音识别");
    return "";
  }

  Serial.println("正在进行百度语音识别...");

  // 获取访问令牌
  String accessToken = getBaiduAccessToken();
  if (accessToken.isEmpty())
  {
    Serial.println("无法获取访问令牌，语音识别失败");
    return "";
  }

  HTTPClient http;
  http.begin("https://vop.baidu.com/server_api");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  // 将音频数据编码为Base64
  String base64Audio = base64::encode(audioData, length);

  // 构建请求JSON
  StaticJsonDocument<16384> doc;
  doc["format"] = "pcm"; // 根据实际录音格式调整
  doc["rate"] = VOICE_SAMPLE_RATE;
  doc["channel"] = 1;
  doc["cuid"] = BAIDU_CUID;
  doc["token"] = accessToken;
  doc["speech"] = base64Audio;
  doc["len"] = length;

  String requestBody;
  serializeJson(doc, requestBody);

  // 发送请求
  int httpCode = http.POST(requestBody);
  String response = "";

  if (httpCode == HTTP_CODE_OK)
  {
    response = http.getString();
    Serial.println("语音识别响应: " + response);

    // 解析响应JSON
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error && responseDoc["err_no"] == 0)
    {
      if (responseDoc.containsKey("result") && responseDoc["result"].size() > 0)
      {
        String recognizedText = responseDoc["result"][0];
        Serial.println("识别结果: " + recognizedText);
        return recognizedText;
      }
    }
    else
    {
      Serial.println("语音识别解析失败: " + String(responseDoc["err_msg"].as<String>()));
    }
  }
  else
  {
    Serial.printf("语音识别请求失败，错误代码: %d\n", httpCode);
  }

  http.end();
  return "";
}
#endif // VOICE_H