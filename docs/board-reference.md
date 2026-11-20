# Board source and adaptation

The reported PCB marking is ESP32-S3-Touch-LCD-5, but the attached panel
requires **1024 x 600**. The supplied factory-register handoff
records LCD_CAM registers `0x6004101c=0xce12b8ab` and
`0x60041020=0x4b33ff15`, decoding to 1024 x 600. In this project the user
confirmed raw color bands, the clock, and its flip animation after the
panel-mode correction. The former 800 x 480 mode showed vertical stripes
even before LVGL started.

The active software profile is `waveshare-esp32-s3-touch-lcd-5b`; this names
the panel mode, not a separately proven PCB revision. It uses a requested
21 MHz PCLK, horizontal pulse/back/front 30/145/170 and vertical 2/23/12.
One framebuffer and a 40-line LVGL draw buffer are in PSRAM; two 10-line
bounce buffers use internal RAM.

The pinout and original reference are in Waveshare's official repository at commit
`77252e5ebd2f1ff9b06e22d8693903c960652669`:

- [Official ESP-IDF RGB port](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-5/blob/77252e5ebd2f1ff9b06e22d8693903c960652669/examples/ESP-IDF/08_lvgl_v8_demo/components/waveshare_rgb_lcd_port.c)
- [Official pin definitions](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-5/blob/77252e5ebd2f1ff9b06e22d8693903c960652669/examples/ESP-IDF/08_lvgl_v8_demo/components/waveshare_rgb_lcd_port.h)
- [Official board configuration and LCD reset](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-5/blob/77252e5ebd2f1ff9b06e22d8693903c960652669/examples/Arduino/examples/09_lvgl_v8_demo/esp_panel_board_custom_conf.h)

RGB565, falling-edge pixel clock and all sixteen data pins follow the
official family pinout. The tested 1024 x 600 mode uses the timing branch
for that panel; the 800 x 480 branch is not used.

`sdkconfig.defaults` also follows that example's 64 KB data cache and
64-byte cache lines. When updating an existing workspace, select
`CONFIG_ESP32S3_DATA_CACHE_64KB` and
`CONFIG_ESP32S3_DATA_CACHE_LINE_64B` in its generated
`sdkconfig.waveshare-clock` too: defaults do not override existing choices.
The generated build header must report a data-cache size of `0x10000` and
a cache-line size of `64`. These settings match the vendor configuration;
they are not proof of physical display correctness.

`CONFIG_LV_CONF_SKIP=n` makes the target and native renderer use the same
`include/lv_conf.h`. The MCU remains on the pinned ESP-IDF 5.5.0 and
LVGL 8.3.11 stack. The handoff supplied hardware evidence; those working
dependencies did not need to change.

I2C uses SDA 8, SCL 9, 400 kHz. The CH422G mode command is address 0x24,
value 0x01. Its output command is address 0x38. LCD reset is output bit 3;
backlight is output bit 2. The official backlight-on output value is 0x1E.
The adaptation retains the other output bits, resets the LCD with bit 3,
and uses the ESP-IDF 5.5 master-I2C API instead of the legacy I2C API.
Touch is not initialized.

The CH422G backlight interface is a binary switch, with no hardware dimmer.
To honor deployment percentages, intermediate values use approximately 100 Hz
software PWM on that switch. A dedicated task performs the I2C writes;
the timer callback only notifies that task. Values 0 and 100 use constant
off/on output without a PWM task. Scheduler and I2C latency affect the exact
duty cycle, especially at the extremes. Physical flicker and brightness
must be checked on the actual board; no hardware measurement is claimed.

RTC is deliberately unused. Every boot waits for NTP as requested.

The HTTPS weather task uses a 64 KiB internal-memory reserve from
`sdkconfig.defaults`. When reusing an older generated sdkconfig, also align
`CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536`; defaults do not override it.
