/*
 * 智能导盲杖 - 配置文件
 * 包含所有硬件定义和库配置
 */

#ifndef CONFIG_H
#define CONFIG_H

// 引入必要的库
#include <Arduino.h>
// FreeRTOS相关头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "base64.h"

// 定义任务优先级
#define ULTRASONIC_TASK_PRIORITY 3
#define VISION_TASK_PRIORITY 2
#define TRAFFIC_LIGHT_TASK_PRIORITY 2
#define VOICE_TASK_PRIORITY 10
#define ALERT_TASK_PRIORITY 4
#define NAVIGATION_TASK_PRIORITY 1
#define BUTTON_TASK_PRIORITY 1
#define LIGHT_SENSOR_TASK_PRIORITY 1
#define GPS_TASK_PRIORITY 1

// 定义任务栈大小
#define ULTRASONIC_TASK_STACK_SIZE 2048
#define BUTTON_TASK_STACK_SIZE 2048
#define LIGHT_SENSOR_TASK_STACK_SIZE 2048
#define GPS_TASK_STACK_SIZE 2048
#define VOICE_TASK_STACK_SIZE 1024 * 32 // 语音处理需要更大的栈
#define VIBRATION_MODULE_PIN 3

// 硬件接口定义
// 超声波传感器
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 6

// 光敏传感器 (AO=9, DO=10)
#define LIGHT_SENSOR_AO_PIN 9  // 模拟输出引脚 (本次未使用)
#define LIGHT_SENSOR_DO_PIN 10 // 数字输出引脚
#define LIGHT_CONTROL_PIN 21   // 控制灯的引脚 (假设使用 GPIO 12)

// 振动马达/蜂鸣器提示
#define VIBRATION_PIN 3
#define BUZZER_PIN 11

// 录音模块
#define MIC_PIN 10
#define RECORD_BUTTON_PIN 11 // 录音触发按钮

// 语音按钮引脚
#define VOICE_BUTTON_PIN 14 // 根据实际硬件连接调整
#define TEST_BUTTON_PIN 2   // 根据实际硬件连接调整
                                    // 最大录音时间（毫秒）
#define MAX_RECORD_TIME_MS 5000 // 5秒

// WiFi配置
#define WIFI_SSID "sharp_caterpillar"
#define WIFI_PASSWORD "zzn20041031"

// 百度语音识别API配置
#define BAIDU_CLIENT_ID "gPm0ytVOuc9VpDkktWaoUEIC"
#define BAIDU_CLIENT_SECRET "z9IPnBfz0T7cCaxMsbRshKygDBw98Tuy"

// 其他配置参数
#define OBSTACLE_DISTANCE_THRESHOLD 100 // 障碍物检测阈值(cm)
#define HIGH_PRIORITY_DISTANCE 30       // 高优先级距离(cm)
#define MEDIUM_PRIORITY_DISTANCE 60     // 中优先级距离(cm)

#define VOICE_RECORD_TIME 5000  // 录音时长(ms)
#define VOICE_SAMPLE_RATE 16000 // 录音采样率(Hz)

// 高德地图API配置
#define AMAP_API_KEY "YourAmapAPIKey"
#define DEFAULT_LOCATION "115.867661,28.743242" // 默认位置：华东交通大学

// 百度语音API配置
#define BAIDU_API_KEY "YourBaiduAPIKey"
#define BAIDU_SECRET_KEY "YourBaiduSecretKey"
#define BAIDU_CUID "YourBaiduCUID"

// 心知天气API配置
#define WEATHER_API_KEY "YourWeatherAPIKey"

// 和风天气KEY
#define QWEATHER_KEY "3f71fd1e04a045f2a1996eb7c1f4302c"


//录音配置
#define INMP441_WS 5
#define INMP441_SCK 4
#define INMP441_SD 6

#define MAX98357_LRC 16
#define MAX98357_BCLK 15
#define MAX98357_DIN 7
#define I2S_IN_PORT I2S_NUM_0

// Audio recording settings
#define SAMPLE_RATE 16000
#define RECORD_TIME_SECONDS 10
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_TIME_SECONDS)


#define EIDSP_QUANTIZE_FILTERBANK 0
#define LED_BUILT_IN 21

// 警报结构体
typedef struct
{
    uint8_t type;   // 警报类型: 0-超声波, 1-视觉, 2-红绿灯
    uint8_t level;  // 警报级别: 0-低, 1-中, 2-高
    String message; // 警报消息
} AlertInfo;

// 语音命令结构体
typedef struct
{
    uint8_t type;   // 命令类型: 0-导航, 1-天气查询, 2-一般对话
    String content; // 命令内容
} VoiceCommand;

// 全局变量
extern bool isConnectedToWifi; // 是否连接到WiFi

// 任务句柄
extern TaskHandle_t ultrasonicTaskHandle;
// extern TaskHandle_t visionTaskHandle;
// extern TaskHandle_t trafficLightTaskHandle;
// extern TaskHandle_t voiceTaskHandle;
extern TaskHandle_t alertTaskHandle;
extern TaskHandle_t navigationTaskHandle;
extern TaskHandle_t buttonTaskHandle;
extern TaskHandle_t lightSensorTaskHandle;
extern TaskHandle_t voiceTaskHandle;
extern TaskHandle_t gpsTaskHandle;

// 队列句柄
extern QueueHandle_t alertQueue;        // 警报队列
extern QueueHandle_t voiceCommandQueue; // 语音命令队列

// 信号量
extern SemaphoreHandle_t wifiMutex; // WiFi资源互斥锁

// 全局变量
extern double originLat; // 原始纬度
extern double originLng; // 原始经度

extern bool record_status_me; // 录音状态
extern String accessToken; // 百度语音识别的access token

// extern const char* severAddress; // 服务器地址
// extern uint16_t severPort; // 服务器端口
// extern char* severPath_ai; // AI对话路径


#endif // CONFIG_H