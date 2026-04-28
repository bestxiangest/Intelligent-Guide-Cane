# 星辰引路者 / Intelligent Guide Cane

基于 ESP32-S3 的智能导盲杖系统，包含主控固件、Flask 语音中枢服务、微信小程序和 ESP32-CAM 视频辅助端。当前版本已经从串行语音问答重构为“低延迟语音中枢”：简单指令走 fast intent，本地安全能力继续由 ESP32 实时处理，大模型只负责语义理解和普通对话。

## 项目结构

```text
Intelligent-Guide-Cane/
├─ 硬件端/                  # ESP32-S3 主控固件，PlatformIO 工程
│  ├─ src/
│  ├─ data/audio/           # LittleFS 本地 16kHz WAV 缓存音频
│  └─ tools/                # 音频转换等工具脚本
├─ web/                     # Flask 服务端
│  ├─ routes/               # HTTP 路由
│  ├─ services/             # 意图、调度、导航、天气、位置等业务服务
│  ├─ clients/              # 千问、高德、和风天气等外部 API 客户端
│  ├─ models/               # 意图、响应、设备状态结构
│  ├─ tests/                # pytest 测试
│  └─ API.md                # 接口文档
├─ 微信小程序/               # 地图、位置、ESP32-CAM 预览小程序
├─ esp-cam/esp_cam/         # ESP32-CAM Arduino 工程
├─ test/                    # 独立硬件/音频测试工程
├─ README.md
└─ 当前已实现功能介绍.md
```

## 核心架构

```text
ESP32-S3 录音
  → 百度 ASR
  → POST /ai
  → fast intent
  → 必要时调用千问输出严格 JSON 意图
  → dispatcher 白名单函数分发
  → 导航 / 天气 / 位置 / 状态业务
  → 统一 JSON 响应
  → ESP32 播放本地音频或百度 TTS
```

服务端只执行显式白名单 intent，不让大模型直接控制导航、天气、设备状态或任何真实世界动作。超声波避障、蜂鸣器、震动提醒、光敏照明等安全能力始终保留在 ESP32 本地。

## 已支持能力

- 唤醒词触发和物理按钮触发
- ESP32-S3 16kHz 录音
- 百度 ASR 语音识别
- fast intent 快速通道：问候、导航、天气、退出导航、帮助等简单指令不调用 LLM
- DashScope / 千问 JSON 意图识别
- dispatcher 白名单函数分发
- 高德步行导航、目的地地理编码、导航状态更新和退出导航
- 和风天气查询，支持当前城市、今天、明天、后天
- GPS 坐标上传、城市反解和位置缓存
- 超声波本地避障
- 光敏本地照明控制
- 百度 TTS 文本播报
- LittleFS 本地 WAV 缓存音频，常用回复可离线快速播放
- 微信小程序地图和视频预览
- ESP32-CAM 图像/视频流辅助端

## 硬件接线

以下引脚来自 `硬件端/src/config.h`。如需改线，请同步修改固件配置。

### ESP32-S3 主控

| 模块 | 模块引脚 | ESP32-S3 引脚 | 说明 |
| --- | --- | --- | --- |
| INMP441 麦克风 | SCK | GPIO4 | I2S BCLK |
| INMP441 麦克风 | WS | GPIO5 | I2S LRCLK |
| INMP441 麦克风 | SD | GPIO6 | I2S DATA IN |
| INMP441 麦克风 | VCC / GND | 3V3 / GND | 建议使用 3.3V |
| MAX98357A 功放 | BCLK | GPIO15 | I2S BCLK |
| MAX98357A 功放 | LRC | GPIO16 | I2S LRCLK |
| MAX98357A 功放 | DIN | GPIO7 | I2S DATA OUT |
| MAX98357A 功放 | VIN / GND | 5V 或 3V3 / GND | 视模块供电规格决定 |
| HC-SR04 超声波 | TRIG | GPIO8 | 触发脚 |
| HC-SR04 超声波 | ECHO | GPIO18 | 回响脚，5V Echo 建议分压或电平转换 |
| 震动模块 | SIG | GPIO3 | 障碍提醒 |
| 有源蜂鸣器 | SIG | GPIO11 | 当前代码默认高电平关闭 |
| 语音按钮 | 一端 | GPIO2 | `INPUT_PULLUP`，按下接 GND |
| 语音按钮 | 另一端 | GND | 低电平触发 |
| 光敏模块 | DO | GPIO10 | 数字光照检测 |
| 照明/LED 控制 | SIG | GPIO21 | 与 `LED_BUILT_IN` 共用配置 |
| GPS 模块 | TX | GPIO17 | ESP32 接收 GPS NMEA |
| GPS 模块 | RX | 未使用 | `GPS_TX_PIN = -1` |
| GPS 模块 | VCC / GND | 3V3 或 5V / GND | 按模块规格供电 |

### ESP32-CAM

ESP32-CAM 使用 AI Thinker 引脚定义，见 `esp-cam/esp_cam/camera_pins.h`。默认提供 HTTP 健康检查、单帧抓拍和视频流地址，启动后会在串口打印访问 URL。

## 配置文件

不要提交真实 Wi-Fi、API Key、Client Secret 或 Access Token。仓库只保留示例配置。

### 服务端

```powershell
cd web
copy local_config.example.py local_config.py
```

填写：

- `QWEATHER_API_KEY`：和风天气 Key
- `AL_API_KEY`：DashScope / 千问 Key
- `AMAP_API_KEY`：高德 Web 服务 Key
- `DEFAULT_LOCATION`：默认经纬度，格式为 `"longitude,latitude"`
- `DEFAULT_WEATHER_CITY`：无 GPS 城市时的默认天气城市

### ESP32-S3 主控

```powershell
cd 硬件端
copy src\config.local.example.h src\config.local.h
```

填写：

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `SERVER_BASE_URL`，例如 `http://192.168.1.100:12345`
- `DEVICE_ID`，例如 `guide-cane-001`
- `BAIDU_CLIENT_ID`
- `BAIDU_CLIENT_SECRET`
- 可选 `BAIDU_ACCESS_TOKEN_SEED` 和 `BAIDU_ACCESS_TOKEN_EXPIRES_AT_SEED`

### 微信小程序

```powershell
cd 微信小程序
copy config.local.example.js config.local.js
```

填写：

- `amapKey`：小程序高德 Key
- `defaultCameraIp`：ESP32-CAM 在热点/局域网中的地址
- `directCameraIp`：ESP32-CAM AP 模式地址，默认常用 `192.168.4.1`

### ESP32-CAM

```powershell
cd esp-cam\esp_cam
copy esp_cam_local_config.example.h esp_cam_local_config.h
```

填写：

- `ESP_CAM_WIFI_SSID`
- `ESP_CAM_WIFI_PASSWORD`
- `ESP_CAM_DIRECT_AP_SSID`
- `ESP_CAM_DIRECT_AP_PASSWORD`

## 部署服务端

建议使用 Python 3.10+。

```powershell
cd web
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
copy local_config.example.py local_config.py
```

编辑 `local_config.py` 后启动：

```powershell
flask --app app run --debug --host 0.0.0.0 --port 12345
```

健康检查：

```powershell
curl http://127.0.0.1:12345/health
```

运行测试：

```powershell
python -m pytest
```

## 编译和烧录 ESP32-S3

项目使用 PlatformIO。当前 `platformio.ini` 已把构建目录放到 `../.pio-build-hardware`，避免 Windows 下中文路径导致链接器无法生成 `firmware.map`。

```powershell
cd 硬件端
python -m platformio run
python -m platformio run -t upload --upload-port COM3
```

本地缓存音频在 `硬件端/data/audio/`，使用 LittleFS 烧录到 ESP32-S3 内部 Flash：

```powershell
python -m platformio run -t buildfs
python -m platformio run -t uploadfs --upload-port COM3
```

串口监视：

```powershell
python -m platformio device monitor --port COM3 --baud 115200
```

## 本地音频缓存

当前本地音频均为 `16kHz / mono / 16-bit PCM WAV`，烧录到 LittleFS 后由 ESP32 优先播放。常用文件：

| audio_id | 文件名 | 用途 |
| --- | --- | --- |
| `record_start_001` | `record_start_001.wav` | 按钮或唤醒词触发后的“我在”提示 |
| `hello_001` | `hello_001.wav` | 问候回复 |
| `wait_001` | `wait_001.wav` | 请稍等 |
| `not_understood_001` | `not_understood_001.wav` | 未听清 |
| `nav_exit_001` | `nav_exit_001.wav` | 已退出导航 |
| `no_navigation_001` | `no_navigation_001.wav` | 当前没有导航 |
| `network_error_001` | `network_error_001.wav` | 网络异常 |
| `gps_invalid_001` | `gps_invalid_001.wav` | GPS 暂不可用 |

如需批量转换 WAV：

```powershell
cd 硬件端
python tools\convert_wav_to_16k.py
```

## 微信小程序运行

1. 打开微信开发者工具。
2. 导入 `微信小程序/`。
3. 复制并填写 `config.local.js`。
4. 如需真机访问服务端或 ESP32-CAM，请在小程序后台配置合法域名，或在开发阶段关闭 URL 校验。
5. 编译预览。

## ESP32-CAM 运行

1. 使用 Arduino IDE 打开 `esp-cam/esp_cam/esp_cam.ino`。
2. 选择 AI Thinker ESP32-CAM。
3. 复制并填写 `esp_cam_local_config.h`。
4. 烧录后打开 115200 串口，查看打印出的 `Health`、`Capture`、`Stream` URL。

## HTTP 接口

保留接口：

- `POST /ai`
- `POST /gps`
- `GET /weather`
- `POST /navigation_update`
- `POST /exit_navigation`

新增接口：

- `GET /health`
- `POST /v1/intent`
- `POST /v1/device/status`
- `POST /v1/audio`

统一响应保留旧字段 `response`，并新增结构化字段：

```json
{
  "ok": true,
  "request_id": "uuid",
  "device_id": "guide-cane-001",
  "intent": "chat.normal",
  "response": "我在呢，请说导航、天气或者帮助。",
  "speak": {
    "mode": "local_audio",
    "text": "我在呢，请说导航、天气或者帮助。",
    "audio_id": "hello_001",
    "audio_url": ""
  },
  "navigation": {
    "active": false,
    "destination": "",
    "remaining_distance": null,
    "total_duration": null,
    "next_instruction": ""
  },
  "error": null
}
```

完整接口说明见 `web/API.md`。

## 安全边界

- 超声波避障、蜂鸣器、震动提醒、光敏照明都在 ESP32 本地运行，不依赖云端。
- 大模型只输出受校验的 JSON 意图，不直接执行外部 API 或设备动作。
- 服务端 dispatcher 只执行白名单 intent。
- 外部 API 调用设置超时，配置缺失时返回可理解的错误提示。
- 日志和仓库不应包含真实 API Key、Client Secret、Access Token 或 Wi-Fi 密码。

## 后续计划

- 服务端 ASR/TTS 接管预留
- WebSocket 音频流上传
- 流式 TTS 下发
- 导航路线缓存和更新节流
- 更多本地安全能力：跌倒检测、紧急呼叫、离线告警
