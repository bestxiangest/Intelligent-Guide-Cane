#include "network.h"

// 初始化WiFi连接
void initWiFi()
{
    Serial.print("连接到WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 等待连接，最多等待20秒
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(1000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi连接成功!");
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
        isConnectedToWifi = true;
    }
    else
    {
        Serial.println("\nWiFi连接失败，系统将以离线模式运行");
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
void sendGpsData(float latitude, float longitude)
{
    if (!isConnectedToWifi)
    {
        Serial.println("WiFi未连接，无法发送GPS数据");
        return;
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
}

