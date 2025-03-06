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

// 盲道检测参数
#define TACTILE_PAVING_H_MIN 20   // 黄色盲道的HSV色彩空间最小值
#define TACTILE_PAVING_H_MAX 40   // 黄色盲道的HSV色彩空间最大值
#define TACTILE_PAVING_S_MIN 100  // 饱和度最小值
#define TACTILE_PAVING_S_MAX 255  // 饱和度最大值
#define TACTILE_PAVING_V_MIN 100  // 亮度最小值
#define TACTILE_PAVING_V_MAX 255  // 亮度最大值

// 红绿灯检测参数
#define RED_H_MIN 160    // 红色的HSV色彩空间最小值
#define RED_H_MAX 180    // 红色的HSV色彩空间最大值
#define GREEN_H_MIN 50   // 绿色的HSV色彩空间最小值
#define GREEN_H_MAX 80   // 绿色的HSV色彩空间最大值
#define YELLOW_H_MIN 20  // 黄色的HSV色彩空间最小值
#define YELLOW_H_MAX 40  // 黄色的HSV色彩空间最大值

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
} ObstacleAlert;

// 函数声明
bool initCamera();
void detectTactilePaving(camera_fb_t *fb, QueueHandle_t alertQueue);
TrafficLightStatus detectTrafficLight(camera_fb_t *fb);
int estimateRemainingTime(camera_fb_t *fb, uint8_t lightStatus);

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

#endif // VISION_H