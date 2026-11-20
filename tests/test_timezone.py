"""Check local dates and seasonal transitions using the generated firmware TZ."""

import os
import time
from datetime import UTC, datetime

import pytest

from tools.timezone import timezone_posix, verify_timezone


@pytest.mark.parametrize(
    ("utc", "local"),
    [
        ("2026-03-29T00:59:00", (2026, 3, 29, 2, 59)),
        ("2026-03-29T01:00:00", (2026, 3, 29, 4, 0)),
        ("2026-10-25T00:59:00", (2026, 10, 25, 3, 59)),
        ("2026-10-25T01:00:00", (2026, 10, 25, 3, 0)),
        ("2026-11-20T21:59:00", (2026, 11, 20, 23, 59)),
        ("2026-11-20T22:00:00", (2026, 11, 21, 0, 0)),
    ],
)
def test_kyiv_converts_utc_at_seasonal_and_midnight_boundaries(
    monkeypatch: pytest.MonkeyPatch, utc: str, local: tuple[int, int, int, int, int]
) -> None:
    original = os.environ.get("TZ")
    monkeypatch.setenv("TZ", timezone_posix("Europe/Kyiv"))
    time.tzset()
    timestamp = datetime.fromisoformat(utc).replace(tzinfo=UTC).timestamp()

    try:
        actual = time.localtime(timestamp)
        assert actual[:5] == local
    finally:
        if original is None:
            os.environ.pop("TZ", None)
        else:
            os.environ["TZ"] = original
        time.tzset()


@pytest.mark.parametrize("zone", ["Europe/Kyiv", "Etc/UTC", "UTC"])
def test_generated_timezone_matches_pinned_iana_rules(zone: str) -> None:
    verify_timezone(zone)
