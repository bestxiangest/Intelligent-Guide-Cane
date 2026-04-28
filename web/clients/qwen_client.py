from http import HTTPStatus

import dashscope

from config import AL_API_KEY, QWEN_MAX_TOKENS, QWEN_MODEL, QWEN_TEMPERATURE
from utils.logger import get_logger

logger = get_logger(__name__)


def call_qwen(messages: list[dict]) -> str | None:
    if not AL_API_KEY:
        logger.info("[QWEN] missing DashScope API key")
        return None

    dashscope.api_key = AL_API_KEY
    try:
        response = dashscope.Generation.call(
            model=QWEN_MODEL,
            messages=messages,
            result_format="message",
            temperature=QWEN_TEMPERATURE,
            max_tokens=QWEN_MAX_TOKENS,
        )
        if response.status_code == HTTPStatus.OK:
            return response.output.choices[0].message.content.strip()

        logger.warning(
            "[QWEN] request failed request_id=%s status=%s code=%s message=%s",
            getattr(response, "request_id", ""),
            getattr(response, "status_code", ""),
            getattr(response, "code", ""),
            getattr(response, "message", ""),
        )
        return None
    except Exception as exc:
        logger.warning("[QWEN] exception: %s", exc)
        return None
