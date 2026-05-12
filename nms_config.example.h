// Copy to nms_config.h and fill in your values (nms_config.h can stay out of git — see repo .gitignore).

#ifndef NMS_CONFIG_H
#define NMS_CONFIG_H

#define NMS_WIFI_SSID "YOUR_WIFI_SSID"
#define NMS_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// GitHub RAW base URL for the folder that contains index.txt — no trailing slash
#define NMS_GITHUB_WORDS_BASE_URL "https://raw.githubusercontent.com/USER/REPO/main/path/to/words"


// New word will show in a random interval betweet min and max
#define NMS_WORD_RANDOM_MIN_SEC 60L
#define NMS_WORD_RANDOM_MAX_SEC 300L

// Show only background before new word
#define NMS_BACKGROUND_GAP_SEC 50L

#ifndef NMS_SHOW_CLOCK
#define NMS_SHOW_CLOCK 1
#endif

// TFT orientation: 0 = default, 1 = 90°, 2 = 180°, 3 = 270° (TFT_eSPI setRotation)
#ifndef NMS_TFT_ROTATION
#define NMS_TFT_ROTATION 0
#endif

#ifndef NMS_ACCENT_BORDER_PX
// Circular ring at display edge (accent colour); thickness in pixels, 0 = off
#define NMS_ACCENT_BORDER_PX 6
#endif

#define NMS_TZ_OFFSET_SECONDS (2L * 3600L)
#define NMS_NTP_SERVER_1 "pool.ntp.org"
#define NMS_NTP_SERVER_2 "time.nist.gov"

// Optional nms_bg565.h: swap RGB565 bytes for pushImage if colours are wrong
// #define NMS_BG565_PUSH_SWAP_BYTES 0

// Optional HTTPS response size cap is defined in NMS_Words.ino (NMS_HTTPS_MAX_BODY_BYTES)

// NeoPixel ring/strip (Adafruit_NeoPixel): pin, pixel count, optional brightness 0–255
// #define LED_COUNT 8
// #define LED_PIN 8
// #define NMS_LED_BRIGHTNESS 255

#endif
