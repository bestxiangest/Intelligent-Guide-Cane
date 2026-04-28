# 服务端接口文档

第一阶段仍由 ESP32 端完成百度 ASR/TTS。服务端负责文本意图识别、函数分发、导航/天气/GPS 状态管理。

## 统一响应

```json
{
  "ok": true,
  "request_id": "uuid",
  "device_id": "guide-cane-001",
  "intent": "navigation.start",
  "response": "路线规划完成，全程1.2公里，大约需要15分钟。请直行。",
  "speak": {
    "mode": "tts_text",
    "text": "路线规划完成，全程1.2公里，大约需要15分钟。请直行。",
    "audio_id": "",
    "audio_url": ""
  },
  "navigation": {
    "active": true,
    "destination": "图书馆",
    "remaining_distance": 1200,
    "total_duration": 900,
    "next_instruction": "请直行"
  },
  "error": null
}
```

`response` 保留给旧硬件端。`speak.mode` 支持 `tts_text`、`local_audio`、`audio_url`、`none`。

## 设备标识

服务端按以下顺序解析设备 ID：

1. HTTP Header `X-Device-ID`
2. JSON body 中的 `device_id`
3. `request.remote_addr`

## 保留接口

- `POST /ai`：请求 `{ "message": "导航到图书馆", "device_id": "guide-cane-001" }`
- `POST /gps`：请求 `{ "latitude": 28.68, "longitude": 115.89 }`
- `GET /weather?city=current&date=today`
- `POST /navigation_update`
- `POST /exit_navigation`

## 新增接口

- `GET /health`：服务健康检查和缺失配置提示。
- `POST /v1/intent`：只返回识别出的 JSON 意图，不执行业务。
- `POST /v1/device/status`：预留设备状态上报。
- `POST /v1/audio`：预留服务端 ASR/流式音频入口，第一阶段不接管语音链路。

## 本地缓存音频 ID

- `hello_001`：我在呢，请说导航、天气或者帮助。
- `wait_001`：请稍等。
- `not_understood_001`：我没有听明白，请再说一遍。
- `nav_exit_001`：已退出导航。
- `no_navigation_001`：当前没有进行中的导航。
- `network_error_001`：网络异常，请稍后重试。
- `gps_invalid_001`：当前位置暂时不可用，请到开阔地点稍后再试。
