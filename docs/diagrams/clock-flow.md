# Startup and background tasks

The verified software profile is 1024 x 600. RTC and touch are unused.
The face shows large HH:MM, a separate second counter and 60-mark scale.

```mermaid
flowchart TD
    TOML[Deployment TOML] --> Validate[Validate parameters]
    Validate -->|Invalid| Stop[Exit before build or upload]
    Validate -->|Valid| Header[Generate private immutable C constants]
    Header --> Build[Build firmware]
    Build -->|Explicit deploy with port| Upload[Upload to board]
    Upload --> Boot[Initialize panel and brightness]
    Boot --> Waiting[Display dashes and inactive second scale]
    Boot --> Network[Start Wi-Fi and NTP worker]
    Network --> UTC[Mark UTC valid after NTP]
    UTC --> UI[Convert to local time and update changed UI fields]
    UI --> Flip[Animate changed hour glyphs]
    UI --> Minutes[Replace changed minute glyphs immediately]
    UI --> Scale[Update seconds and reset scale at zero]
    UTC --> Weather[Background HTTPS weather request]
    Weather --> Parse[Validate bounded current-weather JSON]
    Parse --> Snapshot[Publish snapshot under short critical section]
    Snapshot --> UI
    Network --> Signal[Read actual Wi-Fi RSSI]
    Signal --> UI
    UTC -->|Connection lost| Offline[Keep system clock running]
    Offline --> Retry[Reconnect with bounded backoff]
    Retry --> Network
```

A one-element overwrite queue delivers the latest Wi-Fi state to the network
worker, which owns reconnect attempts and SNTP restarts. Backoff grows from 1
to 60 seconds and resets after acquiring an IP. No network call runs in LVGL.
The weather task waits for Wi-Fi and NTP before TLS. It verifies the server
using the ESP certificate bundle, bounds responses to 4095 bytes and validates
temperature, integer WMO code and integer UTC timestamp. Failed requests keep
a previous reading only until its age limit. Unavailable weather shows dashes.
Weather is not persisted in NVS.

```mermaid
stateDiagram-v2
    [*] --> WaitingForNTP
    WaitingForNTP --> Stable: First valid time appears immediately
    Stable --> Stable: Identical target causes no invalidation
    Stable --> Stable: Minute digit changes immediately
    Stable --> FoldUpper: Hour digit changes
    FoldUpper --> UnfoldLower: 175 milliseconds
    UnfoldLower --> Stable: 350 milliseconds total
```

Brightness 0 and 100 use constant off/on output. For intermediate percentages,
a timer only notifies a dedicated task. That task writes the CH422G backlight
switch over I2C and arms the next phase. The UI colon has no blink timer.
