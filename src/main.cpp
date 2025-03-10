/*
 * 智能导盲杖 - 基于ESP32-S3和FreeRTOS
 * 功能：
 * 1. 超声波避障
 * 2. 视觉检测盲道
 * 3. 红绿灯状态检测及剩余时间
 * 4. 语音录制、识别及大模型交互
 */

#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "config.h"
#include "vision.h"
#include "voice.h"
#include "navigation.h"

// 任务句柄
TaskHandle_t ultrasonicTaskHandle = NULL;
TaskHandle_t visionTaskHandle = NULL;
TaskHandle_t trafficLightTaskHandle = NULL;
TaskHandle_t voiceTaskHandle = NULL;
TaskHandle_t alertTaskHandle = NULL;

// 队列句柄
QueueHandle_t alertQueue = NULL;

// 互斥锁
SemaphoreHandle_t i2cMutex = NULL;
SemaphoreHandle_t spiMutex = NULL;

// 全局变量
TrafficLightStatus currentTrafficLight = {0, 0};
int lastObstacleDistance = -1; // 存储最近一次检测到的障碍物距离

NavigationRoute currentRoute;  // 当前导航路线
bool navigationActive = false; // 导航是否激活
int currentStepIndex = 0;      // 当前导航步骤索引

// 函数声明
void ultrasonicTask(void *pvParameters);   // 超声波测距任务
void visionTask(void *pvParameters);       // 视觉任务
void trafficLightTask(void *pvParameters); // 红绿灯检测任务
void voiceTask(void *pvParameters);        // 语音任务
void alertTask(void *pvParameters);        // 警报任务
void navigationTask(void *pvParameters);   // 导航任务

void setup()
{
  // 初始化串口
  Serial.begin(115200);
  Serial.println("智能导盲杖系统启动中...");

  // 初始化GPIO
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MIC_PIN, INPUT);
  pinMode(VOICE_BUTTON_PIN, INPUT_PULLUP); // 添加语音按钮初始化

  // 创建互斥锁
  i2cMutex = xSemaphoreCreateMutex();
  spiMutex = xSemaphoreCreateMutex();

  // 创建队列
  alertQueue = xQueueCreate(10, sizeof(ObstacleAlert));

  // 创建任务
  xTaskCreate(
      ultrasonicTask,
      "UltrasonicTask",
      4096,
      NULL,
      1,
      &ultrasonicTaskHandle);

  xTaskCreate(
      visionTask,
      "VisionTask",
      8192, // 视觉任务需要更多内存
      NULL,
      1,
      &visionTaskHandle);

  xTaskCreate(
      trafficLightTask,
      "TrafficLightTask",
      8192, // 视觉任务需要更多内存
      NULL,
      1,
      &trafficLightTaskHandle);

  xTaskCreate(
      voiceTask,
      "VoiceTask",
      8192, // 语音处理需要更多内存
      NULL,
      1,
      &voiceTaskHandle);

  xTaskCreate(
      alertTask,
      "AlertTask",
      2048,
      NULL,
      2, // 更高优先级
      &alertTaskHandle);

  // 创建导航任务
  xTaskCreate(
      navigationTask,
      "NavigationTask",
      8192, // 导航任务需要更多内存
      NULL,
      1,
      NULL);

  Serial.println("所有任务已创建，系统运行中...");
}

void loop()
{
  // 主循环为空，所有功能由FreeRTOS任务处理
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// 超声波测距任务
void ultrasonicTask(void *pvParameters)
{
  long duration;
  int distance;
  ObstacleAlert alert;

  for (;;)
  {
    // 触发超声波
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    // 读取回波时间
    duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);

    // 计算距离 (声速340m/s)
    distance = duration * 0.034 / 2;
    lastObstacleDistance = distance;

    // 如果检测到障碍物
    if (distance < 100)
    {
      alert.type = 1; // 前方障碍
      alert.distance = distance;

      // 根据距离设置优先级
      if (distance < 30)
      {
        alert.priority = 3; // 高优先级
      }
      else if (distance < 60)
      {
        alert.priority = 2; // 中优先级
      }
      else
      {
        alert.priority = 1; // 低优先级
      }

      // 发送到警报队列
      xQueueSend(alertQueue, &alert, 0);
    }

    // 每100ms检测一次
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// 视觉检测任务 (盲道检测)
void visionTask(void *pvParameters)
{
  // 初始化摄像头
  if (!initCamera())
  {
    Serial.println("摄像头初始化失败，视觉任务终止");
    vTaskDelete(NULL);
    return;
  }

  Serial.println("视觉任务启动成功");

  for (;;)
  {
    // 获取图像
    camera_fb_t *fb = esp_camera_fb_get();

    if (fb)
    {
      // 处理图像，检测盲道
      detectTactilePaving(fb, alertQueue);

      // 释放图像缓冲区
      esp_camera_fb_return(fb);
    }
    else
    {
      Serial.println("获取图像失败");
    }

    // 每500ms处理一次图像
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// 红绿灯检测任务
void trafficLightTask(void *pvParameters)
{
  // 等待摄像头初始化完成
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  Serial.println("红绿灯检测任务启动");

  for (;;)
  {
    // 获取图像
    camera_fb_t *fb = esp_camera_fb_get();

    if (fb)
    {
      // 处理图像，检测红绿灯
      TrafficLightStatus lightStatus = detectTrafficLight(fb);

      // 更新全局红绿灯状态
      if (lightStatus.status != 0)
      {
        currentTrafficLight = lightStatus;

        // 如果是红灯，发送警报
        if (lightStatus.status == 1)
        {
          ObstacleAlert alert;
          alert.type = 5;     // 5: 红灯警告
          alert.distance = 0; // 不适用
          alert.priority = 2; // 中优先级

          // 发送到警报队列
          xQueueSend(alertQueue, &alert, 0);

          // 输出红绿灯状态
          Serial.print("检测到红灯，剩余时间: ");
          Serial.print(lightStatus.remainingTime);
          Serial.println("秒");
        }
        else if (lightStatus.status == 2)
        {
          Serial.print("检测到绿灯，剩余时间: ");
          Serial.print(lightStatus.remainingTime);
          Serial.println("秒");
        }
        else if (lightStatus.status == 3)
        {
          Serial.print("检测到黄灯，剩余时间: ");
          Serial.print(lightStatus.remainingTime);
          Serial.println("秒");
        }
      }

      // 释放图像缓冲区
      esp_camera_fb_return(fb);
    }
    else
    {
      Serial.println("获取图像失败");
    }

    // 每500ms处理一次图像
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// 语音处理任务
void voiceTask(void *pvParameters)
{
  // 初始化WiFi
  if (!initWiFi())
  {
    Serial.println("WiFi初始化失败，部分语音功能可能不可用");
  }

  // 初始化音频系统
  if (!initAudio())
  {
    Serial.println("音频系统初始化失败，语音任务终止");
    vTaskDelete(NULL);
    return;
  }

  Serial.println("语音任务启动成功");

  // 分配录音缓冲区
  uint8_t *audioBuffer = (uint8_t *)malloc(RECORD_BUFFER_SIZE);
  if (!audioBuffer)
  {
    Serial.println("无法分配录音缓冲区，语音任务终止");
    vTaskDelete(NULL);
    return;
  }

  // 语音交互按钮状态
  bool lastButtonState = HIGH;
  bool recording = false;
  uint32_t recordStartTime = 0;

  for (;;)
  {
    // 检测是否有录音请求（通过按钮触发）
    bool currentButtonState = digitalRead(VOICE_BUTTON_PIN);

    // 按钮按下（下降沿）开始录音
    if (currentButtonState == LOW && lastButtonState == HIGH)
    {
      tone(BUZZER_PIN, 800, 100); // 提示音
      Serial.println("按钮按下，开始录音");
      recording = true;
      recordStartTime = millis();
    }

    // 按钮释放（上升沿）或录音超时，结束录音并处理
    if ((recording && currentButtonState == HIGH && lastButtonState == LOW) ||
        (recording && (millis() - recordStartTime > MAX_RECORD_TIME_MS)))
    {
      recording = false;
      tone(BUZZER_PIN, 1000, 100); // 提示音
      Serial.println("录音结束，开始处理");

      // 录制语音
      size_t audioLength = 0;
      if (recordAudio(audioBuffer, &audioLength, millis() - recordStartTime))
      {
        // 上传到百度进行识别
        String recognizedText = speechToText(audioBuffer, audioLength);

        if (recognizedText.length() > 0)
        {
          Serial.println("识别结果: " + recognizedText);

          // 将识别结果发送给大模型
          String aiResponse = queryAIModel(recognizedText);

          // 接收大模型回复并播放
          playResponse(aiResponse);
        }
        else
        {
          Serial.println("语音识别失败");
          playResponse("抱歉，我没有听清楚");
        }
      }
      else
      {
        Serial.println("录音失败");
      }
    }

    lastButtonState = currentButtonState;
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  // 释放资源（实际上永远不会执行到这里）
  free(audioBuffer);
}

// 警报任务
void alertTask(void *pvParameters)
{
  ObstacleAlert receivedAlert;

  for (;;)
  {
    // 从队列接收警报
    if (xQueueReceive(alertQueue, &receivedAlert, portMAX_DELAY) == pdPASS)
    {
      // 根据警报类型和优先级触发不同的提示
      switch (receivedAlert.priority)
      {
      case 3: // 高优先级
        // 快速振动和蜂鸣
        digitalWrite(VIBRATION_PIN, HIGH);
        tone(BUZZER_PIN, 2000, 500);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(VIBRATION_PIN, LOW);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(VIBRATION_PIN, HIGH);
        tone(BUZZER_PIN, 2000, 500);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(VIBRATION_PIN, LOW);
        break;

      case 2: // 中优先级
        // 中等振动和蜂鸣
        digitalWrite(VIBRATION_PIN, HIGH);
        tone(BUZZER_PIN, 1500, 300);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        digitalWrite(VIBRATION_PIN, LOW);
        break;

      case 1: // 低优先级
        // 轻微振动
        digitalWrite(VIBRATION_PIN, HIGH);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(VIBRATION_PIN, LOW);
        break;
      }

      // 输出调试信息
      Serial.print("检测到障碍物，类型: ");
      Serial.print(receivedAlert.type);
      Serial.print(", 距离: ");
      Serial.print(receivedAlert.distance);
      Serial.print("cm, 优先级: ");
      Serial.println(receivedAlert.priority);
    }
  }
}

// 添加导航任务
void navigationTask(void *pvParameters)
{
  // 等待WiFi连接
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  Serial.println("导航任务启动成功");

  for (;;)
  {
    // 如果导航激活，定期检查是否需要提醒用户下一步指令
    if (navigationActive && currentStepIndex < currentRoute.steps.size())
    {
      // 在实际项目中，这里应该结合GPS模块判断用户是否已经完成当前步骤
      // 现在只是简单地每隔一段时间提醒用户当前导航状态

      // 创建导航提醒警报
      ObstacleAlert navAlert;
      navAlert.type = 10;    // 10: 导航提醒
      navAlert.distance = 0; // 不适用
      navAlert.priority = 1; // 低优先级

      // 发送到警报队列
      xQueueSend(alertQueue, &navAlert, 0);

      Serial.println("导航提醒: 正在导航中，当前步骤 " + String(currentStepIndex + 1) +
                     "/" + String(currentRoute.steps.size()));
    }

    // 每10秒检查一次
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}