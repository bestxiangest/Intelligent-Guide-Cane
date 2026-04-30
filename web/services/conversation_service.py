import threading
import time

from config import CONVERSATION_TIMEOUT

SYSTEM_PROMPT = """
你是智能导盲杖服务端的意图识别器。
你只负责把用户输入转换成严格 JSON 意图，不负责执行导航、天气、设备控制等真实操作。
你必须只输出 JSON，不要 Markdown，不要解释。
意图枚举只能从以下列表中选择：
navigation.start, navigation.exit, navigation.update, navigation.status, weather.query, chat.normal, location.status, device.status, system.help, unknown。

当用户请求去某地、带路、导航到某地，输出 navigation.start，并提取 destination。
当用户要求停止、取消、退出导航，输出 navigation.exit。
当用户问下一步、怎么走、继续导航，输出 navigation.update。
当用户问还有多远、到哪了、快到了吗，输出 navigation.status。
当用户问天气、温度、下雨、冷不冷、热不热，输出 weather.query。
当用户只是打招呼、普通闲聊，或请求讲笑话、讲故事、陪聊天、安慰、鼓励、简单问答，输出 chat.normal，并给出 50 字以内 reply。
只有在用户输入缺失、完全无法理解，或意图既不是上述真实功能也不是普通聊天时，才输出 unknown。

示例：
用户：给我讲个笑话听听。
输出：{"intent":"chat.normal","confidence":0.95,"arguments":{},"reply":"当然。路灯为什么不睡觉？因为它一闭眼，整条街就黑了。","safety_level":"normal"}

必须输出如下 JSON：
{"intent":"...","confidence":0.0,"arguments":{},"reply":"","safety_level":"normal"}

不要输出除 JSON 以外的任何内容。
""".strip()

_conversations: dict[str, list[dict]] = {}
_last_interaction: dict[str, float] = {}
_lock = threading.Lock()


def build_messages(device_id: str, user_message: str) -> list[dict]:
    now = time.time()
    with _lock:
        expired = now - _last_interaction.get(device_id, 0) > CONVERSATION_TIMEOUT
        if device_id not in _conversations or expired:
            _conversations[device_id] = [{"role": "system", "content": SYSTEM_PROMPT}]
        _last_interaction[device_id] = now
        _conversations[device_id].append({"role": "user", "content": user_message})
        _conversations[device_id] = _conversations[device_id][:1] + _conversations[device_id][-6:]
        return list(_conversations[device_id])


def record_assistant(device_id: str, content: str) -> None:
    with _lock:
        if device_id in _conversations:
            _conversations[device_id].append({"role": "assistant", "content": content})
            _conversations[device_id] = _conversations[device_id][:1] + _conversations[device_id][-6:]
