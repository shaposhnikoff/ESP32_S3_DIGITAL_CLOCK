"""Validate, build, and deploy the firmware from a single TOML file."""

import os
import subprocess
import tomllib
from pathlib import Path
from typing import Annotated

import typer
from pydantic import ValidationError

from tools.config import ClockConfig, load_config, write_header
from tools.timezone import verify_timezone

app = typer.Typer(pretty_exceptions_enable=False)
ROOT = Path(__file__).resolve().parents[1]


def checked_config(path: Path) -> ClockConfig:
    """Report field-level errors without displaying any supplied values."""
    try:
        config = load_config(path)
        verify_timezone(config.time.timezone)
        return config
    except ValidationError as error:
        for item in error.errors(include_input=False, include_url=False, include_context=False):
            location = ".".join(str(part) for part in item["loc"])
            typer.echo(f"{location}: {item['msg']}", err=True)
        raise typer.Exit(2) from None
    except tomllib.TOMLDecodeError:
        typer.echo("config: invalid TOML syntax", err=True)
        raise typer.Exit(2) from None
    except (OSError, RuntimeError) as error:
        typer.echo(f"config: {error}", err=True)
        raise typer.Exit(2) from None


def pio_run(config_path: Path, upload_port: str | None = None) -> None:
    """Run PlatformIO, carrying only the path through the process environment."""
    environment = os.environ | {"CLOCK_CONFIG": str(config_path.resolve())}
    if upload_port is not None:
        environment["PLATFORMIO_UPLOAD_PORT"] = upload_port
    try:
        if upload_port is None:
            subprocess.run(
                ["/usr/bin/env", "pio", "run", "-e", "waveshare-clock"],
                env=environment, cwd=ROOT, check=True,
            )
        else:
            subprocess.run(
                ["/usr/bin/env", "pio", "run", "-e", "waveshare-clock", "-t", "upload"],
                env=environment, cwd=ROOT, check=True,
            )
    except FileNotFoundError:
        typer.echo("platformio: pio is not installed or not on PATH", err=True)
        raise typer.Exit(2) from None
    except subprocess.CalledProcessError as error:
        raise typer.Exit(error.returncode) from None


@app.command()
def validate(config: Path) -> None:
    """Validate all fields and the seasonal-time conversion before building."""
    checked_config(config)
    typer.echo("Configuration valid; timezone checked against tzdata for 2025..2035.")


@app.command()
def generate(config: Path, destination: Path) -> None:
    """Generate the private C header used by the PlatformIO pre-build hook."""
    parsed = checked_config(config)
    write_header(parsed, destination)
    typer.echo("Read-only firmware configuration generated.")


@app.command()
def build(config: Path) -> None:
    """Validate and build the image through PlatformIO."""
    checked_config(config)
    pio_run(config)


@app.command()
def deploy(
    config: Path,
    port: Annotated[str, typer.Option(help="Explicit serial upload port.")],
) -> None:
    """Validate, build, and flash the configured image to the explicit port."""
    checked_config(config)
    pio_run(config)
    pio_run(config, port)


if __name__ == "__main__":
    app()
