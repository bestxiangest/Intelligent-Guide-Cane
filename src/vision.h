/*
 * 智能导盲杖 - 视觉处理模块
 * 包含盲道检测和红绿灯识别功能
 */

#ifndef VISION_H
#define VISION_H

#include "config.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"  // 添加队列支持
#include <HTTPClient.h>
#include <WiFiClient.h>

// 摄像头引脚定义 (ESP32-S3-EYE)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     45
#define SIOD_GPIO_NUM     1
#define SIOC_GPIO_NUM     2

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       46
#define Y7_GPIO_NUM       8
#define Y6_GPIO_NUM       7
#define Y5_GPIO_NUM       4
#define Y4_GPIO_NUM       41
#define Y3_GPIO_NUM       40
#define Y2_GPIO_NUM       39
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     42
#define PCLK_GPIO_NUM     5

// 远程视觉处理服务器API
#define VISION_API_ENDPOINT "https://dmz.sharpcaterpillar.top/yunsuan"

// 图像压缩质量 (1-100)，数值越低压缩率越高
#define JPEG_QUALITY 30

// 图像上传间隔 (毫秒)
#define IMAGE_UPLOAD_INTERVAL 2000

// 视觉处理结果结构体
typedef struct {
  bool success;           // 处理是否成功
  int resultType;         // 结果类型: 1=盲道检测, 2=红绿灯检测, 3=障碍物检测
  int status;             // 状态码: 盲道(0=未检测到, 1=左侧, 2=中间, 3=右侧)
                          //        红绿灯(0=未检测到, 1=红灯, 2=绿灯, 3=黄灯)
                          //        障碍物(0=无障碍, 1=有障碍)
  int confidence;         // 置信度 (0-100)
  int distance;           // 距离 (厘米，仅适用于障碍物)
  int remainingTime;      // 剩余时间 (秒，仅适用于红绿灯)
  String message;         // 附加信息
} VisionResult;

// 红绿灯状态结构体 (与主程序中定义一致)
typedef struct {
  uint8_t status;  // 0: 未知, 1: 红灯, 2: 绿灯, 3: 黄灯
  uint8_t remainingTime; // 剩余时间(秒)
} TrafficLightStatus;

// 障碍物检测结构体 (与主程序中定义一致)
typedef struct {
  uint8_t type;  // 0: 无障碍, 1: 前方障碍, 2: 左侧障碍, 3: 右侧障碍
  uint16_t distance; // 障碍物距离(cm)
  uint8_t priority;  // 警报优先级
  uint8_t data;
} ObstacleAlert;

// 函数声明
bool initCamera();
void detectTactilePaving(camera_fb_t *fb, QueueHandle_t alertQueue);
TrafficLightStatus detectTrafficLight(camera_fb_t *fb);
int estimateRemainingTime(camera_fb_t *fb, uint8_t lightStatus);

bool frameToJpeg(camera_fb_t *fb, uint8_t **jpgBuf, size_t *jpgLen);

// 摄像头初始化函数
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  
  // 初始化低分辨率，提高处理速度
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 2;

  // 初始化摄像头
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("摄像头初始化失败，错误代码: 0x%x", err);
    return false;
  }
  
  Serial.println("摄像头初始化成功");
  return true;
}

// 将图像转换为JPEG格式
bool frameToJpeg(camera_fb_t *fb, uint8_t **jpgBuf, size_t *jpgLen) {
  bool ret = false;
  
  // 如果已经是JPEG格式，直接使用
  if (fb->format == PIXFORMAT_JPEG) {
    *jpgBuf = fb->buf;
    *jpgLen = fb->len;
    return true;
  }
  
  // 使用ESP32内置的图像转换函数
  if (fb->format == PIXFORMAT_RGB565) {
    Serial.println("将RGB565转换为JPEG...");
    
    // 分配JPEG缓冲区 (预估大小)
    size_t out_buf_len = fb->width * fb->height / 2; // 保守估计
    uint8_t *out_buf = (uint8_t *)malloc(out_buf_len);
    if (!out_buf) {
      Serial.println("分配JPEG缓冲区失败");
      return false;
    }
    
    // 使用ESP32的图像转换函数
    bool converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, 
                            PIXFORMAT_RGB565, JPEG_QUALITY, &out_buf,
                            &out_buf_len);
    
    if (converted) {
      *jpgBuf = out_buf;
      *jpgLen = out_buf_len;
      ret = true;
      Serial.printf("图像转换成功，JPEG大小: %u 字节\n", out_buf_len);
    } else {
      Serial.println("JPEG编码失败");
      free(out_buf);
    }
    
    return ret;
  }
  
  Serial.println("不支持的图像格式");
  return false;
}

// 盲道检测函数
void detectTactilePaving(camera_fb_t *fb, QueueHandle_t alertQueue) {
  // 这里将实现盲道检测算法
  // 1. 将图像转换为HSV色彩空间
  // 2. 根据黄色阈值提取盲道区域
  // 3. 检测盲道位置和方向
  // 4. 如果偏离盲道，发送警报
  
  // 简化版实现：检测图像下半部分是否有足够的黄色像素
  // 实际项目中应使用更复杂的计算机视觉算法
  
  // 示例代码：
  int yellowPixelCount = 0;
  int threshold = (fb->width * fb->height / 4) * 0.2; // 下半部分20%的像素为黄色视为在盲道上
  
  // 检测图像下半部分的黄色像素
  uint8_t *buf = fb->buf;
  for (int y = fb->height/2; y < fb->height; y++) {
    for (int x = 0; x < fb->width; x++) {
      // 对于RGB565格式，每个像素占用2个字节
      uint16_t pixel = buf[y * fb->width * 2 + x * 2] | (buf[y * fb->width * 2 + x * 2 + 1] << 8);
      
      // 从RGB565提取RGB分量
      uint8_t r = (pixel >> 11) & 0x1F;
      uint8_t g = (pixel >> 5) & 0x3F;
      uint8_t b = pixel & 0x1F;
      
      // 简单的黄色检测 (高R值，高G值，低B值)
      if (r > 20 && g > 30 && b < 12) {
        yellowPixelCount++;
      }
    }
  }
  
  // 如果黄色像素不足，可能偏离盲道
  if (yellowPixelCount < threshold) {
    ObstacleAlert alert;
    alert.type = 4; // 4: 偏离盲道
    alert.distance = 0; // 不适用
    alert.priority = 2; // 中优先级
    
    // 发送到警报队列
    xQueueSend(alertQueue, &alert, 0);
  }
}

// 红绿灯检测函数
TrafficLightStatus detectTrafficLight(camera_fb_t *fb) {
  TrafficLightStatus result = {0, 0}; // 默认未知状态
  
  // 这里将实现红绿灯检测算法
  // 1. 在图像中寻找圆形区域
  // 2. 分析圆形区域的颜色
  // 3. 根据颜色判断红绿灯状态
  
  // 简化版实现：统计图像上部区域的红/绿/黄像素数量
  int redPixels = 0, greenPixels = 0, yellowPixels = 0;
  
  // 检测图像上部的颜色像素
  uint8_t *buf = fb->buf;
  for (int y = 0; y < fb->height/3; y++) { // 只检测上部1/3区域
    for (int x = 0; x < fb->width; x++) {
      // 对于RGB565格式，每个像素占用2个字节
      uint16_t pixel = buf[y * fb->width * 2 + x * 2] | (buf[y * fb->width * 2 + x * 2 + 1] << 8);
      
      // 从RGB565提取RGB分量
      uint8_t r = (pixel >> 11) & 0x1F;
      uint8_t g = (pixel >> 5) & 0x3F;
      uint8_t b = pixel & 0x1F;
      
      // 简单的颜色检测
      if (r > 25 && g < 15 && b < 15) { // 红色
        redPixels++;
      } else if (r < 15 && g > 30 && b < 15) { // 绿色
        greenPixels++;
      } else if (r > 25 && g > 30 && b < 15) { // 黄色
        yellowPixels++;
      }
    }
  }
  
  // 根据像素数量判断红绿灯状态
  int threshold = (fb->width * fb->height / 3) * 0.05; // 5%的像素
  
  if (redPixels > threshold && redPixels > greenPixels && redPixels > yellowPixels) {
    result.status = 1; // 红灯
    result.remainingTime = estimateRemainingTime(fb, 1);
  } else if (greenPixels > threshold && greenPixels > redPixels && greenPixels > yellowPixels) {
    result.status = 2; // 绿灯
    result.remainingTime = estimateRemainingTime(fb, 2);
  } else if (yellowPixels > threshold && yellowPixels > redPixels && yellowPixels > greenPixels) {
    result.status = 3; // 黄灯
    result.remainingTime = estimateRemainingTime(fb, 3);
  }
  
  return result;
}

// 估算红绿灯剩余时间
int estimateRemainingTime(camera_fb_t *fb, uint8_t lightStatus) {
  // 实际项目中，可以通过以下方式估算剩余时间：
  // 1. 检测数字倒计时显示（如果有）
  // 2. 基于历史数据和机器学习预测
  // 3. 使用固定的标准时间（如红灯30秒，绿灯40秒等）
  
  // 简化版实现：返回固定值
  switch (lightStatus) {
    case 1: // 红灯
      return 30; // 假设红灯剩余30秒
    case 2: // 绿灯
      return 40; // 假设绿灯剩余40秒
    case 3: // 黄灯
      return 5;  // 假设黄灯剩余5秒
    default:
      return 0;
  }
}

// 上传图像到远程服务器并获取处理结果
VisionResult uploadImageForProcessing(camera_fb_t *fb) {
  VisionResult result = {false, 0, 0, 0, 0, 0, ""};
  
  if (!fb) {
    Serial.println("无效的图像帧");
    return result;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法上传图像");
    return result;
  }
  
  Serial.println("准备上传图像...");
  
  // 将图像转换为JPEG格式
  uint8_t *jpgBuf = NULL;
  size_t jpgLen = 0;
  
  if (!frameToJpeg(fb, &jpgBuf, &jpgLen)) {
    Serial.println("图像转换失败");
    return result;
  }
  
  Serial.printf("图像压缩完成，大小: %u 字节\n", jpgLen);
  
  // 创建HTTP客户端
  HTTPClient http;
  http.begin(VISION_API_ENDPOINT);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Device-ID", String(ESP.getEfuseMac(), HEX));
  http.addHeader("X-Image-Width", String(fb->width));
  http.addHeader("X-Image-Height", String(fb->height));
  
  // 上传图像
  int httpCode = http.POST(jpgBuf, jpgLen);
  
  // 如果图像缓冲区是新分配的，释放它
  if (jpgBuf != fb->buf) {
    free(jpgBuf);
  }
  
  // 处理响应
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("服务器响应: " + response);
    
    // 解析JSON响应
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      result.success = true;
      result.resultType = doc["resultType"].as<int>();
      result.status = doc["status"].as<int>();
      result.confidence = doc["confidence"].as<int>();
      
      // 根据结果类型解析特定字段
      if (result.resultType == 1) {
        // 盲道检测结果
        Serial.printf("盲道检测结果: 状态=%d, 置信度=%d%%\n", 
                     result.status, result.confidence);
      } 
      else if (result.resultType == 2) {
        // 红绿灯检测结果
        result.remainingTime = doc["remainingTime"].as<int>();
        Serial.printf("红绿灯检测结果: 状态=%d, 剩余时间=%d秒, 置信度=%d%%\n", 
                     result.status, result.remainingTime, result.confidence);
      } 
      else if (result.resultType == 3) {
        // 障碍物检测结果
        result.distance = doc["distance"].as<int>();
        Serial.printf("障碍物检测结果: 状态=%d, 距离=%dcm, 置信度=%d%%\n", 
                     result.status, result.distance, result.confidence);
      }
      
      if (doc.containsKey("message")) {
        result.message = doc["message"].as<String>();
      }
    } else {
      Serial.println("JSON解析错误: " + String(error.c_str()));
    }
  } else {
    Serial.printf("图像上传失败，错误代码: %d\n", httpCode);
    Serial.println("错误响应: " + http.getString());
  }
  
  http.end();
  return result;
}

// 处理视觉结果并发送警报
void processVisionResult(VisionResult result, QueueHandle_t alertQueue) {
  if (!result.success) {
    return;
  }
  
  ObstacleAlert alert;
  
  // 根据结果类型处理
  switch (result.resultType) {
    case 1: // 盲道检测
      if (result.status != 2) { // 不在盲道中间
        alert.type = 2; // 盲道偏离警报
        alert.distance = 0;
        alert.priority = 1; // 低优先级
        
        if (result.status == 0) { // 未检测到盲道
          alert.priority = 2; // 中优先级
        }
        
        xQueueSend(alertQueue, &alert, 0);
      }
      break;
      
    case 2: // 红绿灯检测
      if (result.status == 1) { // 红灯
        alert.type = 5; // 红灯警告
        alert.distance = 0;
        alert.priority = 2; // 中优先级
        alert.data = result.remainingTime; // 存储剩余时间
        
        xQueueSend(alertQueue, &alert, 0);
      }
      break;
      
    case 3: // 障碍物检测
      if (result.status == 1) { // 有障碍物
        alert.type = 1; // 前方障碍
        alert.distance = result.distance;
        
        // 根据距离设置优先级
        if (result.distance < 30) {
          alert.priority = 3; // 高优先级
        } else if (result.distance < 60) {
          alert.priority = 2; // 中优先级
        } else {
          alert.priority = 1; // 低优先级
        }
        
        xQueueSend(alertQueue, &alert, 0);
      }
      break;
  }
}

#endif // VISION_H