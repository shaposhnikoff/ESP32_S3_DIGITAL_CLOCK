#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p .pio/font-tool src/fonts
npm pack @fontsource/roboto-condensed@5.2.5 --pack-destination .pio/font-tool --silent
tar -xzf .pio/font-tool/fontsource-roboto-condensed-5.2.5.tgz -C .pio/font-tool
npx --yes lv_font_conv@1.5.3 --size 256 --bpp 4 --format lvgl --no-compress --no-kerning \
  --font .pio/font-tool/package/files/roboto-condensed-latin-900-normal.woff \
  --symbols '0123456789-' --output src/fonts/clock_digits_256.c
npx --yes lv_font_conv@1.5.3 --size 32 --bpp 4 --format lvgl --no-compress --no-kerning \
  --font .pio/font-tool/package/files/roboto-condensed-latin-700-normal.woff --range 0x20-0x5A,0xB0 \
  --font .pio/font-tool/package/files/roboto-condensed-cyrillic-700-normal.woff --range 0x0400-0x045F \
  --output src/fonts/clock_date_32.c
cp .pio/font-tool/package/LICENSE src/fonts/OFL.txt
npm pack @fontsource/barlow-condensed@5.2.5 --pack-destination .pio/font-tool --silent
mkdir -p .pio/font-tool/barlow
tar -xzf .pio/font-tool/fontsource-barlow-condensed-5.2.5.tgz -C .pio/font-tool/barlow
npx --yes lv_font_conv@1.5.3 --size 448 --bpp 4 --format lvgl --no-compress --no-kerning \
  --font .pio/font-tool/barlow/package/files/barlow-condensed-latin-300-normal.woff \
  --symbols '0123456789-' --output src/fonts/clock_digits_448.c
# Preserve the bundled upstream Barlow OFL notice; the package's generic
# LICENSE omits the Barlow authorship notice.
node -e '
const fs = require("node:fs");
for (const path of process.argv.slice(1)) {
  fs.writeFileSync(path, fs.readFileSync(path, "utf8").trimEnd() + "\n");
}
' src/fonts/clock_*.c
