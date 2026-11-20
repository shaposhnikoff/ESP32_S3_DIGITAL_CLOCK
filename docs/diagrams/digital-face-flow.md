# Digital clock face

```mermaid
flowchart TD
    Boot[Start display and NTP worker] --> Waiting[Four dashes and empty second scale]
    Waiting --> Valid{NTP time available}
    Valid -->|No| Waiting
    Valid -->|Yes| Local[Convert UTC using configured timezone]
    Local --> Minutes[Replace changed minute glyphs immediately]
    Minutes --> Digits{Hour digit changed}
    Digits -->|Yes| Flip[Animate changed glyph over 350 milliseconds]
    Digits -->|No| Seconds
    Flip --> Seconds[Display seconds and color exactly that many ticks]
    Seconds --> Rollover{Second equals zero}
    Rollover -->|Yes| Clear[Clear all 60 ticks]
    Rollover -->|No| Keep[Keep completed ticks orange]
    Clear --> Poll[Poll clock again]
    Keep --> Poll
    Poll --> Local
    Colon[Two permanently orange rectangles] --> Display[Visible throughout all updates]
```

The date changes at local midnight. Identical clock values cause no redraw.
The native preview uses deterministic weather fixtures; it is not a photograph
of the device and does not measure its backlight brightness.
