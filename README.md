# NMS Words

ESP32-C3 + 1.28″ round GC9A01 TFT + NeoPixel ring.

## What is new

1. **Full UTF-8** — Czech etc. via `u8g2_font_unifont_t_extended`; main word via **helvB18/B24**; Autophage/Greek letters via **`u8g2_font_10x20_t_greek`** (`NMS_MAIN_WORD_USE_GREEK_FONT 0`, default).
2. **Selectable dictionaries** — `NMS_DICTIONARIES` in `nms_config.h`, or empty = every file in GitHub `index.txt`.
3. **Per-dictionary UI** — phrases, colours, layout, and label in each `.txt` as `# @…` (see [dictionaries/README.md](dictionaries/README.md)).

## Hardware & libraries

<img width="520" height="576" alt="Screenshot 2026-05-22 at 19 00 01" src="https://github.com/user-attachments/assets/e919cab5-1ab1-4234-8c9f-18e6ab183ba6" />



Wiring: `TFT_eSPI/User_Setup.h`.

 Libraries: **TFT_eSPI**, **Adafruit NeoPixel**, **U8g2**, **U8g2_for_TFT_eSPI**. Copy `User_Setup.h` into Arduino `libraries/TFT_eSPI/`.

## Sketch

Open **`NMS_Words2/NMS_Words2.ino`**. Copy `nms_config.example.h` → `nms_config.h`.

## Configuration (`nms_config.h`)

| Define | Meaning |
|--------|---------|
| `NMS_WIFI_SSID` / `NMS_WIFI_PASSWORD` | Wi-Fi |
| `NMS_GITHUB_WORDS_BASE_URL` | Folder with `index.txt` + `*.txt` (RAW or `github.com/.../blob/...`, auto-converted) |
| `NMS_DICTIONARIES` | Allowlist, e.g. `"gek,capitals_cs"`; `""` = all from `index.txt` (max **8** names) |
| `NMS_WORD_RANDOM_MIN_SEC` / `MAX` | Delay until next word |
| `NMS_BACKGROUND_GAP_SEC` | Backdrop-only seconds before fetch; `0` = off |
| `NMS_WORD_DISPLAY_MODE` | Default layout if no `# @display` (`0` / `1` / `2`) |
| `NMS_SHOW_CLOCK` / `NMS_CLOCK_24H` | Clock at bottom; `1` = 24h, `0` = 12h AM/PM |
| `NMS_SHOW_DICT_NAME` / `NMS_DICT_NAME_BASELINE_Y` | `# @name` at top; vertical position (px) |
| `NMS_MAIN_WORD_FONT_SIZE` | `0` / `1` / `2` — main word size |
| `NMS_PHRASE_FONT_SIZE` | `0` = small phrase font; `1` = same as main word |
| `NMS_MAIN_WORD_USE_GREEK_FONT` | `0` = large Greek font; `1` = small legacy unifont |
| `NMS_ACCENT_BORDER_PX` | Accent ring; `0` = off |
| `LED_PIN` / `LED_COUNT` | NeoPixel |

### On-screen layout (mode 1, default)

Top → bottom: optional **dictionary name** · **large word** (column 2) · **phrase1 / phrase2** · **reference** (column 1) · optional **clock**.

```c
#define NMS_DICTIONARIES "capitals_cs,elements_cs"
#define NMS_WORD_DISPLAY_MODE 1
#define NMS_SHOW_DICT_NAME 1
#define NMS_DICT_NAME_BASELINE_Y 29
```

### Dictionary metadata (in each `.txt`)

```text
# @name: Hlavní města
# @phrase1: Hlavní město země
# @phrase2:
# @quote: 0
# @accent: #46BEE6
# @accent_ref: #9AD4EE
země	Brusel
```

See [dictionaries/README.md](dictionaries/README.md) for all `@` keys — edit them directly in each `.txt` header.

### Topic dictionaries

| Files | Content |
|-------|---------|
| `capitals_cs.txt` / `capitals_en.txt` | country · capital |
| `elements_cs.txt` / `elements_en.txt` | name · symbol (118 elements) |

## Boot screen

Setup shows centered UTF-8: `starting...`, `WiFi OK` / `WiFi failed`, `GitHub OK` / `GitHub failed`.

## vs NMS Words

| | NMS Words | NMS Words 2 |
|---|-----------|---------------|
| Text | GLCD + Latin fold | U8g2 UTF-8 |
| Dictionaries | `index.txt` only | Allowlist + per-file `# @…` |
| Phrases / colours | In firmware | In each `.txt` |

Original: `../NMS_Words/`.
