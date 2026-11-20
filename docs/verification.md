# Verification record

## Current digital face, November 20, 2026

- make test: 38 Python tests passed, plus native Wi-Fi/NTP lifecycle tests
  and native tests of the production cJSON weather parser.
- make lint: Ruff passed.
- make preview: actual LVGL pixel checks passed for unchanged time, seconds
  growing from 13 to 14 to 59, reset at the minute boundary, constant orange
  colon, local midnight and both time formats.
- The subsequent minute-only change passed a regression test proving no
  delayed redraw after a minute transition; hour animation remains active.
- make verify-build: clean firmware build passed. Changing brightness from
  80 to 37 then changed both main.c.o and firmware.bin.
- Clean build reported 40,028 bytes static RAM and 1,459,389 bytes flash.
  Dynamic allocations, including PSRAM glyph canvases, are additional.
- A real HTTPS request to Open-Meteo for the configured Kyiv point succeeded
  and returned numeric current temperature. This proves host access, not MCU TLS.

Before the first local Git commit, a root build encountered ESP-IDF's Git-ref
lookup for the unborn main branch. The successful clean source-copy build
excludes that temporary Git metadata and verifies the actual firmware source.

## Screenshots

All images are deterministic captures of the actual LVGL software renderer
at 1024 x 600, not board photographs. The primary fixture is November 20,
2026, 20:11:13, 4 degrees, OVERCAST and strong Wi-Fi. Production never imports
these fixtures. See README for the screenshot links.

## Preceding physical hardware verification

Factory LCD_CAM registers describe 1024 x 600. After correcting the 800 x 480
profile, the user observed color bands, the clock and its transitions.
The preceding six-digit version was uploaded, then uploaded again at
brightness 30. Boot logs confirmed Wi-Fi and NTP, with synchronization at
approximately 5.359 seconds in the latter check.

The user subsequently requested uploading the new digital face. Upload
succeeded with all three image hashes verified, using the real deployment
configuration at brightness 30 and Kyiv coordinates 50.45 / 30.523.
The subsequent real boot logged UTC synchronization at 5735 ms and
`Current weather updated` at 9253 ms. The bounded device check passed,
and the user responded positively after seeing the result.

After the minute-animation adjustment, another upload and physical boot
check passed: UTC synchronization at 5716 ms, weather update at 10505 ms.

This verifies actual MCU Wi-Fi, NTP and HTTPS weather operation. Prolonged
network recovery under weather load and measured PWM flicker remain
unverified. Native screenshots remain fixtures, not hardware photos.

## Publication safety

Real TOML, generated credential headers, SDK/build trees, firmware binaries
and local reference files are excluded. Build does not upload. Deploy requires
an explicit port. There is no serial or remote runtime settings interface.
Editor SDK include paths are not configured; native and target compiler
results are the authoritative diagnostics.
