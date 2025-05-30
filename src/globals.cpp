#include "config.h"

// FreeRTOS相关头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// 任务句柄定义
TaskHandle_t ultrasonicTaskHandle = NULL;
// TaskHandle_t visionTaskHandle = NULL;
// TaskHandle_t trafficLightTaskHandle = NULL;
// TaskHandle_t voiceTaskHandle = NULL;
TaskHandle_t alertTaskHandle = NULL;
TaskHandle_t navigationTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t lightSensorTaskHandle = NULL;
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t voiceTaskHandle = NULL;

// 队列句柄定义
QueueHandle_t alertQueue = NULL; // 警报队列
QueueHandle_t voiceCommandQueue = NULL; // 语音命令队列

// 信号量定义
SemaphoreHandle_t wifiMutex = NULL; // WiFi资源互斥锁

// 全局变量定义
bool isConnectedToWifi = false; // 是否连接到WiFi

bool record_status_me = true; // 录音状态

String accessToken = ""; // 访问令牌

double originLat = NULL; // 原始纬度
double originLng = NULL; // 原始经度