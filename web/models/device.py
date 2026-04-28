from dataclasses import dataclass, field
from typing import Any


@dataclass
class DeviceStatus:
    device_id: str
    battery_percent: int | None = None
    wifi_rssi: int | None = None
    app_state: str = "unknown"
    safety: dict[str, Any] = field(default_factory=dict)
    updated_at: float = 0.0
