"""Parse deployment configuration and emit read-only C constants."""

import os
import re
import tempfile
import tomllib
from pathlib import Path
from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field, SecretStr, field_validator
from pydantic_core import PydanticCustomError

from tools.timezone import timezone_posix


class ConfigModel(BaseModel):
    """Reject unknown fields and coercion at the deployment boundary."""

    model_config = ConfigDict(frozen=True, extra="forbid", strict=True)


class WifiConfig(ConfigModel):
    """Credentials embedded in flash, never printed."""

    ssid: str
    password: SecretStr

    @field_validator("ssid")
    @classmethod
    def valid_ssid(cls, value: str) -> str:
        """Enforce the ESP-IDF byte limit and forbid embedded NULs."""
        if not 1 <= len(value.encode("utf-8")) <= 32 or "\0" in value:
            raise PydanticCustomError("ssid", "must contain 1..32 UTF-8 bytes without NUL")
        return value

    @field_validator("password")
    @classmethod
    def valid_password(cls, value: SecretStr) -> SecretStr:
        """Allow open, WPA passphrase, or WPA hexadecimal PSK."""
        text = value.get_secret_value()
        if text == "" or re.fullmatch(r"[\x20-\x7e]{8,63}|[0-9a-fA-F]{64}", text):
            return value
        raise PydanticCustomError("password", "must be empty, 8..63 ASCII, or 64 hex digits")


class TimeConfig(ConfigModel):
    """Time policy with an explicitly supported timezone mapping."""

    ntp_servers: Annotated[list[str], Field(min_length=1, max_length=3)]
    sync_interval_seconds: Annotated[int, Field(ge=60, le=86400)]
    timezone: Literal["Europe/Kyiv", "Etc/UTC", "UTC"]
    format: Literal["24h", "12h"]
    locale: Literal["ru_RU", "uk_UA", "en_US"]

    @field_validator("ntp_servers")
    @classmethod
    def valid_servers(cls, servers: list[str]) -> list[str]:
        """Accept DNS hostnames or IPv4 literals without injection characters."""
        for server in servers:
            if len(server) > 253 or not re.fullmatch(
                r"[a-zA-Z0-9](?:[a-zA-Z0-9.-]*[a-zA-Z0-9])?", server
            ):
                raise PydanticCustomError("ntp_servers", "contains an invalid hostname")
            if any(
                not label or len(label) > 63 or label.startswith("-") or label.endswith("-")
                for label in server.split(".")
            ):
                raise PydanticCustomError("ntp_servers", "contains an invalid DNS label")
        if len(set(servers)) != len(servers):
            raise PydanticCustomError("ntp_servers", "must not contain duplicate servers")
        return servers


class DisplayConfig(ConfigModel):
    """Fixed application brightness."""

    brightness: Annotated[int, Field(ge=0, le=100)]


class WeatherConfig(ConfigModel):
    """Open-Meteo location and polling policy, fixed at deployment."""

    latitude: Annotated[float, Field(ge=-90, le=90)]
    longitude: Annotated[float, Field(ge=-180, le=180)]
    update_seconds: Annotated[int, Field(ge=60, le=86400)]
    request_timeout_ms: Annotated[int, Field(ge=1000, le=60000)]
    stale_after_seconds: Annotated[int, Field(ge=60, le=604800)]


class ClockConfig(ConfigModel):
    """All supported deployment parameters."""

    board: Literal["waveshare-esp32-s3-touch-lcd-5b"]
    wifi: WifiConfig
    time: TimeConfig
    display: DisplayConfig
    weather: WeatherConfig


def load_config(path: Path) -> ClockConfig:
    """Load TOML into the validated configuration contract."""
    with path.open("rb") as source:
        return ClockConfig.model_validate(tomllib.load(source))


def c_string(value: str) -> str:
    """Use fixed-width octal escapes to preserve arbitrary UTF-8 bytes safely."""
    return '"' + "".join(f"\\{byte:03o}" for byte in value.encode("utf-8")) + '"'


def render_header(config: ClockConfig) -> str:
    """Render compile-time constants without mutable configuration storage."""
    servers = ", ".join(c_string(server) for server in config.time.ntp_servers)
    return "\n".join(
        [
            "/* Generated from deployment TOML. Contains credentials. Do not commit. */",
            "#pragma once",
            "#define CLOCK_BOARD_5B 1",
            f"#define CLOCK_WIFI_SSID {c_string(config.wifi.ssid)}",
            f"#define CLOCK_WIFI_PASSWORD {c_string(config.wifi.password.get_secret_value())}",
            f"#define CLOCK_NTP_SERVERS {{{servers}}}",
            f"#define CLOCK_NTP_SERVER_COUNT {len(config.time.ntp_servers)}",
            f"#define CLOCK_SYNC_INTERVAL_SECONDS {config.time.sync_interval_seconds}",
            f"#define CLOCK_TIMEZONE_POSIX {c_string(timezone_posix(config.time.timezone))}",
            f"#define CLOCK_FORMAT_24H {int(config.time.format == '24h')}",
            f"#define CLOCK_LOCALE {c_string(config.time.locale)}",
            f"#define CLOCK_BRIGHTNESS {config.display.brightness}",
            f"#define CLOCK_WEATHER_LATITUDE {config.weather.latitude:.6f}",
            f"#define CLOCK_WEATHER_LONGITUDE {config.weather.longitude:.6f}",
            f"#define CLOCK_WEATHER_UPDATE_SECONDS {config.weather.update_seconds}",
            f"#define CLOCK_WEATHER_TIMEOUT_MS {config.weather.request_timeout_ms}",
            f"#define CLOCK_WEATHER_STALE_SECONDS {config.weather.stale_after_seconds}",
            "",
        ]
    )


def write_header(config: ClockConfig, destination: Path) -> bool:
    """Atomically replace changed headers, retaining timestamps for unchanged input."""
    content = render_header(config)
    if destination.exists() and destination.read_text() == content:
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", dir=destination.parent, delete=False
    ) as target:
        target.write(content)
        temporary = Path(target.name)
    os.replace(temporary, destination)
    return True
