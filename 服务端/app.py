from flask import Flask, request, jsonify, Response, send_file
import requests
import dashscope
from http import HTTPStatus
from dashscope.audio.asr import Transcription
from dashscope.audio.tts_v2 import *
import time
import threading
from datetime import datetime
import io
import os

app = Flask(__name__)
app.config['JSON_AS_ASCII'] = False  # 确保 jsonify 返回的 JSON 中文字符不被转义

# 和风天气API密钥 (请替换为您自己的API密钥)
QWEATHER_API_KEY = "3f71fd1e04a045f2a1996eb7c1f4302c"

# 阿里云API KEY
AL_API_KEY = 'sk-dcbb7246ac3f402994454b91120b95ab'
dashscope.api_key = AL_API_KEY

# 高德地图API配置
AMAP_API_KEY = "06ab3ec992282fb5e6d3c5bb9604af7f"
DEFAULT_LOCATION = "120.07275000000001,30.30828611111111"  # 默认位置

# 用于存储每个用户的对话历史和上次交互时间
conversations = {}
last_interaction = {}
conversation_lock = threading.Lock()

# 导航状态管理
navigation_sessions = {}  # 存储用户的导航会话
navigation_lock = threading.Lock()

# 定义对话超时时间（秒）
CONVERSATION_TIMEOUT = 60
NAVIGATION_TIMEOUT = 1800  # 导航超时时间30分钟

# 语音合成相关参数
model = "cosyvoice-v2"
voice = "longwan_v2"
audio_format = AudioFormat.PCM_16000HZ_MONO_16BIT
sample_rate = 16000
channels = 1
bytes_per_sample = 2  # 16-bit audio




# 根目录没有东西
@app.route("/")
def index():
    return "Hello World!"


def get_city_id(city_name: str):
    """
    根据城市名称从和风天气API获取城市ID。
    """
    base_url = "https://geoapi.qweather.com/v2/city/lookup"
    params = {
        "location": city_name,
        "key": QWEATHER_API_KEY
    }
    try:
        response = requests.get(base_url, params=params)
        response.raise_for_status()
        data = response.json()
        if data["code"] == "200" and data["location"]:
            return data["location"][0]["id"]
        else:
            return None
    except requests.exceptions.RequestException as e:
        print(f"查询城市ID失败: {e}")
        return None


def get_weather_forecast(city_id: str):
    """
    根据城市ID从天气预报API获取天气预报数据，并仅保留日期、温度、天气类型和提示语。
    """
    base_url = f"http://t.weather.itboy.net/api/weather/city/{city_id}"
    try:
        response = requests.get(base_url)
        response.raise_for_status()
        data = response.json()
        if data["status"] == 200:
            forecasts = data["data"]["forecast"]
            # 修改处：处理 forecast 数据，仅保留需要的字段
            processed_forecasts = []
            for forecast in forecasts[:3]:  # 仍然只取前三天的预报
                processed_forecast = {
                    "date": forecast["date"],
                    "high": forecast["high"],
                    "low": forecast["low"],
                    "type": forecast["type"],
                    "notice": forecast["notice"]
                }
                processed_forecasts.append(processed_forecast)
            return processed_forecasts
        else:
            return None
    except requests.exceptions.RequestException as e:
        print(f"获取天气预报失败: {e}")
        return None


@app.route("/weather", methods=['GET'])
def read_weather():
    """
    天气API主接口，根据城市名称返回今天、明天和后天的天气预报 (精简版)。
    """
    city = request.args.get('city')
    if not city:
        return jsonify({"error": "请提供城市名称作为查询参数。"}), 400  # 返回 400 状态码表示请求错误

    city_id = get_city_id(city)

    if not city_id:
        return jsonify({"error": "城市ID未找到，请检查城市名称是否正确。"}), 404  # 返回 404 状态码表示资源未找到

    forecast_data = get_weather_forecast(city_id)

    if not forecast_data:
        return jsonify({"error": "获取天气预报数据失败。"}), 500  # 返回 500 状态码表示服务器内部错误

    return jsonify({
        "city": city,
        "forecasts": forecast_data  # 修改处：返回处理后的 forecast_data
    })


def get_destination_gps(destination):
    """
    调用高德地图API获取目的地的GPS坐标。
    """
    base_url = "https://restapi.amap.com/v3/geocode/geo"
    params = {
        "address": destination,
        "output": "json",
        "key": AMAP_API_KEY
    }
    try:
        response = requests.get(base_url, params=params)
        response.raise_for_status()
        data = response.json()
        if data.get("status") == "1" and data.get("geocodes"):
            location = data["geocodes"][0]["location"]
            print("目的地GPS坐标为：" + location)
            return location
        else:
            return None
    except requests.exceptions.RequestException as e:
        print(f"获取目的地GPS坐标失败: {e}")
        return None


def get_walking_navigation(origin, destination_gps):
    """
    调用高德地图步行导航API规划路线（使用v5 API）。
    """
    base_url = "https://restapi.amap.com/v5/direction/walking"
    params = {
        "isindoor": "0",
        "origin": origin,
        "destination": destination_gps,
        "key": AMAP_API_KEY,
        "show_fields": "navi"
    }
    print("origin："+origin)
    try:
        response = requests.get(base_url, params=params)
        response.raise_for_status()
        data = response.json()
        if data.get("status") == "1" and data.get("route") and data["route"].get("paths"):
            path = data["route"]["paths"][0]
            distance = int(path.get("distance", 0))  # 总距离（米）
            duration = int(path.get("cost", {}).get("duration", 0))  # 总时间（秒）
            steps = path.get("steps", [])
            
            # 构建完整的导航信息
            navigation_info = {
                "distance": distance,
                "duration": duration,
                "steps": steps,
                "first_instruction": steps[0]["instruction"] if steps else "无导航指令"
            }
            
            print(f"导航路线：总距离{distance}米，预计时间{duration}秒")
            print(f"第一步指令：{navigation_info['first_instruction']}")
            return navigation_info
        else:
            return None
    except requests.exceptions.RequestException as e:
        print(f"获取步行导航失败: {e}")
        return None


@app.route('/navigation', methods=['GET'])
def navigation():
    """
    接收GET请求中的destination参数，获取导航路线。
    """
    destination = request.args.get('destination')
    if not destination:
        return jsonify({"error": "请在GET请求中提供 'destination' 参数"}), 400

    destination_gps = get_destination_gps(destination)
    if not destination_gps:
        return jsonify({"error": f"无法获取目的地 '{destination}' 的GPS坐标"}), 404

    navigation_steps = get_walking_navigation(DEFAULT_LOCATION, destination_gps)
    firststep = navigation_steps
    if not navigation_steps:
        return jsonify({"error": f"距离太远了，无法规划从当前位置到 '{destination}' 的步行路线"}), 404

    # return jsonify({"steps": navigation_steps})
    return firststep


# 大模型部分

# 提示词
PROMPT = '''你是一个专为智能导盲杖设计的大语言模型助手。你的核心任务是理解用户的指令，并根据指令类型给出结构化的输出或进行自然的对话。你的主要功能包括：导航指引、天气预报查询以及日常交流。
        你的回答不能超过50字。
        
        指令类型与输出格式：
        
        1. 导航指令识别与输出：
            - 识别条件： 用户输入包含明确导航意图的指令（例如包含"导航到"，"去往"，"怎么走"，"带我去"等关键词）。
            - 输出格式： `导航_${destination}`
            - 示例：
                - 用户输入："请导航到上海虹桥火车站"。
                - 你的输出："导航_上海虹桥火车站"。
            - 关键词提示： 注意识别诸如"导航"，"去"，"到"，"前往"，"带我到"，"怎么走"等与地理位置移动相关的词汇。
        
        2. 退出导航指令识别与输出：
            - 识别条件： 用户输入包含明确退出导航意图的指令（例如包含"退出导航"，"结束导航"，"停止导航"，"取消导航"等关键词）。
            - 输出格式： `退出导航`
            - 示例：
                - 用户输入："退出导航"。
                - 你的输出："退出导航"。
            - 关键词提示： 注意识别诸如"退出"，"结束"，"停止"，"取消"等与终止导航相关的词汇。
        
        3. 天气查询指令识别与输出：
            - 识别条件： 用户输入包含明确天气查询意图的指令（例如包含“天气”，“今天天气”，“天气预报”，“温度”等关键词）。
            - 默认城市： 当用户没有明确说明哪个城市时，默认为杭州市。
            - 输出格式： `天气_${city}_${date}`
            - 示例：
                - 用户输入：“今天天气怎么样？”。
                - 你的输出：“天气_杭州市_今日”。
                - 用户输入：“明天北京天气咋样”。
                - 你的输出：“天气_北京市_明日”。
            - 关键词提示： 注意识别诸如“天气”，“气温”，“温度”，“降雨”，“晴朗”，“雾霾”等与天气状况相关的词汇。同时关注“今天”，“今日”，“明天”等时间限定词。
        
        4. 日常对话处理：
            - 识别条件： 用户的输入不属于导航指令、退出导航指令或天气查询指令。
            - 处理方式： 像一个友善、helpful的助手一样进行自然、流畅的对话回复。
            - 输出： 直接进行文本对话，避免输出结构化格式。
            - 示例：
                - 用户输入：“你好，今天心情不错！”。
                - 你的回复：“你好！很高兴听到你心情不错。今天有什么我可以帮到你的吗？”。
        
        重要注意事项：
        
        - 精确匹配与泛化能力：
            - 你需要能够精确识别关键词，同时也要具备一定的泛化能力，理解用户多样化的表达方式。例如，“我想去江西理工大学怎么走”也应被识别为导航指令。
        - 歧义处理：
            - 考虑到用户输入可能存在歧义，例如“今天天气真好，去江西理工大学吧”，模型需要优先判断指令类型。在这个例子中，虽然包含了“天气”和“江西理工大学”，但整体意图更偏向日常对话，而不是天气查询或导航指令。你需要根据语境进行判断。
        - 错误处理与反馈：
            - 当你无法识别用户意图或无法提取必要信息时，可以设定默认回复，例如：“抱歉，我没有理解您的指令。您是想进行导航还是查询天气，或者进行其他对话呢？”或者更明确地引导用户：“请问您想去哪里导航呢？”或“请问您想查询哪个城市的天气呢？”。
        '''


def call_qwen(messages):
    """调用通义千问 API"""
    try:
        responses = dashscope.Generation.call(
            api_key=AL_API_KEY,
            # model="qwen2.5-14b-instruct-1m", # 聪明一点
            model="qwen-turbo",  # 速度最快
            messages=messages,
            result_format='message',
            stream=True,  # 设置输出方式为流式输出
            incremental_output=True  # 增量式流式输出
        )
        # 单次输出
        # if response.status_code == HTTPStatus.OK:
        #     return response.output.choices[0]['message']
        # else:
        #     print(
        #         f"通义千问 API 调用失败：Request id: {response.request_id}, Status code: {response.status_code}, error code: {response.code}, error message: {response.message}")
        #     return None

        # 使用流式输出
        full_content = ""
        # print("流式输出内容为：")
        for response in responses:
            if response.status_code == HTTPStatus.OK:
                full_content += response.output.choices[0].message.content
                # print(response.output.choices[0]['message']['content'], end='')
            else:
                print('Request id: %s, Status code: %s, error code: %s, error message: %s' % (
                    response.request_id, response.status_code,
                    response.code, response.message
                ))
        # print(f"\n完整内容为：{full_content}")
        return full_content


    except Exception as e:
        print(f"调用通义千问 API 发生异常：{e}")
        return None


# 向CosyVoice发送合成文本
def sendtext(text):
    model = "cosyvoice-v1"
    voice = "longwan"
    synthesizer = SpeechSynthesizer(model=model, voice=voice)
    audio = synthesizer.call(text)
    print('[Metric] requestId: {}, first package delay ms: {}'.format(
        synthesizer.get_last_request_id(),
        synthesizer.get_first_package_delay()))

    with open('output.mp3', 'wb') as f:
        f.write(audio)

    return jsonify(
        {'response':text, 'text': text, 'request': synthesizer.get_last_request_id(), 'delay': synthesizer.get_first_package_delay()})


def handle_ai_response(user_id, qwen_response):
    """处理大模型的响应，并根据响应类型调用相应函数。"""
    if qwen_response.startswith("导航_"):
        destination = qwen_response.split("导航_")[1]
        
        # 首先播报正在规划路线
        planning_msg = "正在为您规划路线，请稍候"
        
        destination_gps = get_destination_gps(destination)
        if destination_gps:
            # 使用当前GPS位置或默认位置
            origin = f"{current_longitude},{current_latitude}" if current_latitude and current_longitude else DEFAULT_LOCATION
            navigation_info = get_walking_navigation(origin, destination_gps)
            
            if navigation_info:
                # 启动导航会话
                start_navigation_session(user_id, destination, destination_gps, navigation_info)
                
                # 构建导航开始的响应
                distance_km = navigation_info['distance'] / 1000
                duration_min = navigation_info['duration'] / 60
                
                summary_msg = f"路线规划完成。全程{distance_km:.1f}公里，大约需要{duration_min:.0f}分钟。{navigation_info['first_instruction']}"
                
                return jsonify({
                    "response": summary_msg,
                    "navigation_started": True,
                    "destination": destination,
                    "total_distance": navigation_info['distance'],
                    "total_duration": navigation_info['duration']
                })
            else:
                return jsonify({"response": f"无法规划到{destination}的路线"})
        else:
            return jsonify({"response": f"找不到{destination}的GPS坐标"})
    elif qwen_response.startswith("天气_"):
        parts = qwen_response.split("_")
        city = parts[1]
        date = parts[2]
        city_id = get_city_id(city)
        if city_id:
            forecast_data = get_weather_forecast(city_id)
            if forecast_data:
                # return jsonify({"response": f"{city}天气预报", "forecasts": forecast_data})
                if date == "今日":
                    today_type = forecast_data[0]["type"]
                    today_temp = forecast_data[0]["low"].split()[1] + "到" + forecast_data[0]["high"].split()[
                        1]  # 10到12℃
                    today_notice = forecast_data[0]["notice"]
                    result = str(city) + "今天" + today_type + "，" + today_temp + "，" + today_notice
                    print(result)
                    return jsonify({"response": result})
                    # return sendtext(result)
                elif date == "明日":
                    future_type = forecast_data[0]["type"]
                    future_temp = forecast_data[0]["low"].split()[1] + "到" + forecast_data[0]["high"].split()[
                        1]  # 10到12℃
                    future_notice = forecast_data[0]["notice"]
                    result = str(city) + "明天" + future_type + "，" + future_temp + "，" + future_notice
                    print(result)
                    return jsonify({"response": result})
            else:
                return jsonify({"response": f"获取{city}的天气预报失败"})
        else:
            return jsonify({"response": f"找不到{city}的城市ID"})
    elif qwen_response == "退出导航":
        # 处理退出导航指令
        return handle_exit_navigation(user_id)
    else:
        # 日常对话
        print(qwen_response)
        return jsonify({"response": qwen_response})
        # return sendtext(qwen_response)


@app.route('/ai', methods=['POST'])
def chat():
    user_id = request.remote_addr  # 使用客户端 IP 作为用户标识符
    user_message = request.json.get('message')

    if not user_message:
        return jsonify({"error": "请求中缺少 'message' 字段"}), 400

    with conversation_lock:
        current_time = time.time()

        # 检查用户是否已存在对话，以及是否超时
        if user_id in conversations:
            if current_time - last_interaction.get(user_id, 0) > CONVERSATION_TIMEOUT:
                # 对话超时，重新开始
                conversations[user_id] = [{
                    'role': 'system',
                    'content': PROMPT
                }]
                print(f"用户 {user_id} 的对话已超时，已重置。")
            last_interaction[user_id] = current_time
        else:
            # 新用户，创建新的对话
            conversations[user_id] = [{
                'role': 'system',
                'content': PROMPT
            }]
            last_interaction[user_id] = current_time
            print(f"为用户 {user_id} 创建新的对话。")

        # 将用户的消息添加到对话历史
        conversations[user_id].append({'role': 'user', 'content': user_message})

        # 调用通义千问 API
        qwen_response = call_qwen(conversations[user_id])

        if qwen_response:
            conversations[user_id].append({'role': 'assistant', 'content': qwen_response})
            print(jsonify({"response": qwen_response}))
            # 将大模型回答传给处理函数
            final = handle_ai_response(user_id, qwen_response)
            return final, 200
        else:
            # 如果调用失败，移除用户刚刚发送的消息，保持对话历史的完整性
            conversations[user_id].pop()
            print(jsonify({"error": "调用通义千问 API 失败，请稍后重试"}), 500)
            return jsonify({"error": "调用通义千问 API 失败，请稍后重试"}), 500

def cosyvoice(text):
    model = "cosyvoice-v2"
    voice = "longwan_v2"

    synthesizer = SpeechSynthesizer(
        model=model,
        voice=voice,
        format=AudioFormat.PCM_16000HZ_MONO_16BIT
    )
    audio = synthesizer.call("今天天气怎么样？")
    print('[Metric] requestId: {}, first package delay ms: {}'.format(
        synthesizer.get_last_request_id(),
        synthesizer.get_first_package_delay()))
    print(audio)

    with open('output.mp3', 'wb') as f:
        f.write(audio)

@app.route('/synthesize_pcm', methods=['GET'])
def synthesize_pcm_audio():
    text = request.args.get('text')
    if not text:
        return jsonify({"error": "Missing 'text' parameter"}), 400

    try:
        synthesizer = SpeechSynthesizer(
            model=model,
            voice=voice,
            format=audio_format
        )
        audio = synthesizer.call(text)

        # 返回 PCM 音频数据流
        return send_file(
            io.BytesIO(audio),
            mimetype='audio/pcm; rate=16000; channels=1; bits=16'
        )

    except Exception as e:
        error_message = f"语音合成失败: {str(e)}"
        print(error_message)
        return jsonify({"error": error_message}), 500
        
# 全局变量用于存储最新的GPS数据
current_latitude = None
current_longitude = None

# 导航会话管理函数
def start_navigation_session(user_id, destination, destination_gps, navigation_info):
    """启动导航会话"""
    with navigation_lock:
        navigation_sessions[user_id] = {
            "destination": destination,
            "destination_gps": destination_gps,
            "navigation_info": navigation_info,
            "current_step": 0,
            "start_time": time.time(),
            "last_update": time.time()
        }
        print(f"用户 {user_id} 开始导航到 {destination}")

def update_navigation_session(user_id):
    """更新导航会话，返回下一步指令"""
    with navigation_lock:
        if user_id not in navigation_sessions:
            return None
            
        session = navigation_sessions[user_id]
        current_time = time.time()
        
        # 检查导航是否超时
        if current_time - session["start_time"] > NAVIGATION_TIMEOUT:
            del navigation_sessions[user_id]
            return {"response": "导航已超时，请重新开始导航"}
        
        # 更新当前步骤
        steps = session["navigation_info"]["steps"]
        current_step = session["current_step"]
        
        if current_step < len(steps) - 1:
            session["current_step"] += 1
            session["last_update"] = current_time
            next_step = steps[session["current_step"]]
            
            return {
                "response": f"下一步：{next_step['instruction']}，距离{next_step['step_distance']}米",
                "step_number": session["current_step"] + 1,
                "total_steps": len(steps),
                "remaining_distance": sum(int(step.get('step_distance', 0)) for step in steps[session["current_step"]:]),
                "navigation_active": True
            }
        else:
            # 导航完成
            del navigation_sessions[user_id]
            return {
                "response": f"恭喜您，已到达目的地{session['destination']}！导航结束。",
                "navigation_completed": True
            }

def handle_exit_navigation(user_id):
    """处理退出导航请求"""
    with navigation_lock:
        if user_id in navigation_sessions:
            destination = navigation_sessions[user_id]["destination"]
            del navigation_sessions[user_id]
            return jsonify({"response": f"已退出到{destination}的导航", "navigation_exited": True})
        else:
            return jsonify({"response": "当前没有进行中的导航"})

def get_navigation_status(user_id):
    """获取用户的导航状态"""
    with navigation_lock:
        if user_id in navigation_sessions:
            session = navigation_sessions[user_id]
            return {
                "navigation_active": True,
                "destination": session["destination"],
                "current_step": session["current_step"],
                "total_steps": len(session["navigation_info"]["steps"]),
                "elapsed_time": int(time.time() - session["start_time"])
            }
        return {"navigation_active": False}

@app.route('/gps', methods=['POST'])
def receive_gps_data():
    """
    接收来自设备的GPS数据并存储在全局变量中。
    预期接收的JSON格式: {"latitude": float, "longitude": float}
    """
    global current_latitude, current_longitude

    if not request.is_json:
        return jsonify({"error": "Request must be JSON"}), 400

    data = request.get_json()

    latitude = data.get('latitude')
    longitude = data.get('longitude')

    if latitude is None or longitude is None:
        return jsonify({"error": "Missing latitude or longitude in JSON data"}), 400

    try:
        # 更新全局变量
        current_latitude = float(latitude)
        current_longitude = float(longitude)
        print(f"接收到GPS数据: Latitude={current_latitude}, Longitude={current_longitude}") # 在服务器控制台打印
        return jsonify({"message": "GPS data received successfully", "latitude": current_latitude, "longitude": current_longitude}), 200
    except ValueError:
        return jsonify({"error": "Invalid latitude or longitude format"}), 400
    except Exception as e:
        print(f"处理GPS数据时出错: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/get_latest_gps', methods=['GET'])
def get_latest_gps():
    """
    提供一个接口用于获取当前存储的最新GPS坐标。
    """
    if current_latitude is not None and current_longitude is not None:
        return jsonify({"latitude": current_latitude, "longitude": current_longitude}), 200
    else:
        return jsonify({"message": "No GPS data available yet"}), 404

@app.route('/navigation_update', methods=['POST'])
def navigation_update():
    """
    更新导航状态，返回下一步指令
    """
    user_id = request.remote_addr
    result = update_navigation_session(user_id)
    
    if result:
        return jsonify(result), 200
    else:
        return jsonify({"response": "当前没有进行中的导航"}), 404

@app.route('/navigation_status', methods=['GET'])
def navigation_status():
    """
    获取用户的导航状态
    """
    user_id = request.remote_addr
    status = get_navigation_status(user_id)
    return jsonify(status), 200

@app.route('/exit_navigation', methods=['POST'])
def exit_navigation():
    """
    退出当前导航
    """
    user_id = request.remote_addr
    return handle_exit_navigation(user_id)


if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=12345)
