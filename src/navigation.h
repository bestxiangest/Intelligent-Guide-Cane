/*
 * 智能导盲杖 - 导航模块
 * 包含地理编码和步行导航功能
 */

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>

// 导航指令结构体
typedef struct {
  String instruction;  // 导航指令
  String orientation;  // 方向
  String roadName;     // 道路名称
  int distance;        // 距离(米)
} NavigationStep;

// 导航路线结构体
typedef struct {
  std::vector<NavigationStep> steps;  // 导航步骤
  int totalDistance;                  // 总距离(米)
  int totalDuration;                  // 总时间(秒)
} NavigationRoute;

// 地理编码结果结构体
typedef struct {
  String formattedAddress;  // 格式化地址
  String location;          // 经纬度坐标
} GeocodingResult;

// 函数声明
GeocodingResult geocodeAddress(String address);
NavigationRoute getWalkingRoute(String origin, String destination);
String getCurrentLocation();
void speakNavigationInstructions(NavigationRoute route);

// 地理编码函数 - 将地址转换为经纬度坐标
GeocodingResult geocodeAddress(String address) {
  GeocodingResult result = {"", ""};
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法进行地理编码");
    return result;
  }
  
  Serial.println("正在进行地理编码: " + address);
  
  HTTPClient http;
  // URL编码地址
  String encodedAddress = "";
  for (int i = 0; i < address.length(); i++) {
    char c = address.charAt(i);
    if (c == ' ') {
      encodedAddress += "%20";
    } else if (c >= 0x80) {
      // 简单处理中文字符，实际应使用完整的URL编码
      char hex[7];
      sprintf(hex, "%%%.2X%%%.2X%%%.2X", (unsigned char)address.charAt(i), 
              (unsigned char)address.charAt(i+1), (unsigned char)address.charAt(i+2));
      encodedAddress += hex;
      i += 2;
    } else {
      encodedAddress += c;
    }
  }
  
  String url = "https://restapi.amap.com/v3/geocode/geo?address=" + encodedAddress + 
               "&output=json&key=" + String(AMAP_API_KEY);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("地理编码响应: " + response);
    
    // 解析JSON响应
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc["status"] == "1" && doc["count"].as<int>() > 0) {
      result.formattedAddress = doc["geocodes"][0]["formatted_address"].as<String>();
      result.location = doc["geocodes"][0]["location"].as<String>();
      Serial.println("地理编码成功: " + result.formattedAddress + ", 坐标: " + result.location);
    } else {
      Serial.println("地理编码解析失败");
    }
  } else {
    Serial.printf("地理编码请求失败，错误代码: %d\n", httpCode);
  }
  
  http.end();
  return result;
}

// 获取步行导航路线
NavigationRoute getWalkingRoute(String origin, String destination) {
  NavigationRoute route;
  route.totalDistance = 0;
  route.totalDuration = 0;
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法获取导航路线");
    return route;
  }
  
  Serial.println("正在获取步行导航路线");
  Serial.println("起点: " + origin);
  Serial.println("终点: " + destination);
  
  HTTPClient http;
  String url = "https://restapi.amap.com/v5/direction/walking?isindoor=0&origin=" + origin + 
               "&destination=" + destination + "&key=" + String(AMAP_API_KEY);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("导航响应: " + response);
    
    // 解析JSON响应
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc["status"] == "1" && doc["route"]["paths"].size() > 0) {
      JsonObject path = doc["route"]["paths"][0];
      route.totalDistance = path["distance"].as<int>();
      route.totalDuration = path["cost"]["duration"].as<int>();
      
      // 解析导航步骤
      for (JsonObject step : path["steps"].as<JsonArray>()) {
        NavigationStep navStep;
        navStep.instruction = step["instruction"].as<String>();
        navStep.orientation = step["orientation"].as<String>();
        navStep.roadName = step["road_name"].as<String>();
        navStep.distance = step["step_distance"].as<int>();
        
        route.steps.push_back(navStep);
      }
      
      Serial.printf("导航路线获取成功，总距离: %d米，预计时间: %d秒，共%d个步骤\n", 
                    route.totalDistance, route.totalDuration, route.steps.size());
    } else {
      Serial.println("导航路线解析失败");
    }
  } else {
    Serial.printf("导航请求失败，错误代码: %d\n", httpCode);
  }
  
  http.end();
  return route;
}

// 获取当前位置 (在实际项目中应使用GPS模块)
String getCurrentLocation() {
  // 在实际项目中，这里应该从GPS模块获取当前位置
  // 现在使用默认位置
  return String(DEFAULT_LOCATION);
}

// 语音播报导航指令
void speakNavigationInstructions(NavigationRoute route) {
  if (route.steps.size() == 0) {
    // playResponse("抱歉，无法获取导航路线");
    Serial.println("抱歉，无法获取导航路线");
    return;
  }
  
  // 播报总体信息
  String overviewMsg = "导航路线已规划，总距离" + String(route.totalDistance) + 
                       "米，预计步行时间" + String(route.totalDuration / 60) + "分钟";
//   playResponse(overviewMsg);
  Serial.println(overviewMsg);
  
  // 播报第一步指令
  if (route.steps.size() > 0) {
    NavigationStep firstStep = route.steps[0];
    String firstInstruction = "第一步: " + firstStep.instruction + 
                             "，距离" + String(firstStep.distance) + "米";
    // playResponse(firstInstruction);
    Serial.println(firstInstruction);
  }
}

#endif // NAVIGATION_H