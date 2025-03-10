/*
 * 智能导盲杖 - 天气模块
 * 使用心知天气API获取天气信息
 */

#ifndef WEATHER_H
#define WEATHER_H

#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// 天气信息结构体
typedef struct {
  String date;           // 日期
  String text_day;       // 白天天气文字描述
  String text_night;     // 夜间天气文字描述
  int high;              // 最高温度
  int low;               // 最低温度
  String wind_direction; // 风向
  String wind_scale;     // 风力等级
  String humidity;       // 湿度
} WeatherInfo;

// 天气查询结果结构体
typedef struct {
  String city;                  // 城市名称
  String country;               // 国家
  String last_update;           // 最后更新时间
  std::vector<WeatherInfo> forecast; // 天气预报
} WeatherResult;

// 获取天气信息
WeatherResult getWeather(String location) {
  WeatherResult result;
  result.city = "";
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法获取天气信息");
    return result;
  }
  
  Serial.println("正在获取天气信息: " + location);
  
  HTTPClient http;
  // URL编码城市名称
  String encodedLocation = "";
  for (int i = 0; i < location.length(); i++) {
    char c = location.charAt(i);
    if (c == ' ') {
      encodedLocation += "%20";
    } else if (c >= 0x80) {
      // 简单处理中文字符，实际应使用完整的URL编码
      char hex[7];
      sprintf(hex, "%%%.2X%%%.2X%%%.2X", (unsigned char)location.charAt(i), 
              (unsigned char)location.charAt(i+1), (unsigned char)location.charAt(i+2));
      encodedLocation += hex;
      i += 2;
    } else {
      encodedLocation += c;
    }
  }
  
  String url = "https://api.seniverse.com/v3/weather/daily.json?key=" + String(WEATHER_API_KEY) + 
               "&location=" + encodedLocation + "&language=zh-Hans&unit=c&start=0&days=3";
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("天气API响应: " + response);
    
    // 解析JSON响应
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc.containsKey("results") && doc["results"].size() > 0) {
      JsonObject location = doc["results"][0]["location"];
      result.city = location["name"].as<String>();
      result.country = location["country"].as<String>();
      result.last_update = doc["results"][0]["last_update"].as<String>();
      
      // 解析天气预报
      JsonArray daily = doc["results"][0]["daily"];
      for (JsonObject day : daily) {
        WeatherInfo info;
        info.date = day["date"].as<String>();
        info.text_day = day["text_day"].as<String>();
        info.text_night = day["text_night"].as<String>();
        info.high = day["high"].as<int>();
        info.low = day["low"].as<int>();
        info.wind_direction = day["wind_direction"].as<String>();
        info.wind_scale = day["wind_scale"].as<String>();
        info.humidity = day["humidity"].as<String>();
        
        result.forecast.push_back(info);
      }
      
      Serial.println("天气信息获取成功: " + result.city);
    } else {
      Serial.println("天气信息解析失败");
    }
  } else {
    Serial.printf("天气请求失败，错误代码: %d\n", httpCode);
  }
  
  http.end();
  return result;
}

// 将天气信息转换为语音播报文本
String weatherToSpeech(WeatherResult weather, int day = 0) {
  if (weather.city.isEmpty() || weather.forecast.size() <= day) {
    return "抱歉，无法获取天气信息";
  }
  
  WeatherInfo info = weather.forecast[day];
  String dayText = "";
  
  switch (day) {
    case 0:
      dayText = "今天";
      break;
    case 1:
      dayText = "明天";
      break;
    case 2:
      dayText = "后天";
      break;
    default:
      dayText = "未来几天";
  }
  
  String speech = weather.city + dayText + "天气：" + info.text_day + "，";
  speech += "气温" + String(info.low) + "到" + String(info.high) + "度，";
  speech += info.wind_direction + "风" + info.wind_scale + "级，";
  speech += "湿度" + info.humidity + "%。";
  
  if (day == 0) {
    // 添加一些实用建议
    if (info.text_day.indexOf("雨") >= 0) {
      speech += "外出请带伞。";
    } else if (info.high >= 30) {
      speech += "天气炎热，注意防暑。";
    } else if (info.low <= 5) {
      speech += "天气寒冷，注意保暖。";
    }
    
    if (info.text_day.indexOf("雾") >= 0 || info.text_day.indexOf("霾") >= 0) {
      speech += "能见度较低，外出请小心。";
    }
  }
  
  return speech;
}

#endif // WEATHER_H