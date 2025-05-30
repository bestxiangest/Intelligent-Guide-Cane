#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <_3_inferencing.h>

// FreeRTOS相关头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "config.h" // 项目配置文件#include <Arduino.h>

#include "network.h"
#include "voice.h"
#include "gps.h"

// 唤醒词
#define SAMPLE_RATE 16000U
#define LED_BUILT_IN 21
#define EIDSP_QUANTIZE_FILTERBANK 0
// 唤醒词阈值，阈值越大，要求识别的唤醒词更精准
#define PRED_VALUE_THRESHOLD 0.6

// 函数声明
void initHardware(); // 初始化硬件
void setupTasks();   // 设置并启动任务

// 任务函数声明
void ultrasonicTask(void *pvParameters);  // 超声波任务
void voiceTask(void *pvParameters);       // 语音任务
void buttonTask(void *pvParameters);      // 按钮任务
void lightSensorTask(void *pvParameters); // 光敏传感器任务 (新增)
void gpsTask(void *pvParameters);         // GPS任务 (新增)

// 唤醒词
static bool microphone_inference_record(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static void microphone_inference_end(void);
static bool microphone_inference_start(uint32_t n_samples);

// 定义变量
long duration;    // 声波传播时间
int distanceCm;   // 计算出的距离（厘米）
int distanceInch; // 计算出的距离（英寸）

/** Audio buffers, pointers and selectors */
typedef struct
{
    int16_t *buffer;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool record_status = true;

// 播放响应音频
void playAudio_Zai();

void setup()
{
    Serial.begin(115200);
    Serial.println("星辰引路者 - 启动中...");

    // 初始化硬件
    initHardware();
    // 初始化WiFi
    initWiFi();
    // 初始化I2S
    set_i2s();
    // 初始化GPS
    gpsInit();

    // 创建队列和信号量
    alertQueue = xQueueCreate(10, sizeof(AlertInfo));
    Serial.printf("队列创建成功...");
    wifiMutex = xSemaphoreCreateMutex();
    Serial.println("WiFi互斥锁创建成功...");

    // 设置并启动任务
    Serial.printf("正在启动任务...");
    setupTasks();

    accessToken = getAccessToken_baidu(); // 获取百度语音识别的access token

    Serial.println("系统初始化完成，开始运行...");

    // 唤醒词
    while (!Serial)
        ;
    Serial.println("Edge Impulse Inferencing Demo");

    pinMode(LED_BUILT_IN, OUTPUT);   // Set the pin as output
    digitalWrite(LED_BUILT_IN, LOW); // Turn off

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: ");
    ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf(" ms.\n");
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

    ei_printf("\nStarting continious inference in 2 seconds...\n");
    ei_sleep(2000);

    if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false)
    {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }

    ei_printf("Recording...\n");
}

void loop()
{
    vTaskDelay(10 / portTICK_PERIOD_MS); // 延迟10毫秒

    // 唤醒词
    bool m = microphone_inference_record();
    if (!m)
    {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK)
    {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    int pred_index = -1;                     // Initialize pred_index
    float pred_value = PRED_VALUE_THRESHOLD; // Initialize pred_value

    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
              result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        ei_printf("    %s: ", result.classification[ix].label);
        ei_printf_float(result.classification[ix].value);
        ei_printf("\n");

        // 唤醒词在第一位，此时判断classification[0]位置大于阈值表示唤醒
        if (result.classification[0].value > pred_value)
        {
            pred_index = 0;
        }
    }
    // Display inference result
    if (pred_index == 0)
    {
        digitalWrite(LED_BUILTIN, HIGH); // Turn on

        Serial.println("playAudio_Zai");
        digitalWrite(LED_BUILT_IN, HIGH); // Turn on

        playAudio_Zai();

        record_status = false;
    }
}

// 初始化硬件
void initHardware()
{
    // 设置超声波传感器引脚
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT);

    // 设置光敏传感器引脚 (新增)
    pinMode(LIGHT_SENSOR_DO_PIN, INPUT); // 数字输出作为输入

    // 设置灯控制引脚 (新增)
    pinMode(LIGHT_CONTROL_PIN, OUTPUT);
    digitalWrite(LIGHT_CONTROL_PIN, LOW); // 初始状态关闭灯

    // 设置蜂鸣器引脚
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH); // 假设高电平关闭蜂鸣器

    // // 设置录音按钮引脚
    pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);

    // 震动模块引脚初始化
    pinMode(VIBRATION_MODULE_PIN, OUTPUT);
    digitalWrite(VIBRATION_MODULE_PIN, LOW);

    Serial.println("硬件初始化完成");
}

// 设置并启动FreeRTOS任务
void setupTasks()
{
    // 创建超声波检测任务
    xTaskCreatePinnedToCore(
        ultrasonicTask,
        "UltrasonicTask",
        ULTRASONIC_TASK_STACK_SIZE,
        NULL,
        ULTRASONIC_TASK_PRIORITY,
        &ultrasonicTaskHandle,
        0 // 在核心0上运行
    );

    // 创建检测按钮任务
    xTaskCreatePinnedToCore(
        buttonTask,
        "ButtonTask",
        BUTTON_TASK_STACK_SIZE,
        NULL,
        BUTTON_TASK_PRIORITY,
        &buttonTaskHandle,
        0 // 在核心0上运行
    );

    // 创建光敏传感器任务
    xTaskCreatePinnedToCore(
        lightSensorTask,
        "LightSensorTask",
        LIGHT_SENSOR_TASK_STACK_SIZE,
        NULL,
        LIGHT_SENSOR_TASK_PRIORITY,
        &lightSensorTaskHandle,
        0 // 在核心0上运行
    );

    // 创建GPS任务
    xTaskCreatePinnedToCore(
        gpsTask,
        "GPSTask",
        GPS_TASK_STACK_SIZE,
        NULL,
        GPS_TASK_PRIORITY,
        &gpsTaskHandle,
        0 // 在核心0上运行
    );

    xTaskCreatePinnedToCore(
        voiceTask,
        "VoiceTask",
        VOICE_TASK_STACK_SIZE,
        NULL,
        VOICE_TASK_PRIORITY,
        &voiceTaskHandle,
        1 // 在核心1上运行
    );

    Serial.println("所有任务已创建并启动");
}

// 超声波检测任务
void ultrasonicTask(void *pvParameters)
{
    Serial.println("超声波检测任务启动");

    // 产生一个10微秒的触发脉冲
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2); // 确保Trig引脚先拉低
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);                  // 发送10微秒高电平脉冲
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // 脉冲发送完毕，拉低Trig引脚

    // 等待Echo引脚的脉冲，测量高电平持续时间（即声波往返时间）
    // pulseIn()函数会等待引脚变为HIGH，开始计时，直到引脚变为LOW，停止计时并返回微秒数
    duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);

    // 根据声速计算距离
    // 声速大约是 343 m/s，或 0.0343 cm/microsecond
    // 距离 = (声速 * 时间) / 2  (除以2是因为测量的往返时间)
    // 距离(cm) = (0.0343 cm/us * duration us) / 2
    // 近似计算：距离(cm) = duration / 29.1 (因为 1 / 0.0343 / 2 ≈ 1 / 0.0686 ≈ 14.57，而常见模块参数是除以58或29.1)
    // 使用 29.1 或 58 可以根据实际测试进行微调，这里使用 29.1
    distanceCm = duration / 29.1 / 2; // duration / 58 也可以，取决于模块和精度要求

    // 转换为英寸 (1英寸 = 2.54厘米)
    distanceInch = distanceCm / 2.54;

    // 输出距离到串口监视器
    Serial.print("距离: ");
    Serial.print(distanceCm);
    Serial.print(" cm");
    Serial.print("  |  ");
    Serial.print(distanceInch);
    Serial.println(" inch");

    if (distanceCm <= 30)
    {
        // 振动
        digitalWrite(VIBRATION_MODULE_PIN, HIGH);
        digitalWrite(BUZZER_PIN, LOW);
        delay(150);
        digitalWrite(VIBRATION_MODULE_PIN, LOW);
        digitalWrite(BUZZER_PIN, HIGH);
        delay(850);
    }

    // 稍作延迟，避免连续测量过快
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void buttonTask(void *parameter)
{
    while (true)
    {
        if (digitalRead(TEST_BUTTON_PIN) == LOW)
        {

            digitalWrite(LED_BUILT_IN, HIGH); // Turn on
            // 按钮被按下
            vTaskDelay(200 / portTICK_PERIOD_MS); // 延时去抖动
            Serial.println("按钮被按下");
            vTaskDelay(200 / portTICK_PERIOD_MS); // 延时去抖动
            playAudio_Zai();
            vTaskDelay(300 / portTICK_PERIOD_MS);
            record_status_me = false;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void voiceTask(void *parameter)
{
    while (1)
    {
        if (!record_status || !record_status_me)
        {
            // Record audio from INMP441
            // 分配内存
            uint8_t *pcm_data = (uint8_t *)ps_malloc(BUFFER_SIZE);
            if (!pcm_data)
            {
                Serial.println("Failed to allocate memory for pcm_data");
                return;
            }

            Serial.println("i2s_read: starting");
            // 开始循环录音，将录制结果保存在pcm_data中
            // bytes_read: 记录每次从 I2S 读取到的字节数。
            // recordingSize: 记录已经录制到的总字节数。
            // ttsSize: 它可能在代码的其他地方用于记录文本转语音后音频数据的大小。
            size_t bytes_read = 0, recordingSize = 0, ttsSize = 0;

            // 声明一个大小为 512 的 int16_t 数组，用于临时存储每次从 I2S 读取到的音频数据。
            int16_t data[512];

            // 声明了一些用于检测语音结束的变量：
            // noVoicePre: 记录上一次检测到声音的时间。
            // noVoiceCur: 记录当前检测到声音的时间。
            // noVoiceTotal: 记录连续没有检测到声音的总时长。
            // VoiceCnt: 记录检测到声音的次数。
            size_t noVoicePre = 0, noVoiceCur = 0, noVoiceTotal = 0, VoiceCnt = 0;

            // 设置一个标志，表示当前正在录音。
            bool recording = true;

            while (1)
            {
                // 记录刚开始的时间
                // 记录当前时间。millis() 函数返回 ESP32-S3 启动后经过的毫秒数。
                noVoicePre = millis();

                // i2s录音结果保存在data中
                // I2S_IN_PORT: 指定了使用的 I2S 输入端口。
                // data: 是存储读取到的音频数据的缓冲区。PCM数据格式为 int16_t，每个采样点占用 2 字节。
                // sizeof(data): 是缓冲区的大小。
                // &bytes_read: 是一个指针，用于接收实际读取到的字节数。
                // portMAX_DELAY: 表示如果没有数据可读，则无限期等待（直到有数据）。
                // esp_err_t result: 函数返回一个错误码，用于判断读取是否成功。
                esp_err_t result = i2s_read(I2S_IN_PORT, data, sizeof(data), &bytes_read, portMAX_DELAY);
                // 将刚刚读取到的音频数据 (data) 复制到之前分配的 pcm_data 缓冲区的末尾。recordingSize 用于指示当前已经写入了多少数据。
                memcpy(pcm_data + recordingSize, data, bytes_read);
                // 更新已录制的总字节数
                recordingSize += bytes_read;
                Serial.printf("%x recordingSize: %d bytes_read :%d\n", pcm_data + recordingSize, recordingSize, bytes_read);

                // 计算平均值
                // 这部分代码计算了当前读取到的音频数据的平均绝对值 (sum_data)。这是一种简单的判断当前是否有声音的方法。如果平均值很小，则认为是静音。

                uint32_t sum_data = 0;
                for (int i = 0; i < bytes_read / 2; i++)
                {
                    sum_data += abs(data[i]);
                }
                sum_data = sum_data / bytes_read;
                Serial.printf("sum_data :%d\n", sum_data);

                // 判断当没有说话时间超过一定时间时就退出录音
                // 记录当前时间。
                noVoiceCur = millis();
                if (sum_data < 25)
                {
                    noVoiceTotal += noVoiceCur - noVoicePre;
                }
                else
                {
                    noVoiceTotal = 0;
                    VoiceCnt += 1;
                }
                Serial.printf("noVoiceCur :%d noVoicePre :%d noVoiceTotal :%d\n", noVoiceCur, noVoicePre, noVoiceTotal);

                if (noVoiceTotal > 1000)
                {
                    recording = false;
                }

                if (!recording || (recordingSize >= BUFFER_SIZE - bytes_read))
                {
                    Serial.printf("record done: %d", recordingSize);

                    break;
                }
            }

            // 设置唤醒录音状态为true，此后可以唤醒
            record_status = true;
            record_status_me = true;

            // 如果此时一直没有说话，则退出被唤醒状态
            if (VoiceCnt <= 1)
            {
                digitalWrite(LED_BUILTIN, LOW); // Turn off
                recordingSize = 0;
                // 释放内存
                free(pcm_data);
                digitalWrite(LED_BUILT_IN, LOW); // Turn off
                // 跳过本次循环的剩余部分，回到外层的 while (1) 循环的开始，等待下一次录音触发
                continue;
            }

            if (recordingSize > 0)
            {
                // 音频转文本（语音识别API访问）
                Serial.println("发送语音识别请求");
                String recognizedText = baidu_voice_recognition(accessToken, pcm_data, recordingSize);
                Serial.println("Recognized text: " + recognizedText);

                // 访问后端大模型
                Serial.println("发送后端大模型请求");
                String response = sendTextToServer(recognizedText);
                Serial.println("Response from server: " + response);

                // 文本转音频tts并通过MAX98357A输出
                baiduTTS_Send(accessToken, response);
                Serial.println("ttsSize: ");
                Serial.println(ttsSize);
            }

            // 释放内存
            free(pcm_data);

            // 设置唤醒录音状态为false，此后继续录音对话
            // 这种情况是为了如果想继续对话的话就不用再唤醒了
            record_status = false;
            record_status_me = false;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// 唤醒词任务
static void audio_inference_callback(uint32_t n_bytes)
{
    for (int i = 0; i < n_bytes >> 1; i++)
    {
        inference.buffer[inference.buf_count++] = sampleBuffer[i];

        if (inference.buf_count >= inference.n_samples)
        {
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void capture_samples(void *arg)
{

    const int32_t i2s_bytes_to_read = (uint32_t)arg;
    size_t bytes_read = i2s_bytes_to_read;

    while (record_status)
    {

        /* read data at once from i2s - Modified for XIAO ESP2S3 Sense and I2S.h library */
        i2s_read(I2S_IN_PORT, (void *)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);
        // esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, (void *)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);

        if (bytes_read <= 0)
        {
            ei_printf("Error in I2S read : %d", bytes_read);
        }
        else
        {
            if (bytes_read < i2s_bytes_to_read)
            {
                ei_printf("Partial I2S read");
            }

            // scale the data (otherwise the sound is too quiet)
            for (int x = 0; x < i2s_bytes_to_read / 2; x++)
            {
                sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
            }

            if (record_status)
            {
                audio_inference_callback(i2s_bytes_to_read);
            }
            else
            {
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

    if (inference.buffer == NULL)
    {
        return false;
    }

    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    //    if (i2s_init(EI_CLASSIFIER_FREQUENCY)) {
    //        ei_printf("Failed to start I2S!");
    //    }

    ei_sleep(100);

    record_status = true;

    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void *)sample_buffer_size, 10, NULL);
    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    while (inference.buf_ready == 0)
    {
        delay(10);
    }

    inference.buf_ready = 0;
    return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    free(sampleBuffer);
    ei_free(inference.buffer);
}

// 光敏传感器任务
void lightSensorTask(void *pvParameters)
{
    Serial.println("光敏传感器任务启动");
    while (1)
    {
        // 读取光敏传感器的数字输出值
        // 传感器在黑暗时输出 HIGH，在明亮时输出 LOW
        int lightState = digitalRead(LIGHT_SENSOR_DO_PIN);

        if (lightState == HIGH)
        {
            // 检测到黑暗环境，打开灯
            digitalWrite(LIGHT_CONTROL_PIN, HIGH);
            Serial.println("检测到黑暗，开灯");
        }
        else
        {
            // 检测到明亮环境，关闭灯
            digitalWrite(LIGHT_CONTROL_PIN, LOW);
            Serial.println("检测到明亮，关灯");
        }

        // 每隔一段时间检测一次
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void gpsTask(void *pvParameters)
{
    Serial.println("GPS任务启动");
    while (1)
    {
        parseGpsBuffer(); // 解析GPS串口数据，假设此函数会更新 gpsLatitude 和 gpsLongitude

        // 检查是否有有效的GPS数据并且WiFi已连接
        // (添加一个简单的检查，例如经纬度不为0，你可以根据实际情况调整)
        if (isConnectedToWifi && originLat != 0.0 && originLng != 0.0)
        {
            Serial.printf("获取到GPS坐标: Lat=%.6f, Lon=%.6f\n", originLat, originLng);
            sendGpsData(originLat, originLng); // 调用发送函数
        }
        else if (!isConnectedToWifi)
        {
            Serial.println("GPS任务：WiFi未连接");
        }
        else
        {
            Serial.println("GPS任务：等待有效GPS数据...");
        }

        // 任务延时，例如每5秒发送一次
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}