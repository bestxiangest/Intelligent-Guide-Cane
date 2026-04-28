import logging
import re


SENSITIVE_RE = re.compile(r"(api[_-]?key|secret|token|password)=([^&\s]+)", re.IGNORECASE)


def redact(value: object) -> str:
    return SENSITIVE_RE.sub(r"\1=***", str(value))


def get_logger(name: str) -> logging.Logger:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
    )
    return logging.getLogger(name)
