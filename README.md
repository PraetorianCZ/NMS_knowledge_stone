# NMS Words (ESP32-C3 + GC9A01)

Lolin C3 Super Mini + 1.28″ round TFT — shows a random **English / alien** word pair on a schedule.

**Timing:** After each word, the next one appears after a **uniform random delay** between **`NMS_WORD_RANDOM_MIN_SEC`** and **`NMS_WORD_RANDOM_MAX_SEC`**. In the last **`NMS_BACKGROUND_GAP_SEC`** seconds before each GitHub fetch, only the **backdrop** is shown (no text); NeoPixel colour stays the **previous word’s** accent until the new word is drawn. While HTTPS is loading, the screen also shows **backdrop only** (no “…”).

**Word source:** GitHub **RAW** only. The sketch downloads `index.txt` from `NMS_GITHUB_WORDS_BASE_URL`, picks a random listed `.txt` file, then a random tab-separated line (`english<TAB>alien`).

Use a **RAW** URL (`https://raw.githubusercontent.com/user/repo/branch/folder`). Browser `github.com/.../blob/...` links return HTML; the firmware can rewrite blob/tree URLs to RAW, but setting RAW directly is clearest.

## Hardware

- **MCU:** ESP32-C3 (e.g. Lolin C3 Super Mini)
- **Display:** GC9A01 (SPI)
- **Library:** TFT_eSPI (Bodmer)

Configure SPI pins in TFT_eSPI (`User_Setup.h` or `User_Setup_Select.h`):

- `#define GC9A01_DRIVER`
- Correct `TFT_MOSI`, `TFT_SCLK`, `TFT_CS`, `TFT_DC`, `TFT_RST`, optional `TFT_BL`

## Software

Install **TFT_eSPI** and **Adafruit NeoPixel** in Arduino IDE or PlatformIO.

Sketch: **`nms_hourly_words.ino`**.

NeoPixel colour tracks the **race accent** (same RGB as the alien word): set **`LED_PIN`**, **`LED_COUNT`**, and optionally **`NMS_LED_BRIGHTNESS`** (`0`–`255`) in `nms_config.h`.

## Configuration (`nms_config.h`)

Copy from `nms_config.example.h` if needed (keep secrets out of git — see repo `.gitignore`).

| Define | Meaning |
|--------|---------|
| `NMS_WIFI_SSID` / `NMS_WIFI_PASSWORD` | Wi-Fi credentials |
| **`NMS_GITHUB_WORDS_BASE_URL`** | RAW URL of folder containing **`index.txt`** (no trailing `/`) |
| **`NMS_WORD_RANDOM_MIN_SEC`** / **`NMS_WORD_RANDOM_MAX_SEC`** | Next word appears after a random delay in this inclusive range (seconds) |
| **`NMS_BACKGROUND_GAP_SEC`** | Before each fetch, show **backdrop only** for this many seconds; `0` = off. If larger than the chosen random period, it is clamped |
| **`NMS_SHOW_CLOCK`** | `1` = show `HH:MM` at bottom of TFT; `0` = hide (NTP still used for timing) |
| **`NMS_TFT_ROTATION`** | `0`–`3` — TFT_eSPI rotation (`setRotation`). **`2`** = 180° flip |
| **`NMS_ACCENT_BORDER_PX`** | Thickness (px) of a **circular ring** at the display edge in the race accent colour (matches alien word / NeoPixel). **`0`** disables |
| **`NMS_TZ_OFFSET_SECONDS`** | Fixed offset from UTC for `localtime` (e.g. Prague +2 h → `7200`) |

Timezone is a fixed offset; correct DST would need a TZ string in `configTime`.

### Optional background bitmap

Add **`nms_bg565.h`** (RGB565, same resolution as the panel). If colours are wrong after `pushImage`, toggle **`NMS_BG565_PUSH_SWAP_BYTES`** in config.

### Accented Latin on screen

Built-in GLCD fonts do not include Czech (or other) accented glyphs. **`nms_tft_latin_fold.h`** folds UTF-8 Latin to ASCII for display. True accented rendering needs a smooth font (`.vlw`) and LittleFS — not included here.

## Related repo tools (optional)

To **generate offline word `.txt` files** from the Fandom wiki (for publishing to your GitHub repo), use scripts under `nms_languages/` — see `fetch_and_build.py` and `tools/build_embedded_words.py`. The ESP sketch **no longer** embeds those lists in flash; it only reads from GitHub RAW as described above.

## Security (short)

- Keep **`nms_config.h`** out of public git (Wi-Fi password). Serial log prints SSID and connection status — avoid sharing raw logs if that matters.
- HTTPS uses **`setInsecure()`** (no CA verification). Use **`setCACert()`** if you need stronger TLS on untrusted networks.
- GitHub mode only accepts a **`raw.githubusercontent.com`** base and safe relative paths from `index.txt` (no `..`). Response size is capped (`NMS_HTTPS_MAX_BODY_BYTES` in the `.ino`).

## Git and GitHub

The repo **`.gitignore`** excludes `nms_config.h`. After clone:

```bash
cp nms_hourly_words/nms_config.example.h nms_hourly_words/nms_config.h
```

Fill in Wi-Fi and `NMS_GITHUB_WORDS_BASE_URL`.

### First push (empty GitHub repo)

1. Create an empty repo at [github.com/new](https://github.com/new).
2. From the project root:

```bash
git init
git add .
git status   # confirm nms_config.h is not staged if ignored
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/YOUR_USER/YOUR_REPO.git
git push -u origin main
```

Use a **Personal Access Token** for HTTPS if prompted.

### Stop tracking `nms_config.h` if it was committed once

```bash
git rm --cached nms_hourly_words/nms_config.h
git commit -m "Stop tracking nms_config.h"
git push
```
# NMS_knowledge_stone
# NMS_knowledge_stone
# NMS_knowledge_stone
# NMS_knowledge_stone
