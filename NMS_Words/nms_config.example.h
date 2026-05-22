// Live config (not in git). Keep in sync with nms_config.example.h — same sections and defines.

#ifndef NMS_CONFIG_H
#define NMS_CONFIG_H

// ---------------------------------------------------------------------------
// Wi-Fi (NTP + HTTPS word lists from GitHub RAW)
// ---------------------------------------------------------------------------
#define NMS_WIFI_SSID "YOUR_WIFI"
#define NMS_WIFI_PASSWORD "YOUR_WIFI_PASS"

// ---------------------------------------------------------------------------
// GitHub — folder with index.txt (RAW URL, or github.com/.../blob/... — converted automatically)
// ---------------------------------------------------------------------------
#define NMS_GITHUB_WORDS_BASE_URL "https://github.com/PraetorianCZ/NMS_knowledge_stone/blob/main/dictionaries"

// ---------------------------------------------------------------------------
// Dictionaries — comma-separated names without .txt (max 8). Empty = all from index.txt
// ---------------------------------------------------------------------------
#define NMS_DICTIONARIES ""

// ---------------------------------------------------------------------------
// Word timing (uniform random delay until next fetch)
// ---------------------------------------------------------------------------
#define NMS_WORD_RANDOM_MIN_SEC 5L
#define NMS_WORD_RANDOM_MAX_SEC 15L

// Backdrop-only seconds before each fetch; 0 = off
#define NMS_BACKGROUND_GAP_SEC 0L

// ---------------------------------------------------------------------------
// Layout — 0 = main word only | 1 = main word + phrases + reference | 2 = main word + reference
// ---------------------------------------------------------------------------
#define NMS_WORD_DISPLAY_MODE 1

// ---------------------------------------------------------------------------
// TFT / UI
// ---------------------------------------------------------------------------
#define NMS_SHOW_CLOCK 1             // small clock at bottom
#define NMS_CLOCK_24H 0              // 1 = 24h (14:05) | 0 = 12h (2:05 PM)
#define NMS_SHOW_DICT_NAME 1         // display @name from .txt at top when set; 0 = never
#define NMS_DICT_NAME_BASELINE_Y 29  // @name baseline from top of display (px)
#define NMS_TFT_ROTATION 0           // display rotation if needed (0, 1, 2, 3)
#define NMS_ACCENT_BORDER_PX 6       // thickness of coloured frame (0 = none)

// ---------------------------------------------------------------------------
// Time — fixed UTC offset (Prague +2 h = 7200)
// ---------------------------------------------------------------------------
#define NMS_TZ_OFFSET_SECONDS (2L * 3600L)
#define NMS_NTP_SERVER_1 "pool.ntp.org"
#define NMS_NTP_SERVER_2 "time.nist.gov"

// ---------------------------------------------------------------------------
// NeoPixel — accent colour matches main word (column 2) on TFT
// ---------------------------------------------------------------------------
#define LED_COUNT 8
#define LED_PIN 8
#define NMS_LED_BRIGHTNESS 255

// ---------------------------------------------------------------------------
// TFT fonts
// ---------------------------------------------------------------------------
#define NMS_MAIN_WORD_FONT_SIZE 2
#define NMS_MAIN_WORD_Y_OFFSET 40
#define NMS_PHRASE_FONT_SIZE 0
#define NMS_MAIN_WORD_USE_GREEK_FONT 0

// #define NMS_FONT_TITLE u8g2_font_helvB24_te
// #define NMS_FONT_BODY u8g2_font_unifont_t_extended
// #define NMS_FONT_GREEK u8g2_font_unifont_t_greek
// #define NMS_BG565_PUSH_SWAP_BYTES 0

#endif
