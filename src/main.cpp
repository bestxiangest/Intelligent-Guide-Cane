/*
 * 星辰引路者 - 智能导盲杖主程序
 * 基于ESP32-S3的多模态智能导盲系统
 * 功能:超声波避障、语音交互、唤醒词识别、GPS定位、光敏感应
 */

// ==================== 头文件包含 ====================
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <_3_inferencing.h>
#include <HCSR04.h>

// FreeRTOS相关头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// 项目头文件
#include "config.h"
#include "network.h"
#include "voice.h"
#include "gps.h"

// ==================== 宏定义 ====================
// 唤醒词相关配置
#define SAMPLE_RATE 16000U
#define LED_BUILT_IN 21
#define EIDSP_QUANTIZE_FILTERBANK 0
#define PRED_VALUE_THRESHOLD 0.9 // 唤醒词阈值，阈值越大，要求识别的唤醒词更精准

// ==================== 结构体定义 ====================
/** 音频推理缓冲区结构体 */
typedef struct
{
  int16_t *buffer;    // 音频数据缓冲区
  uint8_t buf_ready;  // 缓冲区就绪标志
  uint32_t buf_count; // 当前缓冲区计数
  uint32_t n_samples; // 采样数量
} inference_t;

// ==================== 全局变量 ====================
// 超声波传感器对象
HCSR04 hc(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);

// 唤醒词推理相关变量
static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false;     // 设置为true可查看原始信号生成的特征
static bool record_status = true; // 录音状态标志

// 导航状态管理变量
static bool navigationActive = false;        // 导航是否激活
static String currentDestination = "";       // 当前目的地
static unsigned long lastNavigationUpdate = 0; // 上次导航更新时间
static const unsigned long NAVIGATION_UPDATE_INTERVAL = 30000; // 导航更新间隔(30秒)

// ==================== 函数声明 ====================
// 系统初始化相关
void initHardware();           // 硬件初始化
void initUltrasonicPins();     // 超声波传感器引脚初始化
void initVibrationAndBuzzer(); // 振动模块和蜂鸣器初始化
void initButtonPins();         // 按钮引脚初始化
void initLightSensorPins();    // 光敏传感器引脚初始化
void initLEDPins();            // LED引脚初始化
void initWakeWordSystem();     // 唤醒词系统初始化

// FreeRTOS任务管理
void setupTasks();       // 创建FreeRTOS任务
void createCore0Tasks(); // 创建核心0任务
void createCore1Tasks(); // 创建核心1任务
void printTaskInfo();    // 打印任务信息

// 硬件功能任务
void ultrasonicTask(void *pvParameters);  // 超声波检测任务
void buttonTask(void *parameter);         // 按钮检测任务
void lightSensorTask(void *pvParameters); // 光敏传感器任务
void gpsTask(void *pvParameters);         // GPS任务

// 超声波相关函数
bool testHCSR04BasicFunction();          // 测试HCSR04基础功能
float measureUltrasonicDistance();       // 使用HCSR04库测量超声波距离
void triggerObstacleAlert(float distance); // 触发障碍物警报

// 按钮相关函数
void handleButtonPress(); // 处理按钮按下事件

// 光敏传感器相关函数
bool checkLightCondition();        // 检查光线条件
void controlLighting(bool isDark); // 控制照明设备

// GPS相关函数
bool processGpsData(); // 处理GPS数据
void uploadGpsData();  // 上传GPS数据
void handleGpsError(); // 处理GPS错误

// 语音交互相关
void voiceTask(void *parameter);                                                                                                 // 语音交互任务
void handleVoiceInteraction();                                                                                                   // 处理语音交互
size_t performAudioRecording(uint8_t *pcm_data);                                                                                 // 执行音频录制
void processVoiceRecognition(uint8_t *pcm_data, size_t recordingSize);                                                           // 处理语音识别
uint32_t calculateAudioEnergy(int16_t *data, size_t bytes_read);                                                                 // 计算音频能量
void updateVoiceDetection(uint32_t audioEnergy, size_t *noVoicePre, size_t *noVoiceCur, size_t *noVoiceTotal, size_t *VoiceCnt); // 更新语音检测
bool shouldStopRecording(size_t noVoiceTotal, size_t recordingSize);                                                             // 检查是否停止录音
void resetRecordStatus();                                                                                                        // 重置录音状态
bool isValidRecording(size_t recordingSize);                                                                                     // 检查录音有效性
void enableContinuousConversation();                                                                                             // 启用连续对话

// 导航相关函数
void startNavigation(String destination);     // 启动导航
void updateNavigationStatus();                // 更新导航状态
void stopNavigation();                        // 停止导航
void checkNavigationUpdate();                 // 检查是否需要更新导航

// 唤醒词推理相关
void performWakeWordInference();                          // 执行唤醒词推理
void printInferenceResults(ei_impulse_result_t *result);  // 打印推理结果
void checkWakeWordDetection(ei_impulse_result_t *result); // 检查唤醒词检测
void handleWakeWordDetected();                            // 处理唤醒词检测事件

// 音频推理系统函数
static bool microphone_inference_record(void);                                             // 麦克风推理录制
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr); // 获取音频信号数据
static bool microphone_inference_start(uint32_t n_samples);                                // 启动麦克风推理
static void microphone_inference_end(void);                                                // 结束麦克风推理
static void audio_inference_callback(uint32_t n_bytes);                                    // 音频推理回调
static void capture_samples(void *arg);                                                    // 音频采集任务
void amplifyAudioData(int16_t *buffer, size_t bytes_read);                                 // 放大音频数据

// 音频播放
void playAudio_Zai(); // 播放唤醒响应音频

// ==================== 主程序入口 ====================
/**
 * @brief 系统初始化函数
 * 按顺序初始化各个模块和组件
 */
void setup()
{
  // 串口初始化
  Serial.begin(115200);
  ei_printf("\n=== 星辰引路者 - 智能导盲杖启动 ===\n");
  ei_printf("基于ESP32-S3的多模态智能导盲系统\n");
  ei_printf("版本: v1.0\n");
  ei_printf("========================================\n");

  // 1. 硬件初始化
  ei_printf("[1/7] 初始化硬件模块...\n");
  initHardware();
  
  // 1.5. HCSR04硬件测试
  // ei_printf("[1.5/7] 测试HCSR04超声波模块...\n");
  // bool hcsr04TestResult = testHCSR04BasicFunction();
  // if (hcsr04TestResult)
  // {
  //   ei_printf("  ✓ HCSR04超声波模块测试通过\n");
  // }
  // else
  // {
  //   ei_printf("  ✗ HCSR04超声波模块测试失败，请检查硬件连接\n");
  // }

  // 2. 网络初始化
  ei_printf("[2/7] 初始化WiFi连接...\n");
  initWiFi();
  ei_printf("  ✓ WiFi初始化完成\n");


  // 3. 音频系统初始化
  ei_printf("[3/7] 初始化I2S音频系统...\n");
  set_i2s();

  // 4. GPS模块初始化
  ei_printf("[4/7] 初始化GPS模块...\n");
  gpsInit();

  // 5. FreeRTOS组件初始化
  ei_printf("[5/7] 创建队列和信号量...\n");
  alertQueue = xQueueCreate(10, sizeof(AlertInfo));
  if (alertQueue != NULL)
  {
    ei_printf("  ✓ 警报队列创建成功\n");
  }
  wifiMutex = xSemaphoreCreateMutex();
  if (wifiMutex != NULL)
  {
    ei_printf("  ✓ WiFi互斥锁创建成功\n");
  }

  // 6. 启动FreeRTOS任务
  ei_printf("[6/7] 启动FreeRTOS任务...\n");
  setupTasks();

  // 7. 语音服务初始化
  ei_printf("[7/7] 初始化语音服务...\n");
  accessToken = getAccessToken_baidu();
  if (accessToken.length() > 0)
  {
    ei_printf("  ✓ 百度语音服务初始化成功\n");
  }

  ei_printf("\n=== 系统初始化完成 ===\n");

  // FreeRTOS超声波模块测试
  // ei_printf("\n[测试] 验证FreeRTOS超声波功能...\n");
  // vTaskDelay(pdMS_TO_TICKS(1000)); // 等待1秒让系统稳定

  // 唤醒词推理系统初始化
  initWakeWordSystem();
}


/**
 * @brief 初始化唤醒词推理系统
 */
void initWakeWordSystem()
{
  ei_printf("\n--- 唤醒词推理系统初始化 ---\n");

  // 等待串口就绪
  while (!Serial)
  {
    delay(10);
  }

  // 设置LED指示灯
  pinMode(LED_BUILT_IN, OUTPUT);
  digitalWrite(LED_BUILT_IN, LOW);

  // 打印推理设置信息
  ei_printf("推理设置信息:\n");
  ei_printf("  采样间隔: %.2f ms\n", (float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf("  帧大小: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("  采样长度: %d ms\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("  分类数量: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
  ei_printf("  唤醒阈值: %.2f\n", PRED_VALUE_THRESHOLD);

  ei_printf("\n1秒后开始连续推理...\n");
  ei_sleep(1000);

  // 启动麦克风推理
  if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false)
  {
    ei_printf("错误: 无法分配音频缓冲区 (大小 %d)\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    ei_printf("这可能是由于模型的窗口长度导致的\n");
    return;
  }

  ei_printf("✓ 唤醒词检测已启动...\n");
  ei_printf("--- 系统就绪，等待唤醒词 ---\n");
}

/**
 * @brief 主循环函数
 * 持续进行唤醒词检测和推理
 */
void loop()
{
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastStateCheck = 0;
  static unsigned long voiceInteractionStartTime = 0;
  const unsigned long VOICE_INTERACTION_MAX_TIME = 60000; // 语音交互最大允许时间60秒
  unsigned long currentTime = millis();
  
  // 每10秒输出一次心跳信息
  if (currentTime - lastHeartbeat > 10000)
  {
    ei_printf("[主循环] 系统运行正常，唤醒词检测活跃\n");
    ei_printf("[状态监控] record_status: %s, record_status_me: %s, buf_ready: %d\n", 
              record_status ? "true" : "false", 
              record_status_me ? "true" : "false",
              inference.buf_ready);
    
    // 添加音频系统状态调试信息
    ei_printf("[音频调试] inference.buf_count: %d, inference.n_samples: %d\n",
              inference.buf_count, inference.n_samples);
    ei_printf("[音频调试] inference.buffer: %s\n", 
              inference.buffer ? "已分配" : "未分配");
    
    // 检查语音交互是否卡住
    if (!record_status || !record_status_me) {
      if (voiceInteractionStartTime == 0) {
        voiceInteractionStartTime = currentTime;
        ei_printf("[状态监控] 检测到语音交互开始\n");
      } else if (currentTime - voiceInteractionStartTime > VOICE_INTERACTION_MAX_TIME) {
        ei_printf("[状态监控] 警告:语音交互超时，强制恢复状态\n");
        record_status = true;
        record_status_me = true;
        voiceInteractionStartTime = 0;
        digitalWrite(LED_BUILT_IN, LOW);
        digitalWrite(LED_BUILTIN, LOW);
      }
    } else {
      if (voiceInteractionStartTime != 0) {
        ei_printf("[状态监控] 语音交互正常结束\n");
        voiceInteractionStartTime = 0;
      }
    }
    
    lastHeartbeat = currentTime;
  }
  
  
  // 执行唤醒词推理
  performWakeWordInference();
  
  // 检查导航更新
  checkNavigationUpdate();

  // 短暂延迟，避免过度占用CPU
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

/**
 * @brief 执行唤醒词推理
 */
void performWakeWordInference()
{
  // 如果语音交互正在进行，跳过唤醒词检测
  if (!record_status || !record_status_me)
  {
    return; // 语音交互进行中，暂停唤醒词检测
  }

  // 录制音频数据
  bool m = microphone_inference_record();
  if (!m)
  {
    ei_printf("[唤醒检测] 音频录制失败，尝试重新初始化音频系统\n");
    
    // 尝试重新初始化音频系统
    microphone_inference_end();
    delay(100);
    
    if (!microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT))
    {
      ei_printf("[唤醒检测] 音频系统重新初始化失败\n");
    }
    else
    {
      ei_printf("[唤醒检测] 音频系统重新初始化成功\n");
    }
    return;
  }

  // 设置信号结构
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &microphone_audio_signal_get_data;
  ei_impulse_result_t result = {0};

  // 运行分类器
  EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
  if (r != EI_IMPULSE_OK)
  {
    ei_printf("错误: 分类器运行失败 (%d)\n", r);
    return;
  }

  // 打印推理结果
  printInferenceResults(&result);

  // 检查唤醒词
  checkWakeWordDetection(&result);
}

/**
 * @brief 打印推理结果
 * @param result 推理结果指针
 */
void printInferenceResults(ei_impulse_result_t *result)
{
  ei_printf("预测结果 (DSP: %d ms, 分类: %d ms, 异常: %d ms): \n",
            result->timing.dsp, result->timing.classification, result->timing.anomaly);

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
  {
    ei_printf("    %s: ", result->classification[ix].label);
    ei_printf_float(result->classification[ix].value);
    ei_printf("\n");
  }
}

/**
 * @brief 检查唤醒词检测
 * @param result 推理结果指针
 */
void checkWakeWordDetection(ei_impulse_result_t *result)
{
  // 计算当前音频缓冲区的能量水平
  uint32_t audioEnergy = calculateAudioEnergy(inference.buffer, inference.n_samples * 2);
  const uint32_t MIN_AUDIO_ENERGY = 150; // 最小音频能量阈值，过滤静音状态
  
  ei_printf("[唤醒检测] 置信度: %.3f, 音频能量: %u\n", result->classification[0].value, audioEnergy);
  
  // 首先检查音频能量是否足够（避免静音时的误触发）
  if (audioEnergy < MIN_AUDIO_ENERGY)
  {
    ei_printf("[唤醒检测] 音频能量过低 (%u < %u)，跳过检测\n", audioEnergy, MIN_AUDIO_ENERGY);
    return;
  }
  
  // 唤醒词在第一位，此时判断classification[0]位置大于阈值表示唤醒
  if (result->classification[0].value > PRED_VALUE_THRESHOLD)
  {
    ei_printf("✓ 检测到唤醒词! 置信度: %.3f, 音频能量: %u\n", result->classification[0].value, audioEnergy);

    // 唤醒响应
    handleWakeWordDetected();
  }
}

/**
 * @brief 处理唤醒词检测事件
 */
void handleWakeWordDetected()
{
  digitalWrite(LED_BUILTIN, HIGH);  // 点亮LED指示
  digitalWrite(LED_BUILT_IN, HIGH); // 点亮内置LED

  ei_printf("[唤醒检测] 检测到唤醒词！\n");
  
  // 先暂停唤醒词检测，避免播放音频时的i2s冲突
  // ei_printf("[唤醒检测] 暂停唤醒词检测，准备播放响应音频\n");
  record_status = false;
  record_status_me = false;
  
  // ei_printf("[唤醒检测] 播放唤醒响应音频\n");
  playAudio_Zai(); // 播放唤醒响应音频

  // 设置语音交互标志
  // ei_printf("[唤醒检测] 设置语音交互触发标志\n");
  // record_status 和 record_status_me 已经在上面设置为false

  ei_printf("[唤醒检测] 唤醒处理完成，等待语音任务响应\n");
}

// ==================== 硬件初始化模块 ====================
/**
 * @brief 初始化所有硬件模块
 * 包括GPIO引脚配置和初始状态设置
 */
void initHardware()
{
  ei_printf("开始硬件初始化...\n");

  // 初始化各个功能模块的硬件
  initUltrasonicPins();
  initVibrationAndBuzzer();
  initButtonPins();
  initLightSensorPins();
  initLEDPins();

  ei_printf("✓ 硬件初始化完成\n");
}

/**
 * @brief 初始化超声波传感器引脚
 */
void initUltrasonicPins()
{
  // HCSR04库会自动处理引脚初始化，无需手动配置
  ei_printf("  ✓ 超声波传感器初始化完成\n");
}

/**
 * @brief 初始化振动模块和蜂鸣器引脚
 */
void initVibrationAndBuzzer()
{
  pinMode(VIBRATION_MODULE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(VIBRATION_MODULE_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH); // 蜂鸣器默认关闭（高电平）
  ei_printf("  ✓ 振动模块和蜂鸣器引脚初始化完成\n");
}

/**
 * @brief 初始化按钮引脚
 */
void initButtonPins()
{
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  ei_printf("  ✓ 按钮引脚初始化完成\n");
}

/**
 * @brief 初始化光敏传感器引脚
 */
void initLightSensorPins()
{
  pinMode(LIGHT_SENSOR_DO_PIN, INPUT);
  pinMode(LIGHT_CONTROL_PIN, OUTPUT);
  digitalWrite(LIGHT_CONTROL_PIN, LOW); // 默认关闭照明
  ei_printf("  ✓ 光敏传感器引脚初始化完成\n");
}

/**
 * @brief 初始化LED引脚
 */
void initLEDPins()
{
  pinMode(LED_BUILT_IN, OUTPUT);
  digitalWrite(LED_BUILT_IN, LOW);
  ei_printf("  ✓ LED引脚初始化完成\n");
}

// ==================== FreeRTOS任务管理模块 ====================
/**
 * @brief 设置并启动所有FreeRTOS任务
 * 创建各个功能模块的任务并分配到不同的CPU核心
 */
void setupTasks()
{
  ei_printf("开始创建FreeRTOS任务...\n");

  // 核心0任务组 - 传感器和硬件控制
  createCore0Tasks();

  // 核心1任务组 - 语音处理和网络通信
  createCore1Tasks();

  ei_printf("✓ 所有任务已创建并启动\n");
  printTaskInfo();
}

/**
 * @brief 创建运行在核心0的任务
 * 主要负责传感器数据采集和硬件控制
 */
void createCore0Tasks()
{
  // 创建语音交互任务
  BaseType_t result1 = xTaskCreatePinnedToCore(
      voiceTask,
      "VoiceTask",
      VOICE_TASK_STACK_SIZE,
      NULL,
      VOICE_TASK_PRIORITY,
      &voiceTaskHandle,
      0 // 在核心0上运行
  );

  // 创建按钮检测任务
  BaseType_t result2 = xTaskCreatePinnedToCore(
      buttonTask,
      "ButtonTask",
      BUTTON_TASK_STACK_SIZE,
      NULL,
      BUTTON_TASK_PRIORITY,
      &buttonTaskHandle,
      0 // 在核心0上运行
  );

  // 创建光敏传感器任务
  BaseType_t result3 = xTaskCreatePinnedToCore(
      lightSensorTask,
      "LightSensorTask",
      LIGHT_SENSOR_TASK_STACK_SIZE,
      NULL,
      LIGHT_SENSOR_TASK_PRIORITY,
      &lightSensorTaskHandle,
      0 // 在核心0上运行
  );

  // 创建GPS任务
  BaseType_t result4 = xTaskCreatePinnedToCore(
      gpsTask,
      "GPSTask",
      GPS_TASK_STACK_SIZE,
      NULL,
      GPS_TASK_PRIORITY,
      &gpsTaskHandle,
      0 // 在核心0上运行
  );

  // 检查核心0任务创建结果
  if (result2 == pdPASS && result3 == pdPASS && result4 == pdPASS)
  {
    ei_printf("  ✓ 核心0任务组创建成功\n");
    ei_printf("    - 按钮任务: %s\n", result2 == pdPASS ? "成功" : "失败");
    ei_printf("    - 光敏传感器任务: %s\n", result3 == pdPASS ? "成功" : "失败");
    ei_printf("    - GPS任务: %s\n", result4 == pdPASS ? "成功" : "失败");
  }
  else
  {
    ei_printf("  ✗ 核心0任务组创建失败\n");
    ei_printf("    - 按钮任务: %s\n", result2 == pdPASS ? "成功" : "失败");
    ei_printf("    - 光敏传感器任务: %s\n", result3 == pdPASS ? "成功" : "失败");
    ei_printf("    - GPS任务: %s\n", result4 == pdPASS ? "成功" : "失败");
  }
}

/**
 * @brief 创建运行在核心1的任务
 * 主要负责语音处理和网络通信
 */
void createCore1Tasks()
{
  // 创建超声波检测任务
  BaseType_t result = xTaskCreatePinnedToCore(
      ultrasonicTask,
      "UltrasonicTask",
      ULTRASONIC_TASK_STACK_SIZE,
      NULL,
      ULTRASONIC_TASK_PRIORITY,
      &ultrasonicTaskHandle,
      1 // 在核心1上运行
  );

  // 检查核心1任务创建结果
  if (result == pdPASS)
  {
    ei_printf("  ✓ 核心1任务组创建成功\n");
  }
  else
  {
    ei_printf("  ✗ 核心1任务组创建失败\n");
  }
}

/**
 * @brief 打印任务信息
 */
void printTaskInfo()
{
  ei_printf("\n--- 任务分配信息 ---\n");
  ei_printf("核心0任务:\n");
  ei_printf("  - 超声波避障检测\n");
  ei_printf("  - 按钮状态监测\n");
  ei_printf("  - 光敏传感器控制\n");
  ei_printf("  - GPS定位服务\n");
  ei_printf("核心1任务:\n");
  ei_printf("  - 语音交互处理\n");
  Serial.printf("可用堆内存: %d 字节\n", esp_get_free_heap_size());
  ei_printf("--- 任务信息结束 ---\n");
}

/**
 * @brief 超声波避障检测任务
 * 持续检测前方障碍物距离，当距离过近时触发警报
 * @param pvParameters 任务参数（未使用）
 */
void ultrasonicTask(void *pvParameters)
{
  ei_printf("超声波检测任务启动\n");
  
  // 统计信息
  int successCount = 0;
  int failureCount = 0;
  int totalMeasurements = 0;
  unsigned long lastStatsReport = millis();
  static int stackDebugCounter = 0;
  
  // 自适应延迟参数
  int baseDelay = 200; 
  int currentDelay = baseDelay;
  int consecutiveFailures = 0;

  while (1)
  {
    // 监控栈使用情况（每50次循环输出一次）
    if (stackDebugCounter % 50 == 0) {
      UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
      ei_printf("[超声波任务] 栈剩余: %d 字节\n", stackHighWaterMark * sizeof(StackType_t));
    }
    stackDebugCounter++;
    totalMeasurements++;
    
    // 使用HCSR04库测量距离
    float distance = measureUltrasonicDistance();

    if (distance > 0 && distance < 400) // HCSR04有效测量范围2-400cm
    {
      successCount++;
      consecutiveFailures = 0;
      currentDelay = baseDelay; // 恢复正常延迟
      
      // ei_printf("当前距离: %.1f CM\n", distance);
      
      // 检查是否需要警报
      if (distance <= OBSTACLE_DISTANCE_THRESHOLD)
      {
        triggerObstacleAlert(distance);
        currentDelay = 100; // 检测到障碍物时提高检测频率
      }
      else if (distance <= 60)
      {
        currentDelay = 150; // 中等距离时适中频率
      }
    }
    else
    {
      failureCount++;
      consecutiveFailures++;
      
      // 连续失败时增加延迟，避免过度占用资源
      if (consecutiveFailures > 3)
      {
        currentDelay = baseDelay * 2; // 延迟加倍
      }
      
      // 连续失败过多时，重置延迟
      if (consecutiveFailures > 10)
      {
        consecutiveFailures = 0;
        currentDelay = baseDelay;
      }
    }
    
    // 每30秒报告一次统计信息
    if (millis() - lastStatsReport > 30000)
    {
      float successRate = (float)successCount / totalMeasurements * 100;
      ei_printf("超声波统计: 总测量%d次, 成功%d次, 失败%d次, 成功率%.1f%% (HCSR04库模式)\n", 
               totalMeasurements, successCount, failureCount, successRate);
      lastStatsReport = millis();
    }

    // 自适应任务延迟
    vTaskDelay(pdMS_TO_TICKS(currentDelay));
  }
}

/**
 * @brief 测试HCSR04基础功能
 * @return true表示测试通过，false表示测试失败
 */
bool testHCSR04BasicFunction()
{
  ei_printf("开始HCSR04基础功能测试...\n");
  
  int successCount = 0;
  int totalTests = 5;
  
  for (int i = 0; i < totalTests; i++)
  {
    ei_printf("测试 %d/%d: ", i + 1, totalTests);
    
    float distance = hc.dist();
    
    if (distance > 0 && distance <= 400)
    {
      // ei_printf("成功 - 距离: %.2f cm\n", distance);
      successCount++;
    }
    else
    {
      ei_printf("失败 - 返回值: %.2f\n", distance);
    }
    
    delay(500); // 等待500ms再进行下一次测试
  }
  
  float successRate = (float)successCount / totalTests * 100;
  ei_printf("HCSR04测试完成: %d/%d 成功, 成功率: %.1f%%\n", successCount, totalTests, successRate);
  
  return successCount >= 3; // 至少3次成功才算通过
}

/**
 * @brief 测量超声波距离（保留兼容性）
 * @return 距离值（厘米），如果测量失败返回-1
 */
float measureUltrasonicDistance()
{
  // 使用HCSR04库获取距离数据
  float distance = hc.dist();
  
  // HCSR04库返回0表示测量失败或超出范围
  if (distance <= 0)
  {
    ei_printf("超声波测量失败: 返回值 %.2f\n", distance);
    return -1; // 返回-1表示测量失败
  }
  
  // 验证距离范围 (HC-SR04 有效范围: 2-400cm)
  if (distance < 2 || distance > 400)
  {
    ei_printf("超声波测量超出范围");
    return -1;
  }
  
  return distance;
}

/**
 * @brief 触发障碍物警报
 * @param distance 检测到的距离
 */
void triggerObstacleAlert(float distance)
{
  // Serial.printf("警告: 检测到障碍物，距离 %.1f cm\n", distance);

  // 振动和蜂鸣器警报
  digitalWrite(VIBRATION_MODULE_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  digitalWrite(BUZZER_PIN, HIGH);
  vTaskDelay(750 / portTICK_PERIOD_MS);
  digitalWrite(VIBRATION_MODULE_PIN, LOW);
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

/**
 * @brief 按钮检测任务
 * 监听按钮按下事件，触发语音交互
 * @param parameter 任务参数（未使用）
 */
void buttonTask(void *parameter)
{
  ei_printf("按钮检测任务启动\n");
  ei_printf("按钮引脚: %d\n", TEST_BUTTON_PIN);
  
  // 添加任务启动确认
  int buttonCheckCount = 0;

  while (1)
  {
    int buttonState = digitalRead(TEST_BUTTON_PIN);
    
    // 每10秒输出一次按钮状态用于调试
    // if (buttonCheckCount % 100 == 0) {
    //   ei_printf("[按钮任务] 按钮状态: %s (引脚%d)\n", 
    //             buttonState == LOW ? "按下" : "释放", TEST_BUTTON_PIN);
    // }
    // buttonCheckCount++;
    
    if (buttonState == LOW)
    {
      ei_printf("[按钮任务] 检测到按钮按下！\n");
      handleButtonPress();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief 处理按钮按下事件
 */
void handleButtonPress()
{
  ei_printf("[按钮处理] 开始处理按钮按下事件\n");
  
  // 防抖动延时
  vTaskDelay(100 / portTICK_PERIOD_MS);

  // 确认按钮仍然被按下
  int buttonState = digitalRead(TEST_BUTTON_PIN);
  // ei_printf("[按钮处理] 防抖后按钮状态: %s\n", buttonState == LOW ? "按下" : "释放");
  
  if (buttonState == LOW)
  {
    ei_printf("[按钮触发] 按钮确认被按下，启动语音交互\n");
    // ei_printf("[按钮触发] 触发前状态 - record_status: %s, record_status_me: %s\n", 
    //           record_status ? "true" : "false", record_status_me ? "true" : "false");

    digitalWrite(LED_BUILT_IN, HIGH); // 点亮LED指示
    // ei_printf("[按钮触发] LED已点亮\n");

    // 播放确认音频
    // ei_printf("[按钮触发] 播放唤醒响应音频\n");
    playAudio_Zai();

    // 设置语音交互标志
    // ei_printf("[按钮触发] 设置语音交互触发标志\n");
    record_status = false;
    record_status_me = false;
    
    // ei_printf("[按钮触发] 触发后状态 - record_status: %s, record_status_me: %s\n", 
    //           record_status ? "true" : "false", record_status_me ? "true" : "false");

    // 额外延时避免重复触发
    vTaskDelay(300 / portTICK_PERIOD_MS);
    
    ei_printf("[按钮触发] 按钮触发的语音交互准备完成\n");
  }
  else
  {
    ei_printf("[按钮处理] 防抖后按钮已释放，忽略此次触发\n");
  }
}

/**
 * @brief 语音交互任务
 * 处理语音录制、识别和合成的完整流程
 * @param parameter 任务参数（未使用）
 */
void voiceTask(void *parameter)
{
  ei_printf("[语音任务] 语音交互任务启动\n");

  while (1)
  {
    // 检查是否有语音交互触发（唤醒词或按钮）
    if (!record_status || !record_status_me)
    {
      ei_printf("[语音任务] 检测到语音交互触发信号\n");
      // ei_printf("[语音任务] 触发前状态 - record_status: %s, record_status_me: %s\n", 
      //           record_status ? "true" : "false", record_status_me ? "true" : "false");
      
      // 执行完整的语音交互流程
      handleVoiceInteraction();
      
      // 语音交互完成后，重置状态以等待下次唤醒
      ei_printf("[语音任务] 语音交互完成，重置状态等待下次唤醒\n");
      resetRecordStatus();
      
      ei_printf("[语音任务] 等待下次触发信号...\n");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // 增加延时，减少CPU占用
  }
}

/**
 * @brief 处理完整的语音交互流程
 */
void handleVoiceInteraction()
{
  ei_printf("[语音交互] 开始语音交互流程\n");
  // ei_printf("[语音交互] 当前状态 - record_status: %s, record_status_me: %s\n", 
  //           record_status ? "true" : "false", record_status_me ? "true" : "false");

  // 分配音频缓冲区内存
  // ei_printf("[语音交互] 分配音频数据缓冲区\n");
  uint8_t *pcm_data = (uint8_t *)ps_malloc(BUFFER_SIZE);
  if (!pcm_data)
  {
    ei_printf("[语音交互] 错误:内存分配失败，退出语音交互\n");
    digitalWrite(LED_BUILT_IN, LOW);
    resetRecordStatus();
    return;
  }

  // 执行音频录制
  ei_printf("[语音交互] 开始音频录制\n");
  size_t recordingSize = performAudioRecording(pcm_data);
  Serial.printf("[语音交互] 音频录制完成，录制大小: %d 字节\n", recordingSize);

  // 重置录音状态
  ei_printf("[语音交互] 重置录音状态\n");
  resetRecordStatus();

  // 检查录音质量
  if (!isValidRecording(recordingSize))
  {
    ei_printf("[语音交互] 录音质量不佳，退出语音交互\n");
    free(pcm_data);
    digitalWrite(LED_BUILT_IN, LOW);
    return;
  }
  
  ei_printf("[语音交互] 开始语音识别和处理\n");
  
  // 记录开始时间，用于超时检测
  unsigned long startTime = millis();
  const unsigned long VOICE_PROCESSING_TIMEOUT = 30000; // 30秒超时
  
  // 执行语音识别和处理
  processVoiceRecognition(pcm_data, recordingSize);
  
  unsigned long endTime = millis();
  unsigned long processingTime = endTime - startTime;
  
  ei_printf("[语音交互] 语音处理耗时: %lu 毫秒\n", processingTime);
  
  // 检查是否超时
  if (processingTime > VOICE_PROCESSING_TIMEOUT) {
    ei_printf("[语音交互] 警告:语音处理超时 (%lu ms > %lu ms)\n", processingTime, VOICE_PROCESSING_TIMEOUT);
  }

  // 释放内存
  // ei_printf("[语音交互] 释放音频缓冲区内存\n");
  free(pcm_data);

  // 确保LED被关闭
  digitalWrite(LED_BUILT_IN, LOW);
  digitalWrite(LED_BUILTIN, LOW);  // 同时关闭外置LED
  // ei_printf("[语音交互] 关闭录音指示LED\n");

  // 强制重置所有状态，确保系统能够恢复正常
  record_status = true;
  record_status_me = true;
  // ei_printf("[语音交互] 强制重置系统状态，恢复唤醒词检测\n");

  ei_printf("[语音交互] 语音交互流程完成\n");
}

/**
 * @brief 执行音频录制
 * @param pcm_data 音频数据缓冲区
 * @return 录制的音频数据大小
 */
size_t performAudioRecording(uint8_t *pcm_data)
{
  ei_printf("[录音] 开始音频录制...\n");
  digitalWrite(LED_BUILT_IN, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);  // 同时点亮外置LED

  size_t bytes_read = 0, recordingSize = 0;
  int16_t data[512];
  size_t noVoicePre = 0, noVoiceCur = 0, noVoiceTotal = 0, VoiceCnt = 0;
  bool recording = true;
  size_t initialBufferPeriod = 0; // 初始缓冲期计数器
  unsigned long recordingStartTime = millis(); // 录音开始时间
  const unsigned long MAX_RECORDING_TIME_MS = 15000; // 最大录音时间15秒

  while (recording)
  {
    noVoicePre = millis();

    // 从I2S读取音频数据
    esp_err_t result = i2s_read(I2S_IN_PORT, data, sizeof(data), &bytes_read, portMAX_DELAY);
    
    if (result != ESP_OK)
    {
      Serial.printf("[录音] I2S读取错误: %s\n", esp_err_to_name(result));
      break;
    }

    // 将音频数据复制到缓冲区
    memcpy(pcm_data + recordingSize, data, bytes_read);
    recordingSize += bytes_read;

    // 计算音频能量以检测语音活动
    uint32_t audioEnergy = calculateAudioEnergy(data, bytes_read);

    // 增加初始缓冲期计数
    initialBufferPeriod++;
    
    // 减少初始缓冲期，提高响应速度（给用户1秒时间开始说话）
    if (initialBufferPeriod > 30) // 约1秒的缓冲时间
    {
      // 更新语音检测状态
      updateVoiceDetection(audioEnergy, &noVoicePre, &noVoiceCur, &noVoiceTotal, &VoiceCnt);
    }
    else
    {
      // 在缓冲期内，重置静音计数
      noVoiceTotal = 0;
      // 在缓冲期内也检测语音活动，但不累加静音计数
      if (audioEnergy > 300)
      {
        VoiceCnt++;
        Serial.printf("[录音] 缓冲期检测到语音活动，能量: %d\n", audioEnergy);
      }
    }

    // 每隔一段时间输出录音状态
    if (recordingSize % 10240 == 0) // 每10KB输出一次状态
    {
      Serial.printf("[录音] 录音进行中... 已录制: %d 字节, 音频能量: %d, 静音计数: %d\n", recordingSize, audioEnergy, noVoiceTotal);
    }

    // 检查录音结束条件
    if (shouldStopRecording(noVoiceTotal, recordingSize))
    {
      recording = false;
      ei_printf("[录音] 检测到静音持续时间过长，停止录音\n");
      ei_printf("[录音] 录音完成\n");
    }
    
    // 检查录音超时
    if (millis() - recordingStartTime > MAX_RECORDING_TIME_MS)
    {
      recording = false;
      ei_printf("[录音] 录音超时，强制停止录音\n");
      ei_printf("[录音] 录音时长: %lu 毫秒\n", millis() - recordingStartTime);
    }
  }

  digitalWrite(LED_BUILT_IN, LOW);
  digitalWrite(LED_BUILTIN, LOW);  // 同时关闭外置LED
  return recordingSize;
}

/**
 * @brief 计算音频能量
 * @param data 音频数据
 * @param bytes_read 数据字节数
 * @return 音频能量值
 */
uint32_t calculateAudioEnergy(int16_t *data, size_t bytes_read)
{
  uint32_t sum_data = 0;
  for (int i = 0; i < bytes_read / 2; i++)
  {
    sum_data += abs(data[i]);
  }
  return sum_data / bytes_read;
}

/**
 * @brief 更新语音检测状态
 */
void updateVoiceDetection(uint32_t audioEnergy, size_t *noVoicePre, size_t *noVoiceCur,
                          size_t *noVoiceTotal, size_t *VoiceCnt)
{
  // 优化语音活动检测阈值
  const uint32_t VOICE_THRESHOLD = 300; // 进一步降低阈值，提高敏感度
  const size_t SILENCE_CONFIRM_COUNT = 3; // 连续3次低能量才确认静音

  if (audioEnergy > VOICE_THRESHOLD)
  {
    *VoiceCnt += 1;
    *noVoiceCur = 0;
    // 检测到语音时，重置静音计数
    if (*noVoiceTotal > 0)
    {
      // Serial.printf("[语音检测] 检测到语音活动，重置静音计数\n");
      *noVoiceTotal = 0;
    }
  }
  else
  {
    *noVoiceCur += 1;
    // 减少静音确认所需的连续次数，提高响应速度
    if (*noVoiceCur >= SILENCE_CONFIRM_COUNT)
    {
      *noVoiceTotal += 1;
      *noVoiceCur = 0; // 重置当前静音计数，避免重复累加
    }
  }

  // 每隔一定时间输出检测状态
  static int debugCounter = 0;
  debugCounter++;
  if (debugCounter % 15 == 0) // 增加输出频率，每15次输出一次
  {
    Serial.printf("[语音检测] 音频能量: %d, 语音计数: %d, 当前静音: %d, 总静音: %d\n", 
                  audioEnergy, *VoiceCnt, *noVoiceCur, *noVoiceTotal);
  }
}

/**
 * @brief 检查是否应该停止录音
 */
bool shouldStopRecording(size_t noVoiceTotal, size_t recordingSize)
{
  
  const size_t MIN_RECORDING_SIZE = 16000; // 至少1秒的音频数据
  const size_t MAX_RECORDING_SIZE = 160000; // 最大10秒录音
  const size_t SILENCE_THRESHOLD = 30; // 减少静音阈值到30个周期（约1秒）
  
  bool shouldStop = false;
  
  // 检查是否达到最大录音时间
  if (recordingSize >= MAX_RECORDING_SIZE)
  {
    shouldStop = true;
    Serial.printf("[录音控制] 达到最大录音时间，强制停止 - 录制大小: %d\n", recordingSize);
  }
  // 检查缓冲区是否快满
  else if (recordingSize >= BUFFER_SIZE - 1024)
  {
    shouldStop = true;
    Serial.printf("[录音控制] 缓冲区即将满，停止录音 - 录制大小: %d\n", recordingSize);
  }
  // 检查静音持续时间（只有在录制足够长度后才检查）
  else if (recordingSize > MIN_RECORDING_SIZE && noVoiceTotal > SILENCE_THRESHOLD)
  {
    shouldStop = true;
    Serial.printf("[录音控制] 检测到静音持续时间过长，停止录音 - 静音计数: %d, 录制大小: %d\n", noVoiceTotal, recordingSize);
  }
  
  return shouldStop;
}

/**
 * @brief 重置录音状态
 */
void resetRecordStatus()
{
  ei_printf("[状态重置] 重置录音状态为待机模式\n");
  record_status = true;
  record_status_me = true;
  // ei_printf("[状态重置] 状态重置完成 - record_status: %s, record_status_me: %s\n", 
  //           record_status ? "true" : "false", record_status_me ? "true" : "false");
}

/**
 * @brief 检查录音是否有效
 */
bool isValidRecording(size_t recordingSize)
{
  const size_t MIN_RECORDING_SIZE = 8000; // 至少0.5秒的音频数据
  bool isValid = recordingSize > MIN_RECORDING_SIZE;
  
  // ei_printf("[录音验证] 录音大小: %d 字节, 最小要求: %d 字节, 有效性: %s\n", 
  //               recordingSize, MIN_RECORDING_SIZE, isValid ? "有效" : "无效");
  
  return isValid;
}

/**
 * @brief 处理语音识别和响应
 */
void processVoiceRecognition(uint8_t *pcm_data, size_t recordingSize)
{
  ei_printf("[语音识别] 开始语音识别处理\n");
  
  if (recordingSize > 0)
  {
    // 语音识别
    // ei_printf("[语音识别] 调用百度语音识别API\n");
    String recognizedText = "";
    
    // 添加异常处理，防止网络请求导致系统挂起
    try {
      recognizedText = baidu_voice_recognition(accessToken, pcm_data, recordingSize);
      ei_printf("[语音识别] 识别结果: %s\n", recognizedText.c_str());
    } catch (...) {
      ei_printf("[语音识别] 错误:语音识别API调用异常\n");
      recognizedText = "";
    }

    if (recognizedText.length() > 0)
    {
      // 大模型处理
      ei_printf("[AI对话] 发送文本到AI服务器\n");
      String response = "";
      
      try {
        response = sendTextToServer(recognizedText);
        ei_printf("[AI对话] AI回复: %s\n", response.c_str());
      } catch (...) {
        ei_printf("[AI对话] 错误:AI服务器请求异常\n");
        response = "";
      }

      if (response.length() > 0)
      {
        // 尝试解析JSON响应
        DynamicJsonDocument responseDoc(2048);
        DeserializationError error = deserializeJson(responseDoc, response);
        
        String textToSpeak = "";
        bool isNavigationResponse = false;
        
        if (!error) {
          // 成功解析JSON
          if (responseDoc.containsKey("response")) {
            textToSpeak = responseDoc["response"].as<String>();
          }
          
          // 检查是否是导航响应
          if (responseDoc.containsKey("navigation_started") && responseDoc["navigation_started"].as<bool>()) {
            isNavigationResponse = true;
            ei_printf("[导航] 检测到导航开始响应\n");
            
            if (responseDoc.containsKey("destination")) {
              String destination = responseDoc["destination"].as<String>();
              ei_printf("[导航] 目的地: %s\n", destination.c_str());
            }
          }
        } else {
          // JSON解析失败，直接使用原始响应
          textToSpeak = response;
          ei_printf("[响应解析] JSON解析失败，使用原始响应\n");
        }
        
        if (textToSpeak.length() > 0) {
          // 语音合成期间暂停唤醒词检测
          ei_printf("[语音合成] 开始语音合成，暂停唤醒词检测\n");
          record_status = false;
          record_status_me = false;
          
          try {
            baiduTTS_Send(accessToken, textToSpeak);
            ei_printf("[语音合成] 语音合成完成\n");
          } catch (...) {
            ei_printf("[语音合成] 错误:语音合成API调用异常\n");
          }
          
          // 如果是导航响应，启动导航更新任务
           if (isNavigationResponse) {
             ei_printf("[导航] 导航已开始，将定期更新导航状态\n");
             if (responseDoc.containsKey("destination")) {
               String destination = responseDoc["destination"].as<String>();
               startNavigation(destination);
             }
           }
          
          // 语音合成完成后，不在这里恢复状态，让handleVoiceInteraction统一管理
          ei_printf("[语音合成] 语音合成完成，状态将由主流程统一管理\n");
        }
      }
      else
      {
        ei_printf("[AI对话] 警告:AI服务器未返回有效回复\n");
      }
    }
    else
    {
      ei_printf("[语音识别] 警告:语音识别未返回有效结果\n");
    }
  }
  else
  {
    ei_printf("[语音识别] 错误:录音数据为空\n");
  }
  
  // 确保在所有情况下都恢复状态（异常处理的最后保障）
  // ei_printf("[语音识别] 确保状态恢复 - 当前状态: record_status=%s, record_status_me=%s\n", 
  //           record_status ? "true" : "false", record_status_me ? "true" : "false");
  
  ei_printf("[语音识别] 语音识别处理完成\n");
}

/**
 * @brief 启用连续对话模式
 */
void enableContinuousConversation()
{
  ei_printf("[连续对话] 语音交互完成，等待下次唤醒\n");
  // 不再自动触发下次对话，需要重新唤醒
  record_status = true;
  record_status_me = true;
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

/**
 * @brief 启动导航
 * @param destination 目的地
 */
void startNavigation(String destination)
{
  ei_printf("[导航] 启动导航到: %s\n", destination.c_str());
  navigationActive = true;
  currentDestination = destination;
  lastNavigationUpdate = millis();
  
  // 立即获取第一次导航指令
  updateNavigationStatus();
}

/**
 * @brief 更新导航状态
 */
void updateNavigationStatus()
{
  if (!navigationActive || currentDestination.isEmpty()) {
    return;
  }
  
  ei_printf("[导航] 更新导航状态\n");
  
  HTTPClient http;
  http.begin("http://101.132.166.117:12345/navigation_update");
  http.addHeader("Content-Type", "application/json");
  
  // 构建请求JSON
  DynamicJsonDocument doc(1024);
  doc["destination"] = currentDestination;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.POST(requestBody);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    ei_printf("[导航] 服务器响应: %s\n", response.c_str());
    
    // 解析响应
    DynamicJsonDocument responseDoc(2048);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("next_instruction")) {
        String instruction = responseDoc["next_instruction"].as<String>();
        ei_printf("[导航] 下一步指令: %s\n", instruction.c_str());
        
        // 语音播报导航指令
        baiduTTS_Send(accessToken, instruction.c_str()); // 使用中文语音合成,语速设为5
      }
      
      if (responseDoc.containsKey("navigation_complete") && responseDoc["navigation_complete"].as<bool>()) {
        ei_printf("[导航] 导航完成\n");
        stopNavigation();
      }
    }
  } else {
    ei_printf("[导航] 更新导航状态失败，HTTP错误码: %d\n", httpResponseCode);
  }
  
  http.end();
  lastNavigationUpdate = millis();
}

/**
 * @brief 停止导航
 */
void stopNavigation()
{
  ei_printf("[导航] 停止导航\n");
  navigationActive = false;
  currentDestination = "";
  lastNavigationUpdate = 0;
  
  // 通知后端退出导航
  HTTPClient http;
  http.begin("http://101.132.166.117:12345/exit_navigation");
  http.addHeader("Content-Type", "application/json");
  
  DynamicJsonDocument doc(256);
  doc["action"] = "exit_navigation";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.POST(requestBody);
  
  if (httpResponseCode > 0) {
    ei_printf("[导航] 成功通知后端退出导航\n");
  } else {
    ei_printf("[导航] 通知后端退出导航失败，HTTP错误码: %d\n", httpResponseCode);
  }
  
  http.end();
}

/**
 * @brief 检查是否需要更新导航
 */
void checkNavigationUpdate()
{
  if (navigationActive && (millis() - lastNavigationUpdate) >= NAVIGATION_UPDATE_INTERVAL) {
    updateNavigationStatus();
  }
}

// ==================== 音频推理回调系统 ====================
/**
 * @brief 音频推理回调函数
 * 将采集到的音频数据存储到推理缓冲区
 * @param n_bytes 音频数据字节数
 */
static void audio_inference_callback(uint32_t n_bytes)
{
  // 将16位音频样本复制到推理缓冲区
  int samples_count = n_bytes >> 1; // 除以2，因为每个样本是16位（2字节）

  for (int i = 0; i < samples_count; i++)
  {
    // 检查缓冲区是否还有空间
    if (inference.buf_count >= inference.n_samples)
    {
      // 缓冲区已满，标记为就绪
      inference.buf_count = 0;
      inference.buf_ready = 1;
      break;
    }

    // 将音频样本存储到推理缓冲区
    inference.buffer[inference.buf_count++] = sampleBuffer[i];
  }

  // 如果缓冲区已满，标记为就绪
  if (inference.buf_count >= inference.n_samples)
  {
    inference.buf_count = 0;
    inference.buf_ready = 1;
  }
}

/**
 * @brief 音频采集任务
 * 持续从I2S接口读取音频数据并处理
 * @param arg 任务参数，包含每次读取的字节数
 */
static void capture_samples(void *arg)
{
  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = 0;
  static int debug_counter = 0;
  static unsigned long last_debug_time = 0;

  ei_printf("[音频调试] 音频采集任务已启动，每次读取 %d 字节\n", i2s_bytes_to_read);
  ei_printf("[音频调试] I2S端口: %d, 缓冲区大小: %d\n", I2S_IN_PORT, sample_buffer_size);

  while (record_status)
  {
    // 从I2S接口读取音频数据
    esp_err_t result = i2s_read(I2S_IN_PORT, (void *)sampleBuffer,
                                i2s_bytes_to_read, &bytes_read, 100);

    // 检查读取结果
    if (result != ESP_OK || bytes_read <= 0)
    {
      ei_printf("[音频调试] I2S读取错误: %d, 字节数: %d\n", result, bytes_read);
      continue;
    }

    // 每5秒输出一次调试信息
    unsigned long current_time = millis();
    if (current_time - last_debug_time > 5000)
    {
      ei_printf("[音频调试] I2S读取正常: %d 字节，样本数: %d，计数器: %d\n", 
                bytes_read, bytes_read/2, debug_counter);
      last_debug_time = current_time;
    }
    debug_counter++;

    // 检查是否为部分读取
    if (bytes_read < i2s_bytes_to_read)
    {
      ei_printf("[音频调试] I2S部分读取: %d/%d 字节\n", bytes_read, i2s_bytes_to_read);
    }

    // 音频数据增益处理（放大8倍以提高音量）
    amplifyAudioData(sampleBuffer, bytes_read);

    // 如果仍在录音状态，处理音频数据
    if (record_status)
    {
      audio_inference_callback(bytes_read);
    }
    else
    {
      break;
    }
  }

  ei_printf("[音频调试] 音频采集任务结束\n");
  vTaskDelete(NULL);
}

/**
 * @brief 放大音频数据
 * @param buffer 音频数据缓冲区
 * @param bytes_read 读取的字节数
 */
void amplifyAudioData(int16_t *buffer, size_t bytes_read)
{
  const int16_t NOISE_THRESHOLD = 100; // 噪声阈值，低于此值的信号视为噪声
  const int AMPLIFY_FACTOR = 4; // 降低放大倍数从8倍到4倍，减少噪声放大
  
  // 将音频数据放大并进行噪声抑制
  for (int x = 0; x < bytes_read / 2; x++)
  {
    // 噪声抑制:如果信号幅度小于噪声阈值，则将其设为0
    if (abs(buffer[x]) < NOISE_THRESHOLD)
    {
      buffer[x] = 0;
      continue;
    }
    
    // 防止溢出的安全放大
    int32_t amplified = (int32_t)buffer[x] * AMPLIFY_FACTOR;
    if (amplified > INT16_MAX)
    {
      buffer[x] = INT16_MAX;
    }
    else if (amplified < INT16_MIN)
    {
      buffer[x] = INT16_MIN;
    }
    else
    {
      buffer[x] = (int16_t)amplified;
    }
  }
}

/**
 * @brief 初始化推理结构体并启动音频采集
 * @param n_samples 采样数量
 * @return true表示初始化成功，false表示失败
 */
static bool microphone_inference_start(uint32_t n_samples)
{
  ei_printf("[音频调试] 开始初始化麦克风推理，样本数: %d\n", n_samples);
  
  // 分配推理缓冲区内存
  inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

  if (inference.buffer == NULL)
  {
    ei_printf("[音频调试] 错误: 无法分配推理缓冲区内存\n");
    return false;
  }
  
  ei_printf("[音频调试] 内存分配成功，地址: %p，大小: %d 字节\n", 
            inference.buffer, n_samples * sizeof(int16_t));

  // 初始化推理结构体参数
  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  // 等待系统稳定
  ei_sleep(100);

  // 启用录音状态
  record_status = true;
  ei_printf("[音频调试] 录音状态已启用\n");

  // 创建音频采集任务
  BaseType_t taskResult = xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void *)sample_buffer_size, 10, NULL);
  
  if (taskResult != pdPASS)
  {
    ei_printf("[音频调试] 错误: 无法创建音频采集任务，返回值: %d\n", taskResult);
    return false;
  }
  
  ei_printf("[音频调试] 音频采集任务创建成功\n");
  ei_printf("麦克风推理系统已启动，采样数量: %d\n", n_samples);
  return true;
}

// ==================== 音频推理系统 ====================
/**
 * @brief 等待新的音频数据准备就绪
 * @return true表示数据准备完成，false表示失败
 */
static bool microphone_inference_record(void)
{
  bool ret = true;
  unsigned long startTime = millis();
  const unsigned long timeout = 5000; // 5秒超时

  // 等待缓冲区数据准备就绪，添加超时保护
  while (inference.buf_ready == 0)
  {
    delay(10);
    
    // 检查超时
    if (millis() - startTime > timeout)
    {
      ei_printf("[唤醒检测] 音频缓冲区等待超时，可能需要重新初始化音频系统\n");
      return false;
    }
  }

  // 重置缓冲区就绪标志
  inference.buf_ready = 0;
  return ret;
}

/**
 * @brief 获取原始音频信号数据
 * @param offset 数据偏移量
 * @param length 数据长度
 * @param out_ptr 输出数据指针
 * @return 0表示成功
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
  // 将16位整数音频数据转换为浮点数
  numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

  return 0;
}

/**
 * @brief 停止麦克风推理并释放缓冲区
 * 清理音频推理系统占用的内存资源
 */
static void microphone_inference_end(void)
{
  // 停止音频采集任务
  record_status = false;
  
  // 等待采集任务结束
  delay(200);
  
  // 释放推理缓冲区（sampleBuffer是静态分配的，不需要释放）
  if (inference.buffer != NULL)
  {
    ei_free(inference.buffer);
    inference.buffer = NULL;
  }
  
  // 重置推理状态
  inference.buf_ready = 0;
  inference.buf_count = 0;
  inference.n_samples = 0;

  ei_printf("[唤醒检测] 音频推理系统已停止，缓冲区已释放\n");
}

/**
 * @brief 光敏传感器任务
 * 根据环境光线自动控制照明
 * @param pvParameters 任务参数（未使用）
 */
void lightSensorTask(void *pvParameters)
{
  ei_printf("光敏传感器任务启动\n");

  static bool lastLightState = false;
  static int debugCounter = 0;

  while (1)
  {
    // 监控栈使用情况（每100次循环输出一次）
    if (debugCounter % 100 == 0) {
      UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
      // ei_printf("[光敏传感器] 栈剩余: %d 字节\n", stackHighWaterMark * sizeof(StackType_t));
    }
    debugCounter++;

    bool currentLightState = checkLightCondition();

    // 只在状态改变时执行操作，避免重复输出
    if (currentLightState != lastLightState)
    {
      controlLighting(currentLightState);
      lastLightState = currentLightState;
    }

    // 每隔一段时间检测一次
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief 检查光线条件
 * @return true表示黑暗，false表示明亮
 */
bool checkLightCondition()
{
  // 读取光敏传感器的数字输出值
  // 传感器在黑暗时输出 HIGH，在明亮时输出 LOW
  return digitalRead(LIGHT_SENSOR_DO_PIN) == HIGH;
}

/**
 * @brief 控制照明设备
 * @param isDark true表示黑暗需要开灯，false表示明亮需要关灯
 */
void controlLighting(bool isDark)
{
  if (isDark)
  {
    // 检测到黑暗环境，打开灯
    digitalWrite(LIGHT_CONTROL_PIN, HIGH);
    ei_printf("检测到黑暗，开灯\n");
  }
  else
  {
    // 检测到明亮环境，关闭灯
    digitalWrite(LIGHT_CONTROL_PIN, LOW);
    ei_printf("检测到明亮，关灯\n");
  }
}

/**
 * @brief GPS定位任务
 * 持续获取GPS数据并上传到服务器
 * @param pvParameters 任务参数（未使用）
 */
void gpsTask(void *pvParameters)
{
  ei_printf("GPS任务启动\n");

  while (1)
  {
    // 解析GPS数据
    bool gpsDataValid = processGpsData();

    // 检查网络连接和GPS数据有效性
    if (isConnectedToWifi && gpsDataValid)
    {
      uploadGpsData();
    }
    else
    {
      handleGpsError();
    }

    // 任务延时，每5秒执行一次
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief 处理GPS数据
 * @return true表示GPS数据有效，false表示无效
 */
bool processGpsData()
{
  parseGpsBuffer(); // 解析GPS串口数据

  // 检查GPS数据有效性
  return (originLat != 0.0 && originLng != 0.0);
}

/**
 * @brief 上传GPS数据到服务器
 */
void uploadGpsData()
{
  Serial.printf("获取到GPS坐标: Lat=%.6f, Lon=%.6f\n", originLat, originLng);

  // 发送GPS数据到服务器
  sendGpsData(originLat, originLng);
  ei_printf("GPS数据已发送\n");
}

/**
 * @brief 处理GPS错误情况
 */
void handleGpsError()
{
  if (!isConnectedToWifi)
  {
    ei_printf("GPS任务:WiFi未连接\n");
  }
  else
  {
    // ei_printf("GPS任务:等待有效GPS数据...\n");
  }
}


// #include <Arduino.h>
// #include <WiFi.h>
// #include <WebSocketsClient.h>
// #include <ArduinoJson.h>

// // WiFi credentials - REPLACE WITH YOUR NETWORK DETAILS
// const char *ssid = "sharp_caterpillar";
// const char *password = "zzn20041031";

// // WebSocket server details
// const char *websocket_server_host = "192.168.102.212"; // REPLACE WITH YOUR SERVER IP
// uint16_t websocket_server_port = 8765;

// // WebSocket client instance
// WebSocketsClient webSocket;

// // LED state
// bool ledState = false;

// void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
// void handleCommand(String command);
// void sendLEDStatus();

// void setup()
// {
//   // Initialize Serial communication
//   Serial.begin(115200);
//   ei_printf("ESP32 WebSocket LED Control Starting...");

//   // Initialize built-in LED
//   pinMode(LED_BUILTIN, OUTPUT);
//   digitalWrite(LED_BUILTIN, LOW);

//   // Connect to WiFi
//   WiFi.begin(ssid, password);
//   Serial.print("Connecting to WiFi");

//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//   }

//   ei_printf();
//   ei_printf("WiFi connected!");
//   Serial.print("IP address: ");
//   ei_printf(WiFi.localIP());

//   // Configure WebSocket client
//   webSocket.begin(websocket_server_host, websocket_server_port, "/");
//   webSocket.onEvent(webSocketEvent);
//   webSocket.setReconnectInterval(5000);

//   ei_printf("WebSocket client configured");
// }

// void loop()
// {
//   // Handle WebSocket events
//   webSocket.loop();

//   // Small delay to prevent watchdog issues
//   delay(10);
// }

// void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
// {
//   switch (type)
//   {
//   case WStype_DISCONNECTED:
//     ei_printf("WebSocket Disconnected");
//     break;

//   case WStype_CONNECTED:
//     Serial.printf("WebSocket Connected to: %s\n", payload);
//     // Send initial status
//     sendLEDStatus();
//     break;

//   case WStype_TEXT:
//     Serial.printf("Received command: %s\n", payload);
//     handleCommand((char *)payload);
//     break;

//   case WStype_ERROR:
//     Serial.printf("WebSocket Error: %s\n", payload);
//     break;

//   default:
//     break;
//   }
// }

// void handleCommand(String command)
// {
//   command.trim();

//   if (command == "ON")
//   {
//     digitalWrite(LED_BUILTIN, HIGH);
//     ledState = true;
//     ei_printf("LED turned ON");
//   }
//   else if (command == "OFF")
//   {
//     digitalWrite(LED_BUILTIN, LOW);
//     ledState = false;
//     ei_printf("LED turned OFF");
//   }
//   else
//   {
//     ei_printf("Unknown command: " + command);
//     return;
//   }

//   // Send status update back to server
//   sendLEDStatus();
// }

// void sendLEDStatus()
// {
//   // Create JSON status message
//   StaticJsonDocument<100> doc;
//   doc["status"] = ledState ? "ON" : "OFF";

//   String statusMessage;
//   serializeJson(doc, statusMessage);

//   // Send to WebSocket server
//   webSocket.sendTXT(statusMessage);
//   ei_printf("Sent status: " + statusMessage);
// }
