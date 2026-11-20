"""Build a clean source copy and verify a changed TOML rebuilds its consumers."""

import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    """Identify an actual compiler output without depending on log wording."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    """Run two actual PlatformIO builds without touching the user's config."""
    snapshot = Path(tempfile.mkdtemp(prefix="clock-clean-")) / "source"
    shutil.copytree(
        ROOT, snapshot,
        ignore=shutil.ignore_patterns(
            ".git", ".pio", ".venv", "managed_components", "__pycache__",
            ".pytest_cache", ".ruff_cache", "clock.config.toml", "sdkconfig.waveshare-clock*",
        ),
    )
    config = snapshot / "clock.config.toml"
    shutil.copyfile(snapshot / "clock.config.example.toml", config)
    reports = ROOT / ".pio" / "verification"
    reports.mkdir(parents=True, exist_ok=True)
    outputs = snapshot / ".pio" / "build" / "waveshare-clock"
    with (reports / "clean-build.log").open("w") as log:
        subprocess.run(
            ["/usr/bin/env", "uv", "run", "python", "-m", "tools.clockctl",
             "build", "clock.config.toml"],
            cwd=snapshot, stdout=log, stderr=subprocess.STDOUT, check=True,
        )
    image_before = digest(outputs / "firmware.bin")
    object_before = digest(outputs / "src" / "main.c.o")
    config.write_text(config.read_text().replace("brightness = 80", "brightness = 37"))
    with (reports / "incremental-build.log").open("w") as log:
        subprocess.run(
            ["/usr/bin/env", "uv", "run", "python", "-m", "tools.clockctl",
             "build", "clock.config.toml"],
            cwd=snapshot, stdout=log, stderr=subprocess.STDOUT, check=True,
        )
    image_after = digest(outputs / "firmware.bin")
    object_after = digest(outputs / "src" / "main.c.o")
    if object_before == object_after or image_before == image_after:
        msg = "Changed brightness did not rebuild the configuration consumer and image"
        raise RuntimeError(msg)
    report = {
        "clean_source": str(snapshot),
        "clean_build": "passed",
        "incremental_build": "passed",
        "brightness": [80, 37],
        "main_object_sha256": [object_before, object_after],
        "firmware_sha256": [image_before, image_after],
    }
    (reports / "build-results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
