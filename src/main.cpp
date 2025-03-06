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
 
 // 硬件接口定义
 // 超声波传感器
 #define ULTRASONIC_TRIG_PIN 5
 #define ULTRASONIC_ECHO_PIN 6
 // 振动马达/蜂鸣器提示
 #define VIBRATION_PIN 7
 #define BUZZER_PIN 8
 // 摄像头接口 (ESP32-S3 支持摄像头接口)
 // 使用默认的VSPI接口
 // 录音模块
 #define MIC_PIN 10
 
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
 
 // 注意：ObstacleAlert 和 TrafficLightStatus 结构体已在 vision.h 中定义，此处不再重复定义
 
 // 全局变量
 TrafficLightStatus currentTrafficLight = {0, 0};
 
 // 函数声明
 void ultrasonicTask(void *pvParameters);
 void visionTask(void *pvParameters);
 void trafficLightTask(void *pvParameters);
 void voiceTask(void *pvParameters);
 void alertTask(void *pvParameters);
 
 void setup() {
   // 初始化串口
   Serial.begin(115200);
   Serial.println("智能导盲杖系统启动中...");
   
   // 初始化GPIO
   pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
   pinMode(ULTRASONIC_ECHO_PIN, INPUT);
   pinMode(VIBRATION_PIN, OUTPUT);
   pinMode(BUZZER_PIN, OUTPUT);
   pinMode(MIC_PIN, INPUT);
   
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
     &ultrasonicTaskHandle
   );
   
   xTaskCreate(
     visionTask,
     "VisionTask",
     8192,  // 视觉任务需要更多内存
     NULL,
     1,
     &visionTaskHandle
   );
   
   xTaskCreate(
     trafficLightTask,
     "TrafficLightTask",
     8192,  // 视觉任务需要更多内存
     NULL,
     1,
     &trafficLightTaskHandle
   );
   
   xTaskCreate(
     voiceTask,
     "VoiceTask",
     8192,  // 语音处理需要更多内存
     NULL,
     1,
     &voiceTaskHandle
   );
   
   xTaskCreate(
     alertTask,
     "AlertTask",
     2048,
     NULL,
     2,  // 更高优先级
     &alertTaskHandle
   );
   
   Serial.println("所有任务已创建，系统运行中...");
 }
 
 void loop() {
   // 主循环为空，所有功能由FreeRTOS任务处理
   vTaskDelay(1000 / portTICK_PERIOD_MS);
 }
 
 // 超声波测距任务
 void ultrasonicTask(void *pvParameters) {
   long duration;
   int distance;
   ObstacleAlert alert;
   
   for(;;) {
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
     
     // 如果检测到障碍物
     if (distance < 100) {
       alert.type = 1; // 前方障碍
       alert.distance = distance;
       
       // 根据距离设置优先级
       if (distance < 30) {
         alert.priority = 3; // 高优先级
       } else if (distance < 60) {
         alert.priority = 2; // 中优先级
       } else {
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
 void visionTask(void *pvParameters) {
   // 这里将实现摄像头初始化和盲道检测算法
   // 由于涉及复杂的图像处理，这里只提供框架
   
   for(;;) {
     // 获取图像
     // 处理图像，检测盲道
     // 如果偏离盲道，发送警报
     
     // 每500ms处理一次图像
     vTaskDelay(500 / portTICK_PERIOD_MS);
   }
 }
 
 // 红绿灯检测任务
 void trafficLightTask(void *pvParameters) {
   // 这里将实现红绿灯检测算法
   // 由于涉及复杂的图像处理，这里只提供框架
   
   for(;;) {
     // 获取图像
     // 处理图像，检测红绿灯
     // 更新全局红绿灯状态
     
     // 如果是红灯，计算剩余时间并提醒用户
     
     // 每500ms处理一次图像
     vTaskDelay(500 / portTICK_PERIOD_MS);
   }
 }
 
 // 语音处理任务
 void voiceTask(void *pvParameters) {
   // 这里将实现录音、语音识别和大模型交互
   // 由于涉及复杂的网络和API调用，这里只提供框架
   
   for(;;) {
     // 检测是否有录音请求（可以通过按钮触发）
     // 录制语音
     // 上传到云端进行识别
     // 将识别结果发送给大模型
     // 接收大模型回复并播放
     
     vTaskDelay(100 / portTICK_PERIOD_MS);
   }
 }
 
 // 警报任务
 void alertTask(void *pvParameters) {
   ObstacleAlert receivedAlert;
   
   for(;;) {
     // 从队列接收警报
     if (xQueueReceive(alertQueue, &receivedAlert, portMAX_DELAY) == pdPASS) {
       // 根据警报类型和优先级触发不同的提示
       switch (receivedAlert.priority) {
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