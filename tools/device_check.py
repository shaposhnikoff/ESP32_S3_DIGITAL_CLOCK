"""Reset the USB-connected clock and verify NTP and a real weather update."""

import time
from pathlib import Path

import serial
import typer
from esptool.reset import HardReset

ROOT = Path(__file__).resolve().parents[1]


def main(port: str, timeout_seconds: float = 90.0) -> None:
    """Capture one physical boot and require NTP plus successful HTTPS weather."""
    log_path = ROOT / ".pio" / "verification" / "device-boot.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    device = serial.Serial(baudrate=115200, timeout=0.5)
    device.port = port
    device.dtr = False
    device.rts = False
    with device, log_path.open("w", encoding="utf-8") as log:
        device.reset_input_buffer()
        HardReset(device, uses_usb=True)()
        deadline = time.monotonic() + timeout_seconds
        synchronized = False
        weather_updated = False
        while time.monotonic() < deadline:
            line = device.readline().decode("utf-8", errors="replace").rstrip()
            if not line:
                continue
            log.write(line + "\n")
            log.flush()
            typer.echo(line)
            if any(marker in line for marker in ("abort()", "Guru Meditation", "assert failed")):
                typer.echo("DEVICE_CHECK_FAILED: firmware aborted during boot", err=True)
                raise typer.Exit(1)
            if "UTC synchronized" in line:
                synchronized = True
            if "Current weather updated" in line:
                weather_updated = True
            if synchronized and weather_updated:
                typer.echo("DEVICE_CHECK_PASSED: real boot synchronized UTC and fetched weather")
                return
    typer.echo("DEVICE_CHECK_FAILED: NTP and weather not both observed within timeout", err=True)
    raise typer.Exit(1)


if __name__ == "__main__":
    typer.run(main)
