#include "esp_camera.h"
#include "esp_system.h"
#include <WiFi.h>

// AI Thinker ESP32-CAM. Change this only if your board is a different model.
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#if defined(__has_include)
#if __has_include("esp_cam_local_config.h")
#include "esp_cam_local_config.h"
#endif
#endif

#ifndef ESP_CAM_WIFI_SSID
#define ESP_CAM_WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef ESP_CAM_WIFI_PASSWORD
#define ESP_CAM_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef ESP_CAM_HOSTNAME
#define ESP_CAM_HOSTNAME "esp32-cam"
#endif

#ifndef ESP_CAM_DIRECT_AP_SSID
#define ESP_CAM_DIRECT_AP_SSID "ESP32-CAM-Stream"
#endif

#ifndef ESP_CAM_DIRECT_AP_PASSWORD
#define ESP_CAM_DIRECT_AP_PASSWORD "YOUR_DIRECT_AP_PASSWORD"
#endif

const char *WIFI_SSID = ESP_CAM_WIFI_SSID;
const char *WIFI_PASSWORD = ESP_CAM_WIFI_PASSWORD;
const char *HOSTNAME = ESP_CAM_HOSTNAME;

const int FLASH_LED_PIN = 4;
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 45000;
const uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;

const int CAMERA_BRIGHTNESS = 0;
const int CAMERA_CONTRAST = 1;
const int CAMERA_SATURATION = 2;
const int CAMERA_SHARPNESS = 1;
const int CAMERA_AE_LEVEL = -1;

// Direct AP is useful when a phone hotspot blocks access from the phone to
// connected clients. Connect your phone to this AP to test the camera directly.
const char *DIRECT_AP_SSID = ESP_CAM_DIRECT_AP_SSID;
const char *DIRECT_AP_PASSWORD = ESP_CAM_DIRECT_AP_PASSWORD;

bool hotspotConnected = false;
bool directApStarted = false;
bool bootReady = false;
int cameraStreamPort = 0;
uint32_t lastReconnectAttempt = 0;

int startCameraServer();

const char *wifiStatusText(wl_status_t status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  default:
    return "WL_UNKNOWN";
  }
}

const char *resetReasonText(esp_reset_reason_t reason)
{
  switch (reason)
  {
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_EXT:
    return "EXTERNAL_RESET";
  case ESP_RST_SW:
    return "SOFTWARE_RESET";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INTERRUPT_WATCHDOG";
  case ESP_RST_TASK_WDT:
    return "TASK_WATCHDOG";
  case ESP_RST_WDT:
    return "OTHER_WATCHDOG";
  case ESP_RST_DEEPSLEEP:
    return "DEEP_SLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "UNKNOWN";
  }
}

void printMemoryLog(const char *stage)
{
  Serial.printf("[MEM] %s free_heap=%u min_free_heap=%u psram_found=%s psram_size=%u free_psram=%u\n",
                stage,
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap(),
                psramFound() ? "yes" : "no",
                ESP.getPsramSize(),
                ESP.getFreePsram());
}

void printUrls(const char *label, const IPAddress &ip, int streamPort)
{
  Serial.println();
  Serial.printf("========== %s URLs ==========\n", label);
  Serial.printf("Health:        http://%s/health\n", ip.toString().c_str());
  Serial.printf("Stream health: http://%s:%d/health\n", ip.toString().c_str(), streamPort);
  Serial.printf("Capture:       http://%s/capture\n", ip.toString().c_str());
  Serial.printf("Stream:        http://%s:%d/stream\n", ip.toString().c_str(), streamPort);
  Serial.printf("Stream alt:    http://%s/stream\n", ip.toString().c_str());
  Serial.printf("Page:          http://%s/\n", ip.toString().c_str());
  Serial.println("Test order: open Health first, then Capture, then Stream.");
  Serial.println("====================================");
  Serial.println();
}

void applyIndoorVividColorProfile(sensor_t *sensor)
{
  if (!sensor)
  {
    return;
  }

  sensor->set_framesize(sensor, FRAMESIZE_QVGA);
  sensor->set_quality(sensor, 12);
  sensor->set_brightness(sensor, CAMERA_BRIGHTNESS);
  sensor->set_contrast(sensor, CAMERA_CONTRAST);
  sensor->set_saturation(sensor, CAMERA_SATURATION);
  sensor->set_sharpness(sensor, CAMERA_SHARPNESS);
  sensor->set_whitebal(sensor, 1);
  sensor->set_awb_gain(sensor, 1);
  sensor->set_wb_mode(sensor, 0);
  sensor->set_exposure_ctrl(sensor, 1);
  sensor->set_aec2(sensor, 1);
  sensor->set_ae_level(sensor, CAMERA_AE_LEVEL);
  sensor->set_gain_ctrl(sensor, 1);
  sensor->set_gainceiling(sensor, GAINCEILING_16X);
  sensor->set_bpc(sensor, 1);
  sensor->set_wpc(sensor, 1);
  sensor->set_raw_gma(sensor, 1);
  sensor->set_lenc(sensor, 1);
  sensor->set_dcw(sensor, 1);
  sensor->set_colorbar(sensor, 0);
}

bool initCamera()
{
  Serial.println("[CAM] Init begin");

  camera_config_t config = {};
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Start with a small frame so the first target is reliable LAN streaming.
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = psramFound() ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY;

  Serial.printf("[CAM] Config frame_size=QVGA jpeg_quality=%d fb_count=%u fb_location=%s grab_mode=%s xclk=%u\n",
                config.jpeg_quality,
                config.fb_count,
                psramFound() ? "PSRAM" : "DRAM",
                psramFound() ? "LATEST" : "WHEN_EMPTY",
                config.xclk_freq_hz);

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("[CAM] Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor)
  {
    applyIndoorVividColorProfile(sensor);
    Serial.printf("[CAM] Sensor PID=0x%02x framesize=%u quality=%u brightness=%d contrast=%d saturation=%d sharpness=%d ae_level=%d wb_mode=%u\n",
                  sensor->id.PID,
                  sensor->status.framesize,
                  sensor->status.quality,
                  sensor->status.brightness,
                  sensor->status.contrast,
                  sensor->status.saturation,
                  sensor->status.sharpness,
                  sensor->status.ae_level,
                  sensor->status.wb_mode);
  }
  else
  {
    Serial.println("[CAM] Warning: esp_camera_sensor_get() returned null");
  }

  Serial.println("[CAM] Camera initialized");
  return true;
}

bool connectToHotspot()
{
  Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);

  WiFi.disconnect(false, true);
  delay(200);
  WiFi.mode(directApStarted ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  wl_status_t lastStatus = WiFi.status();
  Serial.printf("[WiFi] Initial status=%d (%s)\n", lastStatus, wifiStatusText(lastStatus));
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(500);
    wl_status_t status = WiFi.status();
    if (status != lastStatus)
    {
      lastStatus = status;
      Serial.printf("\n[WiFi] Status changed: %d (%s), elapsed=%lu ms\n",
                    status,
                    wifiStatusText(status),
                    (unsigned long)(millis() - start));
    }
    else
    {
      Serial.print(".");
    }
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED)
  {
    wl_status_t status = WiFi.status();
    Serial.printf("[WiFi] Connect failed after %lu ms, status=%d (%s)\n",
                  (unsigned long)(millis() - start),
                  status,
                  wifiStatusText(status));
    return false;
  }

  Serial.printf("[WiFi] Connected in %lu ms\n", (unsigned long)(millis() - start));
  Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("[WiFi] Subnet: %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
  Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
  return true;
}

bool startDirectAccessPoint()
{
  Serial.println("[WiFi] Starting direct access point");
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  bool ok = WiFi.softAP(DIRECT_AP_SSID, DIRECT_AP_PASSWORD);
  if (!ok)
  {
    Serial.println("[WiFi] Failed to start direct AP");
    return false;
  }

  directApStarted = true;
  Serial.printf("[WiFi] Direct AP SSID: %s\n", DIRECT_AP_SSID);
  Serial.printf("[WiFi] Direct AP password: %s\n", DIRECT_AP_PASSWORD);
  Serial.printf("[WiFi] Direct AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[WiFi] Direct AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
  return true;
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);
  Serial.println();
  Serial.println("===== ESP32-CAM MJPEG Stream =====");
  Serial.printf("[BOOT] Reset reason: %s (%d)\n", resetReasonText(esp_reset_reason()), esp_reset_reason());
  Serial.printf("[BOOT] CPU freq: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("[BOOT] Flash size: %u bytes\n", ESP.getFlashChipSize());
  printMemoryLog("before camera");

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  WiFi.persistent(false);

  if (!initCamera())
  {
    Serial.println("[BOOT] Stop: camera is not ready");
    return;
  }
  printMemoryLog("after camera");

  bool apReady = startDirectAccessPoint();
  hotspotConnected = connectToHotspot();
  if (!hotspotConnected)
  {
    Serial.println("[WiFi] Hotspot connection failed");
    Serial.println("[WiFi] You can still test direct mode by connecting your phone to ESP32-CAM-Stream");
    if (!apReady)
    {
      Serial.println("[BOOT] Stop: no usable WiFi mode");
      return;
    }
  }

  int streamPort = startCameraServer();
  if (streamPort <= 0)
  {
    Serial.println("[HTTP] Stop: camera server failed to start");
    return;
  }
  cameraStreamPort = streamPort;
  Serial.printf("[HTTP] Server started: web_port=80 stream_port=%d\n", streamPort);

  if (hotspotConnected)
  {
    printUrls("Hotspot", WiFi.localIP(), streamPort);
  }
  if (directApStarted)
  {
    printUrls("Direct AP", WiFi.softAPIP(), streamPort);
  }
  Serial.println("[TEST] If Hotspot URLs timeout but Direct AP URLs work, the phone hotspot is isolating clients.");
  Serial.println("[TEST] If Health works but Capture fails, check camera power/cable/PSRAM.");
  Serial.println("[TEST] If Capture works but Stream fails, test the Stream alt URL on port 80.");

  bootReady = true;
  printMemoryLog("ready");
  Serial.println("[BOOT] Ready");
}

void loop()
{
  if (!bootReady)
  {
    delay(1000);
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!hotspotConnected)
    {
      hotspotConnected = true;
      printUrls("Hotspot reconnected", WiFi.localIP(), cameraStreamPort);
    }
  }
  else
  {
    wl_status_t status = WiFi.status();
    if (hotspotConnected)
    {
      Serial.printf("[WiFi] Lost hotspot connection, status=%d (%s)\n", status, wifiStatusText(status));
    }
    hotspotConnected = false;
    uint32_t now = millis();
    if (now - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL_MS)
    {
      lastReconnectAttempt = now;
      Serial.println("[WiFi] Disconnected, reconnecting...");
      WiFi.disconnect(false);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  delay(1000);
}
