# 硬件端固件

`硬件端/` 是智能导盲杖主控固件工程，运行在 `ESP32-S3` 上，使用 PlatformIO + Arduino 框架。

## 主要能力

- 语音唤醒与单轮语音交互
- 百度语音识别与语音播报
- 百度 `access_token` NVS 持久化缓存
- 手机热点友好的 DHCP WiFi 连接与失败诊断
- GPS 采集与位置上报
- 超声波避障提醒
- 光敏检测与自动照明
- 与服务端联动的天气查询、导航与普通问答
- `DEVICE_ID` 设备标识与 `X-Device-ID` 请求头
- 服务端新 `speak` 响应格式解析，本地音频缺失时回退百度 TTS

## 本地配置

先复制配置模板：

```bash
copy src\config.local.example.h src\config.local.h
```

然后填写：

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `SERVER_BASE_URL`
- `DEVICE_ID`
- `BAIDU_CLIENT_ID`
- `BAIDU_CLIENT_SECRET`

可选填写：

- `BAIDU_ACCESS_TOKEN_SEED`
- `BAIDU_ACCESS_TOKEN_EXPIRES_AT_SEED`

如果已经从百度接口拿到仍在有效期内的 `access_token`，可以把 token 和过期时间写入上述 seed 配置。首次启动时固件会把它写入 ESP32 NVS，之后重启会优先从 NVS 读取，不再每次开机都请求百度 token。

## 网络与百度 Token

WiFi 初始化位于 `src/network.cpp`。当前逻辑使用 STA + DHCP，不再在连接前写入 `0.0.0.0` 静态 IP 配置，更适合手机 2.4G 热点。连接失败时会打印 WiFi 状态码，并扫描目标 SSID、信道、RSSI 和加密类型，便于排查热点兼容问题。

百度 token 获取位于 `src/voice.cpp`。启动时顺序如下：

1. 从 ESP32 NVS 读取缓存 token
2. 如果 NVS 没有可用 token，则读取 `config.local.h` 中的 seed token 并写入 NVS
3. 如果两者都不可用，则阻塞式请求百度 `oauth/2.0/token`
4. 线上请求成功后自动刷新 NVS 缓存

如果确实拿不到 token，程序会停留在 token 获取阶段，不会继续进入后续语音识别、大模型问答或 TTS 链路。

## 语音合成响应处理

百度 TTS 位于 `src/voice.cpp`。当前实现使用 `application/x-www-form-urlencoded` 请求 `https://tsn.baidu.com/text2audio`，并兼容两类返回：

- 原始 WAV/音频流：解析 WAV 头后播放 PCM 数据
- JSON 响应：读取 `binary` 字段，base64 解码后播放

即使百度返回 `HTTP 200` 但 `Content-Type` 为空，固件也会下载响应 body 并按内容自动判断类型。

## 服务端响应协议

固件会优先解析服务端新字段：

- `speak.mode=local_audio`：尝试播放本地 `audio_id`，失败后回退 TTS。
- `speak.mode=tts_text`：播报 `speak.text`。
- `speak.mode=audio_url`：第一阶段回退文本播报。
- `speak.mode=none`：不播报。

旧字段 `response` 仍然兼容。导航状态会从 `navigation.active`、`navigation.next_instruction` 和旧字段 `navigation_complete` 中同步。

## 构建与烧录

```bash
pio run
pio run --target upload
python -m platformio run -t upload --upload-port COM10
```

如果 Windows 环境在中文路径下链接时报 `cannot open map file ... firmware.map`，可以临时用英文路径 junction 构建：

```powershell
New-Item -ItemType Junction -Path D:\igc_hw_src -Target "D:\Intelligent-Guide-Cane\硬件端"
python -m platformio run -d D:\igc_hw_src
```

如果需要串口监视：

```bash
pio device monitor -b 115200
```

## 关键文件

- `src/main.cpp`：任务调度、语音流程、导航更新
- `src/network.cpp`：WiFi 初始化、服务端接口通信
- `src/services/server_api.cpp`：服务端 JSON 请求、`X-Device-ID`、超时配置
- `src/utils/json_helper.cpp`：服务端响应解析
- `src/audio/local_audio.cpp`：本地缓存音频协议占位
- `src/app_state.cpp`：应用状态机
- `src/voice.cpp`：录音、ASR、TTS、百度 token 缓存
- `src/gps.cpp`：GPS 解析与上传
- `src/config.h`：公共默认配置
- `src/config.local.h`：本地私有配置，不提交

## 接线说明

更简化的接线说明见 [接线说明.md](/D:/Intelligent-Guide-Cane/硬件端/test/接线说明.md)。

完整项目启动顺序以仓库根目录的 [README.md](/D:/Intelligent-Guide-Cane/README.md) 为准。
