#include "esp_camera.h"
#include <WiFi.h>

// Select camera model
// #define CAMERA_MODEL_WROVER_KIT
// #define CAMERA_MODEL_ESP_EYE
// #define CAMERA_MODEL_M5STACK_PSRAM
// #define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char *ssid = "abcde";
const char *password = "12345678";

const IPAddress ip(192, 168, 137, 1);
const IPAddress gateway(192, 168, 137, 1);
const IPAddress subnet(255, 255, 255, 0);

void cameraBegin();      // 相机初始化
int startCameraServer(); // 启动相机服务
void setWifi();
WiFiServer server(8000); // 申明AP对象

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("开机");
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH); // 闪光灯
  cameraBegin();
  setWifi();                      // 设置wifi
  int prot = startCameraServer(); // 开启相机服务并得到服务端口
  IPAddress myAddress = WiFi.localIP();
  
  Serial.print("请在浏览器中输入:");
  Serial.println(myAddress);


  delay(1000);
  digitalWrite(4, LOW); // 闪光灯
}

void loop()
{
  String msgs = "", onDeng = "";
  WiFiClient client = server.available();
  boolean receiveFlag = false; // 是否接到消息标志位
  if (client)
  {
    while (client.connected())
    {
      while (Serial.available() > 0)
      {
        char c = Serial.read();
        receiveFlag = true; // 接到来自小车的串口消息
        msgs += c;
      }

      while (client.available() > 0)
      {
        char c = client.read();
        Serial.write(c); // 向串口发送消息
        onDeng += c;
      }

      if (onDeng == "DENG_ON")
      {
        digitalWrite(4, HIGH); // 闪光灯
      }
      else if (onDeng == "DENG_OFF")
      {
        digitalWrite(4, LOW); // 闪光灯
      }
      if (receiveFlag)
      {
        receiveFlag = false;
        client.write(&msgs[0]); // 向客户端发送消息
        msgs = "";
      }
      onDeng = "";
    }
    client.stop();
  }
  else
    delay(500);
}

/**
 * @brief 配置wifi
 */
// void setWifi()
// {
//   WiFi.mode(WIFI_AP);                     // 设置wifi模
//   WiFi.softAPConfig(ip, gateway, subnet); // 配置网络信息
//   WiFi.softAP(ssid, password);            // 配置wifi名称及密码
//   Serial.println("开启网络....等待连接.....");
//   delay(500);
//   server.begin();
//   delay(500);
// }
void setWifi()
{
  WiFi.mode(WIFI_STA);                     // 设置wifi模
  WiFi.begin(ssid,password);
  IPAddress testIP = WiFi.localIP();
  Serial.println("....等待连接.....");
  delay(500);
  Serial.println(testIP);
  Serial.println("....连接成功.....");
  server.begin();
  delay(500);
}
/**
 * @brief 相机初始化
 *
 */
void cameraBegin()
{
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
  config.pixel_format = PIXFORMAT_JPEG;
  // 用高规格初始化来预分配更大的缓冲区
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // 相机初始化
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // 最初的传感器垂直翻转，颜色有点饱和
  if (s->id.PID == OV3660_PID)
  {
    s->set_vflip(s, 0);       // 反转
    s->set_brightness(s, 1);  // 将暗处调高
    s->set_saturation(s, -2); // 降低饱和度
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
}