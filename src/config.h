/*
 * 智能导盲杖 - 配置文件
 * 包含所有硬件定义和库配置
 */

#ifndef CONFIG_H
#define CONFIG_H

// 硬件接口定义
// 超声波传感器
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 6

// 振动马达/蜂鸣器提示
#define VIBRATION_PIN 7
#define BUZZER_PIN 8

// 摄像头接口 (ESP32-S3 支持摄像头接口)
// 使用ESP32-CAM模块
#define CAMERA_MODEL_ESP32S3_EYE

// 录音模块
#define MIC_PIN 10
#define RECORD_BUTTON_PIN 11  // 录音触发按钮

// WiFi配置
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

// API密钥
#define SPEECH_API_KEY "YourSpeechToTextAPIKey"
#define AI_MODEL_API_KEY "YourAIModelAPIKey"

// 其他配置参数
#define OBSTACLE_DISTANCE_THRESHOLD 100  // 障碍物检测阈值(cm)
#define HIGH_PRIORITY_DISTANCE 30        // 高优先级距离(cm)
#define MEDIUM_PRIORITY_DISTANCE 60      // 中优先级距离(cm)

#define VOICE_RECORD_TIME 5000           // 录音时长(ms)
#define VOICE_SAMPLE_RATE 16000          // 录音采样率(Hz)

#endif // CONFIG_H