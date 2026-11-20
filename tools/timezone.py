"""Translate supported IANA zones to newlib POSIX TZ and verify its rules."""

import os
import time
from datetime import UTC, datetime, timedelta
from importlib.resources import files
from typing import Literal, assert_never
from zoneinfo import ZoneInfo


def timezone_posix(zone: Literal["Europe/Kyiv", "Etc/UTC", "UTC"]) -> str:
    """Return the firmware TZ rule for the configured IANA zone."""
    match zone:
        case "Europe/Kyiv":
            return "EET-2EEST,M3.5.0/3,M10.5.0/4"
        case "Etc/UTC" | "UTC":
            return "UTC0"
        case unreachable:
            assert_never(unreachable)


def verify_timezone(zone: Literal["Europe/Kyiv", "Etc/UTC", "UTC"]) -> None:
    """Check POSIX conversion against pinned tzdata hourly over 2025..2035."""
    resource = files("tzdata.zoneinfo").joinpath(*zone.split("/"))
    with resource.open("rb") as source:
        reference = ZoneInfo.from_file(source)
    original = os.environ.get("TZ")
    try:
        os.environ["TZ"] = timezone_posix(zone)
        time.tzset()
        instant = datetime(2025, 1, 1, tzinfo=UTC)
        end = datetime(2036, 1, 1, tzinfo=UTC)
        while instant < end:
            timestamp = instant.timestamp()
            actual = time.localtime(timestamp)
            expected = instant.astimezone(reference)
            if actual[:6] != expected.timetuple()[:6]:
                msg = f"time.timezone: POSIX rule differs from tzdata at {instant.isoformat()}"
                raise RuntimeError(msg)
            instant += timedelta(hours=1)
    finally:
        if original is None:
            os.environ.pop("TZ", None)
        else:
            os.environ["TZ"] = original
        time.tzset()
