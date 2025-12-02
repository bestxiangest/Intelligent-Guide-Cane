#include "network.h"

// ==================== 网络初始化模块 ====================
/**
 * @brief 初始化WiFi连接
 * 连接到指定的WiFi网络并等待连接成功
 */
/**
 * @brief WiFi网络诊断函数（安全版本）
 * 扫描可用网络并检查目标SSID是否存在
 */
void diagnoseWiFiConnection()
{
  Serial.println("开始WiFi网络诊断...");
  
  // 添加延时确保WiFi模块稳定
  delay(1000);
  
  // 扫描可用网络，添加错误处理
  Serial.println("扫描可用WiFi网络:");
  int networkCount = WiFi.scanNetworks(false, false, false, 300);
  
  // 检查扫描结果
  if (networkCount == WIFI_SCAN_FAILED)
  {
    Serial.println("  ✗ WiFi扫描失败，跳过网络诊断");
    return;
  }
  
  if (networkCount == 0)
  {
    Serial.println("  未发现任何WiFi网络");
    return;
  }
  
  // 限制显示的网络数量，避免内存问题
  int maxNetworks = min(networkCount, 10);
  bool targetFound = false;
  Serial.printf("  发现 %d 个网络 (显示前%d个):\n", networkCount, maxNetworks);
  
  for (int i = 0; i < maxNetworks; i++)
  {
    // 添加边界检查
    if (i >= networkCount) break;
    
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    wifi_auth_mode_t encType = WiFi.encryptionType(i);
    
    // 检查SSID是否有效
    if (ssid.length() == 0)
    {
      Serial.printf("    %d: [隐藏网络] (信号: %d dBm)\n", i + 1, rssi);
      continue;
    }
    
    Serial.printf("    %d: %s (信号: %d dBm, 加密: %s)\n", 
                  i + 1, ssid.c_str(), rssi, 
                  (encType == WIFI_AUTH_OPEN) ? "开放" : "加密");
    
    if (ssid == WIFI_SSID)
    {
      targetFound = true;
      Serial.printf("      ✓ 找到目标网络 '%s', 信号强度: %d dBm\n", WIFI_SSID, rssi);
      if (rssi < -70)
      {
        Serial.println("      ⚠ 警告: 信号强度较弱，可能影响连接");
      }
    }
    
    // 添加小延时避免看门狗超时
    delay(10);
  }
  
  if (!targetFound)
  {
    Serial.printf("  ✗ 未找到目标网络 '%s'\n", WIFI_SSID);
    Serial.println("  请检查SSID配置或网络是否开启");
  }
  
  // 安全清理扫描结果
  WiFi.scanDelete();
  delay(100); // 确保清理完成
}

void initWiFi()
{
  Serial.println("开始WiFi连接...");
  Serial.printf("  SSID: %s\n", WIFI_SSID);

  // 设置WiFi模式
  WiFi.mode(WIFI_STA);
  
  // 暂时跳过网络诊断，避免内存问题
  Serial.println("  跳过网络诊断，直接连接...");

  // 开始连接
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // 等待连接，最多等待30秒
  int attempts = 0;
  const int maxAttempts = 20;

  Serial.print("  连接中");
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    delay(1000);
    Serial.print(".");
    attempts++;
    
    // 每5秒输出当前状态
    if (attempts % 5 == 0)
    {
      Serial.printf(" [%d秒, 状态: %d] ", attempts, WiFi.status());
    }
  }
  Serial.println();

  // 检查连接结果
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("✓ WiFi连接成功");
    Serial.printf("  IP地址: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  信号强度: %d dBm\n", WiFi.RSSI());
    Serial.printf("  MAC地址: %s\n", WiFi.macAddress().c_str());
    isConnectedToWifi = true;
  }
  else
  {
    Serial.println("✗ WiFi连接失败");
    Serial.printf("  最终状态码: %d\n", WiFi.status());
    Serial.println("  状态码含义:");
    Serial.println("    0: WL_IDLE_STATUS - 空闲状态");
    Serial.println("    1: WL_NO_SSID_AVAIL - 找不到SSID");
    Serial.println("    4: WL_CONNECT_FAILED - 连接失败");
    Serial.println("    6: WL_DISCONNECTED - 已断开连接");
    Serial.println("  将在离线模式下运行");
    isConnectedToWifi = false;
  }
}


// 上传PCM数据到服务端
void uploadPcmData(String serverUrl, uint8_t *audioData, int audioDataSize)
{
    // audio数据包许愿哦进行Base64编码，数据量会增大1/3
    int audio_data_len = audioDataSize * sizeof(char) * 1.4;
    unsigned char *audioDataBase64 = (unsigned char *)ps_malloc(audio_data_len);
    if (!audioDataBase64)
    {
        Serial.println("Failed to allocate memory for audioDataBase64");
        return;
    }

    // json包大小，由于需要将audioData数据进行Base64的编码，数据量会增大1/3
    int data_json_len = audioDataSize * sizeof(char) * 1.4;
    char *data_json = (char *)ps_malloc(data_json_len);
    if (!data_json)
    {
        Serial.println("Failed to allocate memory for data_json");
        return;
    }

    // 将 PCM 数据进行 Base64 编码
    encode_base64(audioData, audioDataSize, audioDataBase64);

    memset(data_json, '\0', data_json_len); // 清空 json 包
    strcat(data_json, "{");                 // 拼接 json 包
    strcat(data_json, "\"pcm_data\":\"");
    strcat(data_json, (const char *)audioDataBase64);
    strcat(data_json, "\"");
    strcat(data_json, "}");

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // 发送 POST 请求
    int httpResponseCode = http.POST(data_json);
    // int httpResponseCode = http.POST();

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }

    // 释放内存
    if (audioDataBase64)
    {
        free(audioDataBase64);
    }

    if (data_json)
    {
        free(data_json);
    }

    http.end();
    return;
}

// 发送文本到服务端AI接口
String sendTextToServer(String text){
    String serverUrl = "http://101.132.166.117:12345/ai";

    DynamicJsonDocument jsonDocument(1024);
    jsonDocument["message"] = text;
    String jsonStr;
    serializeJson(jsonDocument, jsonStr);

    HTTPClient http;
    http.begin(serverUrl); // 开始连接
    http.addHeader("Content-Type", "application/json"); // 设置请求头
    int httpResponseCode = http.POST(jsonStr); // 发送请求
    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        if (payload.length() > 0){
            DynamicJsonDocument resDoc(1024);
            DeserializationError error = deserializeJson(resDoc, payload);
            if (!error){
                String res = resDoc["response"].as<String>();
                http.end();
                return res;
            }else{
                Serial.println("解析失败");
            }
        }
    }
    http.end();
    return "请求失败";
}

// 新增：发送GPS数据到后端
bool sendGpsData(float latitude, float longitude)
{
    if (!isConnectedToWifi)
    {
        Serial.println("WiFi未连接，无法发送GPS数据");
        return false;
    }

    String serverUrl = "http://101.132.166.117:12345/gps"; // 后端接口地址

    // 创建JSON对象
    DynamicJsonDocument jsonDoc(256); // 分配足够容纳经纬度的空间
    jsonDoc["latitude"] = latitude;
    jsonDoc["longitude"] = longitude;

    String jsonPayload;
    serializeJson(jsonDoc, jsonPayload); // 将JSON对象序列化为字符串

    HTTPClient http;
    http.begin(serverUrl);                      // 初始化HTTP连接
    http.addHeader("Content-Type", "application/json"); // 设置请求头

    Serial.print("发送GPS数据: ");
    Serial.println(jsonPayload);

    // 发送POST请求
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0)
    {
        Serial.print("GPS数据发送 HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString(); // 获取响应内容（可选）
        Serial.println(payload);           // 打印响应内容（可选）
    }
    else
    {
        Serial.print("GPS数据发送失败，Error code: ");
        Serial.println(httpResponseCode);
        Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end(); // 关闭连接
    return true;
}

