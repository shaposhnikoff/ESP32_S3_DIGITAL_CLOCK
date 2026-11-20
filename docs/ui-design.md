# Digital face

The fixed 1024 x 600 layout follows the first supplied Clock Faces-selection.png
reference. There are no cards, buttons, touch controls or permanent hinge lines.
Background: #0B0B0C. Digits and weekday: #ECE8E0. Accent: #FF6A2E.
Secondary text: #7C7A76. Inactive marks: #29292B.

Four proportional glyph canvases show HH:MM. Hours are right-aligned before
the colon; minutes start at x=613. The fixed colon comprises 29 x 40 rectangles
at x=558, y=242 and y=371. No timer toggles it. The top row contains localized
weekday, abbreviated date, weather and actual Wi-Fi signal strength.

Sixty bottom marks start at x=56 with 864/60-pixel spacing and end at y=548.
Every fifth mark is 20 pixels tall; others are 11 pixels tall. At second N,
exactly N marks are orange. At 00 all marks become inactive again. The second
counter starts at x=945. Second-only updates do not animate the large digits.

Minute glyphs switch immediately without animation. Changed hour glyphs
retain the 350 ms two-stage transition: the old upper
half folds over 175 ms and the new lower half unfolds over 175 ms. Cached
RGB565 glyphs use one output canvas per digit. Proportional widths follow
the target font; the previous glyph is resampled into the target width during
a transition. The settled image has no seam. The first valid time appears
immediately. Before NTP, the large digits and second counter show dashes.

Digits use Barlow Condensed Light 300 at size 448, pinned to
@fontsource/barlow-condensed@5.2.5. Labels use Roboto Condensed Bold 700 at
size 32, pinned to @fontsource/roboto-condensed@5.2.5. The degree sign is
explicitly included. LV_FONT_FMT_TXT_LARGE=1 is shared by MCU and native builds.
The historical 256-size asset remains in the tree but is unused by this face.

Regenerate with sh tools/generate_fonts.sh, using lv_font_conv@1.5.3. Both SIL
OFL licenses accompany the fonts in src/fonts/. Firmware builds consume the
bundled C assets without npm downloads. Dates support Russian, Ukrainian and
English; weather descriptions use English. Twelve-hour mode shows 01..12.

Native captures use deterministic fixture data, not device photos. Physical
panel behavior, backlight brightness and animation smoothness require the board.
