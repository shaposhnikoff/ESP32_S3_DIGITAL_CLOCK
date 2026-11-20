"""Deployment boundary tests: invalid input cannot reach a flash command."""

import os
import subprocess
from pathlib import Path

import pytest
from pydantic import ValidationError
from typer.testing import CliRunner

from tools.clockctl import app
from tools.config import load_config, render_header, write_header

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = (ROOT / "clock.config.example.toml").read_text()


@pytest.mark.parametrize(
    ("before", "after", "field"),
    [
        ("brightness = 80", "brightness = 101", "display.brightness"),
        ("brightness = 80", "brightness = true", "display.brightness"),
        ("brightness = 80", 'brightness = "80"', "display.brightness"),
        ('password = "CHANGE_ME_123"', 'password = "secret"', "wifi.password"),
        ('ssid = "YOUR_WIFI_SSID"', 'ssid = ""', "wifi.ssid"),
        ('ssid = "YOUR_WIFI_SSID"', f'ssid = "{"я" * 17}"', "wifi.ssid"),
        ('timezone = "Europe/Kyiv"', 'timezone = "Europe/Unknown"', "time.timezone"),
        (
            "sync_interval_seconds = 3600",
            "sync_interval_seconds = 59",
            "time.sync_interval_seconds",
        ),
        ('format = "24h"', 'format = "25h"', "time.format"),
        ('locale = "ru_RU"', 'locale = "xx_XX"', "time.locale"),
        ("brightness = 80", "brightnes = 80", "display"),
        ("brightness = 80", "brightness = 80\nbrightnes = 20", "display.brightnes"),
        ('board = "waveshare-esp32-s3-touch-lcd-5b"', 'board = "5B"', "board"),
        (
            'board = "waveshare-esp32-s3-touch-lcd-5b"',
            'board = "waveshare-esp32-s3-touch-lcd-5"',
            "board",
        ),
        ('["time.cloudflare.com", "pool.ntp.org", "time.google.com"]', "[]", "time.ntp_servers"),
        (
            '["time.cloudflare.com", "pool.ntp.org", "time.google.com"]',
            '["a", "b", "c", "d"]',
            "time.ntp_servers",
        ),
        (
            '["time.cloudflare.com", "pool.ntp.org", "time.google.com"]',
            '["bad/server"]',
            "time.ntp_servers",
        ),
        (
            '["time.cloudflare.com", "pool.ntp.org", "time.google.com"]',
            '["a", "a"]',
            "time.ntp_servers",
        ),
    ],
)
def test_invalid_field_is_rejected_without_disclosing_values(
    tmp_path: Path, before: str, after: str, field: str
) -> None:
    config = tmp_path / "invalid.toml"
    config.write_text(EXAMPLE.replace(before, after))

    result = CliRunner().invoke(app, ["validate", str(config)])

    assert result.exit_code == 2
    assert field in result.output
    assert "CHANGE_ME_123" not in result.output
    assert "secret" not in result.output


def test_invalid_deploy_stops_before_platformio(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    config = tmp_path / "invalid.toml"
    config.write_text(EXAMPLE.replace("brightness = 80", "brightness = -1"))
    marker = tmp_path / "pio-was-called"
    pio = tmp_path / "pio"
    pio.write_text(f'#!/bin/sh\ntouch "{marker}"\nexit 42\n')
    pio.chmod(0o700)
    monkeypatch.setenv("PATH", f"{tmp_path}{os.pathsep}{os.environ['PATH']}")

    result = CliRunner().invoke(app, ["deploy", str(config), "--port", "test-port"])

    assert result.exit_code == 2
    assert "display.brightness" in result.output
    assert not marker.exists()


def test_malformed_toml_error_does_not_echo_password(tmp_path: Path) -> None:
    config = tmp_path / "broken.toml"
    config.write_text('password = "VERY_PRIVATE" invalid\n')

    result = CliRunner().invoke(app, ["validate", str(config)])

    assert result.exit_code == 2
    assert "TOML" in result.output
    assert "VERY_PRIVATE" not in result.output


def test_header_changes_only_when_configuration_changes(tmp_path: Path) -> None:
    config_path = tmp_path / "clock.toml"
    config_path.write_text(EXAMPLE)
    header = tmp_path / "generated" / "clock_config.h"
    config = load_config(config_path)
    write_header(config, header)
    original = header.read_bytes()
    original_mtime = header.stat().st_mtime_ns

    changed = write_header(config, header)

    assert not changed
    assert header.stat().st_mtime_ns == original_mtime
    config_path.write_text(EXAMPLE.replace("brightness = 80", "brightness = 37"))
    write_header(load_config(config_path), header)
    assert header.read_bytes() != original
    assert b"#define CLOCK_BRIGHTNESS 37" in header.read_bytes()
    assert header.stat().st_mode & 0o777 == 0o600


def test_generated_utf8_and_quotes_survive_the_c_compiler(tmp_path: Path) -> None:
    config = tmp_path / "clock.toml"
    config.write_text(EXAMPLE.replace('ssid = "YOUR_WIFI_SSID"', """ssid = 'Київ"\\123'"""))
    write_header(load_config(config), tmp_path / "clock_config.h")
    source = tmp_path / "consumer.c"
    source.write_text(
        '#include <stdio.h>\n#include "clock_config.h"\n'
        "int main(void) {fputs(CLOCK_WIFI_SSID, stdout); return 0;}\n"
    )
    subprocess.run(["/usr/bin/cc", "consumer.c", "-o", "consumer"], cwd=tmp_path, check=True)

    result = subprocess.run(["./consumer"], cwd=tmp_path, check=True, capture_output=True)

    assert result.stdout == 'Київ"\\123'.encode()


def test_invalid_cli_is_rejected_in_real_process(tmp_path: Path) -> None:
    config = tmp_path / "clock.toml"
    config.write_text(EXAMPLE.replace("brightness = 80", "brightness = 101"))

    result = subprocess.run(
        ["/usr/bin/env", "python", "-m", "tools.clockctl", "build", "clock.toml"],
        cwd=tmp_path,
        env=os.environ | {"PYTHONPATH": str(ROOT)},
        capture_output=True,
        check=False,
    )

    assert result.returncode == 2
    assert b"display.brightness" in result.stderr


def test_configuration_is_immutable() -> None:
    config = load_config(ROOT / "clock.config.example.toml")

    with pytest.raises(ValidationError, match="frozen"):
        config.display.brightness = 20


def test_weather_coordinates_are_readonly_build_parameters() -> None:
    config = load_config(ROOT / "clock.config.example.toml")
    header = render_header(config)
    assert "#define CLOCK_WEATHER_LATITUDE 50.450000" in header
    assert "#define CLOCK_WEATHER_LONGITUDE 30.523000" in header
    with pytest.raises(ValidationError, match="frozen"):
        config.weather.latitude = 0.0


@pytest.mark.parametrize(
    ("before", "after"),
    [
        ("latitude = 50.45", "latitude = 91.0"),
        ("longitude = 30.523", "longitude = -181.0"),
        ("update_seconds = 900", "update_seconds = 0"),
        ("request_timeout_ms = 15000", "request_timeout_ms = 0"),
    ],
)
def test_invalid_weather_parameters_are_rejected(tmp_path: Path, before: str, after: str) -> None:
    config = tmp_path / "weather.toml"
    config.write_text(EXAMPLE.replace(before, after))
    result = CliRunner().invoke(app, ["validate", str(config)])
    assert result.exit_code == 2
    assert "weather." in result.output
