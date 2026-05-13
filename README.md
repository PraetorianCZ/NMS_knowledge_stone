
# NMS Words (ESP32-C3 mini + 1.28" TFT display with GC9A01 + Neopixel LEDs)

Lolin C3 Super Mini + 1.28″ round TFT — shows a random **English / alien** word pair on a schedule.

**Timing:** After each word, the next one appears after a **uniform random delay** between **`NMS_WORD_RANDOM_MIN_SEC`** and **`NMS_WORD_RANDOM_MAX_SEC`**. In the last **`NMS_BACKGROUND_GAP_SEC`** seconds before each GitHub fetch, only the **backdrop** is shown (no text); NeoPixel colour stays the **previous word’s** accent until the new word is drawn.

**Word source:** GitHub **RAW** only. The sketch downloads `index.txt` from `NMS_GITHUB_WORDS_BASE_URL`, picks a random listed `.txt` file, then a random tab-separated line (`english<TAB>alien`).

Use a **RAW** URL (`https://raw.githubusercontent.com/user/repo/branch/folder`). Browser `github.com/.../blob/...` links return HTML; the firmware can rewrite blob/tree URLs to RAW, but setting RAW directly is clearest.

## Hardware

- **MCU:** ESP32-C3 (e.g. Lolin C3 Super Mini)
- **Display:** GC9A01 (SPI)
- **Library:** TFT_eSPI (Bodmer)

Configure SPI pins in TFT_eSPI (`User_Setup.h` or `User_Setup_Select.h`):

<img width="896" height="954" alt="knowledgeStone_schema" src="https://github.com/user-attachments/assets/08a275e8-c83d-4395-8ae4-674ef2016287" />

- `#define GC9A01_DRIVER`
- Correct `TFT_MOSI`, `TFT_SCLK`, `TFT_CS`, `TFT_DC`, `TFT_RST`, optional `TFT_BL`

## Software

Install **TFT_eSPI** and **Adafruit NeoPixel** in Arduino IDE or PlatformIO.

Sketch: **`NMS_Words.ino`** (Arduino sketch folder must match the `.ino` name).

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
