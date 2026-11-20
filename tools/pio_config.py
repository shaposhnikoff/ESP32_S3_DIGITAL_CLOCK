"""Generate the configuration before ESP-IDF configures its CMake build."""

import os
import subprocess
from pathlib import Path

from SCons.Script import COMMAND_LINE_TARGETS, Import

Import("env")
project_dir = Path(env.subst("$PROJECT_DIR"))
config_path = Path(os.environ.get("CLOCK_CONFIG", project_dir / "clock.config.toml")).resolve()
if not COMMAND_LINE_TARGETS or not set(COMMAND_LINE_TARGETS) <= {"clean", "fullclean"}:
    subprocess.run(
        [
            "uv", "run", "--project", str(project_dir), "python", "-m", "tools.clockctl",
            "generate", str(config_path), str(project_dir / ".pio" / "generated" / "clock_config.h"),
        ],
        cwd=project_dir,
        check=True,
    )
