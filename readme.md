# 智能导盲杖系统

![智能导盲杖](https://img.shields.io/badge/智能导盲杖-ESP32--S3-blue)
![版本](https://img.shields.io/badge/版本-1.0.0-green)
![许可证](https://img.shields.io/badge/许可证-MIT-orange)

## 项目概述

智能导盲杖系统是一款基于ESP32-S3微控制器的辅助设备，旨在帮助视障人士更安全、便捷地出行。该系统集成了多种传感器和人工智能技术，能够实时检测周围环境，识别障碍物、盲道和红绿灯，并通过语音交互提供导航和天气查询等功能。

## 功能特点

- **障碍物检测**：使用超声波传感器实时检测前方障碍物，并根据距离提供不同级别的警报
- **盲道识别**：通过摄像头实时识别地面盲道，辅助视障人士沿着正确的路径行走
- **红绿灯识别**：自动识别交通信号灯状态，提醒用户何时安全通行
- **语音交互**：支持语音指令输入和语音反馈，实现无障碍人机交互
- **智能导航**：通过语音指令设置目的地，提供实时导航指引
- **天气查询**：支持通过语音查询指定城市的天气情况
- **大模型对话**：集成大语言模型，支持自然语言对话和信息查询

## 硬件要求

- ESP32-S3开发板（推荐ESP32-S3-DevKitC-1）
- 超声波传感器（HC-SR04）
- 摄像头模块（ESP32-CAM或OV2640）
- 麦克风模块（支持I2S接口）
- 振动马达
- 蜂鸣器
- 按钮（用于触发语音交互）
- 电源模块（锂电池+充电管理）

## 接线图

| 组件             | 引脚                   |
| ---------------- | ---------------------- |
| 超声波传感器Trig | GPIO 5                 |
| 超声波传感器Echo | GPIO 6                 |
| 振动马达         | GPIO 7                 |
| 蜂鸣器           | GPIO 8                 |
| 麦克风           | GPIO 10                |
| 语音按钮         | GPIO 14                |
| 摄像头           | 详见代码中的摄像头配置 |

## 软件依赖

- [Arduino框架](https://www.arduino.cc/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [ArduinoJson](https://arduinojson.org/) (v7.3.1+)
- [Base64](https://github.com/Densaugeo/base64) (v1.4.0+)
- [PlatformIO](https://platformio.org/) 开发环境

## 安装与编译

1. 安装[PlatformIO IDE](https://platformio.org/platformio-ide)（基于VSCode）
2. 克隆本仓库：git clone https://github.com/yourusername/intelligent-guide-cane.git
3. 打开PlatformIO IDE，导入项目
4. 配置`config.h`文件中的API密钥和WiFi信息
5. 编译并上传到ESP32-S3开发板

## 配置说明

在使用前，请确保在`config.h`文件中正确配置以下信息：

- WiFi连接信息（SSID和密码）
- 百度语音API密钥（用于语音识别）
- 心知天气API密钥（用于天气查询）
- AnythingLLM API配置（用于大模型对话）

## 使用方法

1. 开机后，系统将自动连接WiFi并初始化各个模块
2. 超声波传感器会持续检测前方障碍物，当检测到障碍物时会通过振动和蜂鸣器提醒用户
3. 摄像头会实时检测盲道和红绿灯，并在必要时提供警报
4. 按下语音按钮可以进行语音交互：

- 导航指令：例如"导航到江西理工大学"
- 天气查询：例如"今天南昌天气怎么样"
- 一般问题：可以询问任何问题，系统会通过大模型提供回答

## 系统架构

系统基于FreeRTOS多任务架构，主要包含以下任务：

- 超声波检测任务：负责障碍物检测
- 视觉检测任务：负责盲道识别
- 红绿灯检测任务：负责交通信号灯识别
- 语音处理任务：负责语音交互
- 警报任务：负责整合各类警报并通过振动和声音提醒用户
- 导航任务：负责提供路径导航

## API集成

本项目集成了多个外部API服务：

- 百度语音识别API：用于将语音转换为文本
- 心知天气API：用于获取天气信息
- AnythingLLM API：用于大模型对话
- 高德地图API：用于地理编码和路径规划

## 贡献指南

欢迎对本项目进行贡献！如果您有任何改进建议或发现了bug，请提交issue或pull request。

## 许可证

本项目采用MIT许可证。详情请参阅[LICENSE](LICENSE)文件。

## 联系方式

如有任何问题或建议，请联系：

- 邮箱：1816054322@qq.com
- GitHub：[bestxiangest](https://github.com/bestxiangest)

## 致谢

感谢所有为本项目提供支持和帮助的个人和组织。

