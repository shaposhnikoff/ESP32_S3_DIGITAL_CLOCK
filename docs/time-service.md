# Time and network service

`src/network_time.c` targets the installed ESP-IDF 5.5.0 interfaces.
The service starts a dedicated FreeRTOS worker, then returns to the LVGL loop.
There is no RTC access or persistent time-valid flag.

At every boot the flag starts false, even if the ESP32 system clock happens
to retain a plausible value after a reset. Only the actual SNTP notification
callback makes it true. ESP-IDF's immediate synchronization mode sets system
UTC before invoking that callback. The UI polls the flag with C atomics, then
uses `localtime_r` for the configured POSIX timezone.

Wi-Fi events overwrite a one-element queue. The worker sees the latest state,
including when UP and DOWN arrive before it is scheduled. A successful IP
event restarts SNTP. A disconnect schedules another connection attempt after
1, 2, 4, 8, 16, 32, then at most 60 seconds. Acquiring an IP resets the delay.
The worker never clears the time-valid flag after the first synchronization:
system UTC continues to advance while offline.

The configured NTP names occupy lwIP server slots in TOML order. On first
boot slot zero is tried first. DNS failure, receive timeout, or a rejected
server makes lwIP try the next configured slot cyclically. A working server
is retained for subsequent polls. Restarting SNTP after reconnect retains the
current slot; it does not force every reconnect back to the first name.
No server is accepted from DHCP.

Application credentials are copied into the Wi-Fi driver's RAM configuration.
Wi-Fi NVS and PHY calibration storage are disabled; no configuration is read
from NVS. The firmware has no network or serial handler that changes settings.

## Official source references

- [SNTP configuration and API](https://github.com/espressif/esp-idf/blob/v5.5/components/esp_netif/include/esp_netif_sntp.h)
- [Thread-safe SNTP initialization and restart](https://github.com/espressif/esp-idf/blob/v5.5/components/esp_netif/lwip/esp_netif_sntp.c)
- [ESP SNTP synchronization callback contract](https://github.com/espressif/esp-idf/blob/v5.5/components/lwip/include/apps/esp_sntp.h)
- [Wi-Fi station example](https://github.com/espressif/esp-idf/blob/v5.5/examples/wifi/getting_started/station/main/station_example_main.c)
- The installed `framework-espidf` 3.50500.0 file
  `components/lwip/lwip/src/apps/sntp/sntp.c`, specifically
  `sntp_try_next_server`, `sntp_init`, and `sntp_stop`, establishes slot order
  and the retained-server behavior described above.

## Host lifecycle test

`make test-network` compiles the actual service source with a deterministic
FreeRTOS/ESP-IDF boundary fake. It drives the registered event handler and
captured worker entry, rather than a copy of the service logic.

It checks initial invalid time, no validity from Wi-Fi alone, the real sync
callback making time valid, local UTC conversion, continued time while offline,
SNTP restart on reconnection, latest-state queue behavior, maximum backoff, and
resetting that backoff after success.

This simulation does not test the radio, DHCP, actual NTP packets, or device
oscillator accuracy. Those require the physical board and a configured network.
