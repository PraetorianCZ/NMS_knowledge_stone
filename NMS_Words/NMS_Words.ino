// NMS_Words2 — ESP32-C3 + round TFT (GC9A01). UTF-8 display + selectable dictionaries.
#include "nms_config.h"

#ifndef NMS_SHOW_CLOCK
#define NMS_SHOW_CLOCK 1
#endif

// 1 = 24-hour clock (14:05) | 0 = 12-hour (2:05 PM)
#ifndef NMS_CLOCK_24H
#define NMS_CLOCK_24H 1
#endif

// Optional nms_bg565.h background: if colours look wrong after pushImage(), toggle NMS_BG565_PUSH_SWAP_BYTES in nms_config.h
#ifndef NMS_BG565_PUSH_SWAP_BYTES
#define NMS_BG565_PUSH_SWAP_BYTES 1
#endif

// TFT_eSPI: 0 = 0°, 1 = 90°, 2 = 180°, 3 = 270° (clockwise)
#ifndef NMS_TFT_ROTATION
#define NMS_TFT_ROTATION 0
#endif

// Accent ring at panel edge (same RGB565 as main word / NeoPixel). 0 = off.
#ifndef NMS_ACCENT_BORDER_PX
#define NMS_ACCENT_BORDER_PX 6
#endif

// Display: 0 = main word only | 1 = main word + phrases + reference (default) | 2 = main word + reference only
#ifndef NMS_WORD_DISPLAY_MODE
#define NMS_WORD_DISPLAY_MODE 1
#endif

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ctype.h>
#include <cstring>
#include <math.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>
#include "nms_tft_utf8.h"
#include "nms_dictionaries.h"
#include "nms_dict_meta.h"

#if __has_include("nms_bg565.h")
#include "nms_bg565.h"
#define NMS_HAS_BG565 1
#endif

#ifndef LED_COUNT
#error "Define LED_COUNT and LED_PIN in nms_config.h for Adafruit NeoPixel"
#endif
#ifndef LED_PIN
#error "Define LED_COUNT and LED_PIN in nms_config.h for Adafruit NeoPixel"
#endif
#ifndef NMS_LED_BRIGHTNESS
#define NMS_LED_BRIGHTNESS 255
#endif

static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/** TFT_eSPI RGB565 -> 8-bit RGB for NeoPixel (matches on-screen accent colour). */
static void nmsNeoPixelSetAccent(uint16_t tftRgb565) {
  const uint8_t r5 = (tftRgb565 >> 11) & 0x1F;
  const uint8_t g6 = (tftRgb565 >> 5) & 0x3F;
  const uint8_t b5 = tftRgb565 & 0x1F;
  const uint8_t r = (r5 << 3) | (r5 >> 2);
  const uint8_t g = (g6 << 2) | (g6 >> 4);
  const uint8_t b = (b5 << 3) | (b5 >> 2);
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
}

// Boot status: centered lines on the round TFT during setup only (no Serial).
static void nmsStatusLogImpl(const char* level, const String& msg);
static void logLine(const char* level, const String& msg) { nmsStatusLogImpl(level, msg); }
static void logInfo(const String& msg) { logLine("INFO", msg); }
static void logWarn(const String& msg) { logLine("WARN", msg); }

// TLS does not verify server certificates (common on ESP32). Use setCACert() if you need MITM protection.
#ifndef NMS_HTTPS_MAX_BODY_BYTES
#define NMS_HTTPS_MAX_BODY_BYTES 98304
#endif

static bool nmsHttpsGetBodyUa(const String& url, String& bodyOut, const char* userAgent) {
  bodyOut = "";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000);
  if (!http.begin(client, url)) {
    return false;
  }
  if (userAgent != nullptr && userAgent[0] != '\0') {
    http.setUserAgent(userAgent);
  }
  const int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  const int len = http.getSize();
  if (len > 0 && len > (int)NMS_HTTPS_MAX_BODY_BYTES) {
    http.end();
    return false;
  }
  bodyOut = http.getString();
  http.end();
  return true;
}

// ----- GitHub RAW word lists (index.txt → random .txt → random TAB-separated line) -----

#ifndef NMS_GITHUB_WORDS_BASE_URL
#error "Define NMS_GITHUB_WORDS_BASE_URL in nms_config.h (RAW base URL without trailing slash). See README."
#endif

/** Content must be fetched from raw.githubusercontent.com. Browser github.com/blob|tree URLs are rewritten to RAW. */
static String nmsGithubWordsBaseRaw(const String& configured) {
  String s = configured;
  while (s.length() > 0 && s.endsWith("/")) {
    s.remove(s.length() - 1);
  }
  if (s.indexOf("raw.githubusercontent.com") >= 0) {
    return s;
  }

  int path0 = -1;
  {
    const char* m = "www.github.com/";
    const int p = s.indexOf(m);
    if (p >= 0) {
      path0 = p + 15;
    }
  }
  if (path0 < 0) {
    const char* m = "github.com/";
    const int p = s.indexOf(m);
    if (p >= 0) {
      path0 = p + 11;
    }
  }
  if (path0 < 0 || path0 >= (int)s.length()) {
    return s;
  }

  const String tail = s.substring(path0);
  const int u1 = tail.indexOf('/');
  if (u1 <= 0) {
    return s;
  }
  const int u2 = tail.indexOf('/', u1 + 1);
  if (u2 <= u1) {
    return s;
  }
  const int u3 = tail.indexOf('/', u2 + 1);
  if (u3 <= u2) {
    return s;
  }

  const String kind = tail.substring(u2 + 1, u3);
  if (kind != "blob" && kind != "tree") {
    return s;
  }

  const int u4 = tail.indexOf('/', u3 + 1);
  const String user = tail.substring(0, u1);
  const String repo = tail.substring(u1 + 1, u2);
  String branch;
  String pathRest;
  if (u4 < 0) {
    branch = tail.substring(u3 + 1);
  } else {
    branch = tail.substring(u3 + 1, u4);
    pathRest = tail.substring(u4 + 1);
  }

  if (user.length() == 0 || repo.length() == 0 || branch.length() == 0) {
    return s;
  }

  String out = String("https://raw.githubusercontent.com/") + user + "/" + repo + "/" + branch;
  if (pathRest.length() > 0) {
    out += "/" + pathRest;
  }
  return out;
}

static bool nmsHttpsGetBody(const String& url, String& bodyOut) {
  return nmsHttpsGetBodyUa(url, bodyOut, "ESP32C3-NMS-Words/1.0");
}

static bool nmsGithubRawBaseOk(const String& raw) {
  static const char kPrefix[] = "https://raw.githubusercontent.com/";
  if (!raw.startsWith(kPrefix)) {
    return false;
  }
  if (raw.indexOf('@') >= 0 || raw.indexOf('#') >= 0) {
    return false;
  }
  return true;
}

static bool nmsGithubIndexPathCharsOk(const String& t) {
  if (t.length() > 120) {
    return false;
  }
  for (size_t i = 0; i < t.length(); i++) {
    const char c = t.charAt((int)i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' ||
        c == '/') {
      continue;
    }
    return false;
  }
  return true;
}

static String nmsUrlJoin(const String& base, const String& rel) {
  String b = base;
  while (b.length() > 0 && b.endsWith("/")) {
    b.remove(b.length() - 1);
  }
  String r = rel;
  r.trim();
  while (r.length() > 0 && r.startsWith("/")) {
    r.remove(0, 1);
  }
  return b + "/" + r;
}

static String nmsGithubIndexLineClean(const String& line) {
  String t = line;
  t.trim();
  if (t.length() == 0 || t.charAt(0) == '#') {
    return t;
  }
  int cut = t.indexOf(" //");
  if (cut > 0) {
    t = t.substring(0, cut);
  }
  cut = t.indexOf(" #");
  if (cut > 0) {
    t = t.substring(0, cut);
  }
  t.trim();
  return t;
}

static bool nmsGithubLineOk(const String& line) {
  String t = nmsGithubIndexLineClean(line);
  if (t.length() == 0 || t.charAt(0) == '#') return false;
  if (t.indexOf("..") >= 0) return false;
  if (t.indexOf('<') >= 0 || t.indexOf('>') >= 0) {
    return false;
  }
  if (t.startsWith("/")) {
    return false;
  }
  String low = t;
  low.toLowerCase();
  if (!low.endsWith(".txt")) {
    return false;
  }
  if (!nmsGithubIndexPathCharsOk(t)) {
    return false;
  }
  return true;
}

static bool nmsPickRandomIndexPath(const String& indexBody, String& pathOut) {
  int n = 0;
  int start = 0;
  while (start < (int)indexBody.length()) {
    int nl = indexBody.indexOf('\n', start);
    if (nl < 0) nl = indexBody.length();
    const String line = indexBody.substring(start, nl);
    if (nmsGithubLineOk(line) && nmsDictionaryPathAllowed(nmsGithubIndexLineClean(line))) {
      n++;
    }
    start = nl + 1;
  }
  if (n <= 0) return false;
  const int pick = (int)random(n);
  int i = 0;
  start = 0;
  while (start < (int)indexBody.length()) {
    int nl = indexBody.indexOf('\n', start);
    if (nl < 0) nl = indexBody.length();
    const String line = indexBody.substring(start, nl);
    if (nmsGithubLineOk(line) && nmsDictionaryPathAllowed(nmsGithubIndexLineClean(line))) {
      if (i == pick) {
        pathOut = nmsGithubIndexLineClean(line);
        return true;
      }
      i++;
    }
    start = nl + 1;
  }
  return false;
}

static bool nmsDictParseTabLine(const String& rawLine, String& col1, String& col2) {
  String line = nmsDictPrepareDataLine(rawLine);
  if (line.length() == 0 || line.charAt(0) == '#') {
    return false;
  }
  const int tab = line.indexOf('\t');
  if (tab <= 0 || tab >= (int)line.length() - 1) {
    return false;
  }
  col1 = line.substring(0, tab);
  col2 = line.substring(tab + 1);
  col1.trim();
  col2.trim();
  return col1.length() > 0 && col2.length() > 0;
}

static bool nmsPickRandomTabLine(const String& fileBody, String& col1, String& col2) {
  int n = 0;
  int start = 0;
  while (start < (int)fileBody.length()) {
    int nl = fileBody.indexOf('\n', start);
    if (nl < 0) nl = fileBody.length();
    String c1, c2;
    if (nmsDictParseTabLine(fileBody.substring(start, nl), c1, c2)) {
      n++;
    }
    start = nl + 1;
  }
  if (n <= 0) return false;
  const int pick = (int)random(n);
  int i = 0;
  start = 0;
  while (start < (int)fileBody.length()) {
    int nl = fileBody.indexOf('\n', start);
    if (nl < 0) nl = fileBody.length();
    if (nmsDictParseTabLine(fileBody.substring(start, nl), col1, col2)) {
      if (i == pick) {
        return true;
      }
      i++;
    }
    start = nl + 1;
  }
  return false;
}

static bool nmsFetchRandomWordFromGithub(String& englishOut, String& alienOut) {
  englishOut = "";
  alienOut = "";

  const String baseRaw = nmsGithubWordsBaseRaw(String(NMS_GITHUB_WORDS_BASE_URL));
  if (!nmsGithubRawBaseOk(baseRaw)) {
    return false;
  }

  String relPath;
  if (nmsDictionariesUseAllowlist()) {
    if (!nmsPickRandomAllowedDictionary(relPath)) {
      return false;
    }
  } else {
    const String indexUrl = nmsUrlJoin(baseRaw, "index.txt");
    String indexBody;
    if (!nmsHttpsGetBody(indexUrl, indexBody)) {
      return false;
    }
    if (!nmsPickRandomIndexPath(indexBody, relPath)) {
      return false;
    }
  }
  const String fileUrl = nmsUrlJoin(baseRaw, relPath);
  String fileBody;
  if (!nmsHttpsGetBody(fileUrl, fileBody)) {
    return false;
  }
  nmsDictMetaParseFromFileBody(fileBody);
  return nmsPickRandomTabLine(fileBody, englishOut, alienOut);
}

// ----- Display -----
TFT_eSPI tft = TFT_eSPI();
U8g2_for_TFT_eSPI u8g2 = U8g2_for_TFT_eSPI();

// Vertical layout for showWord() (U8g2 baselines)
#ifndef NMS_MAIN_WORD_Y_OFFSET
#define NMS_MAIN_WORD_Y_OFFSET 20
#endif

// Layout (top → bottom): optional @name | main word (col 2) | phrase lines | reference (col 1) | optional clock

static const int NMS_ALIEN_TOP_BASELINE = 72 + NMS_MAIN_WORD_Y_OFFSET;
static const int NMS_GAP_ALIEN_TO_SENTENCE = 10;
static const int NMS_SENTENCE_LINE_EXTRA = 4;

static bool sTftBootLogReady = false;
static bool sSetupPhaseUi = true;

#ifndef NMS_BOOT_STATUS_MAX_LINES
#define NMS_BOOT_STATUS_MAX_LINES 6
#endif
static String sBootText[NMS_BOOT_STATUS_MAX_LINES];
static uint16_t sBootCol[NMS_BOOT_STATUS_MAX_LINES];
static uint8_t sBootCount = 0;

static void nmsBootStatusReset(void) {
  sBootCount = 0;
}

/** Append a boot status line and redraw all lines centered as a vertical block. */
static void nmsStatusLogImpl(const char* level, const String& msg) {
  if (!sTftBootLogReady || !sSetupPhaseUi) {
    return;
  }
  uint16_t fg = TFT_CYAN;
  if (strcmp(level, "WARN") == 0) {
    fg = TFT_YELLOW;
  }

  String line = msg;
  nmsTftSetBodyFont();
  const int maxW = tft.width() - 16;
  while (line.length() > 0 && nmsTftTextWidth(line) > maxW) {
    nmsUtf8PopLastCodepoint(line);
  }
  if (sBootCount < NMS_BOOT_STATUS_MAX_LINES) {
    sBootText[sBootCount] = line;
    sBootCol[sBootCount] = fg;
    sBootCount++;
  }

  tft.fillScreen(TFT_BLACK);
  nmsTftSetBodyFont();
  const int cx = tft.width() / 2;
  const int lh = nmsTftLineHeight();
  const int n = (int)sBootCount;
  const int totalH = n * lh;
  const int blockTop = (tft.height() - totalH) / 2;
  for (int i = 0; i < n; i++) {
    const int lineCy = blockTop + i * lh + lh / 2;
    nmsTftDrawUtf8Centered(cx, nmsTftBaselineForCenterY(lineCy), sBootText[(uint8_t)i], sBootCol[(uint8_t)i]);
  }
}

static void drawBackdropNoBitmap(void) {
  tft.fillScreen(TFT_BLACK);
  tft.fillCircle(tft.width() / 2, tft.height() / 2, tft.width() / 2 + 4, tft.color565(4, 4, 10));
}

static void drawWordBackdrop(void) {
#if defined(NMS_HAS_BG565)
  if (NMS_BG565_WIDTH == tft.width() && NMS_BG565_HEIGHT == tft.height()) {
    const bool prevSwap = tft.getSwapBytes();
    tft.setSwapBytes(NMS_BG565_PUSH_SWAP_BYTES != 0);
    tft.pushImage(0, 0, NMS_BG565_WIDTH, NMS_BG565_HEIGHT, nms_bg565);
    tft.setSwapBytes(prevSwap);
  } else {
    drawBackdropNoBitmap();
  }
#else
  drawBackdropNoBitmap();
#endif
}

static uint16_t sLastAccent565 = TFT_WHITE;
/** After blank-gap backdrop was drawn once; reset when next cycle is scheduled. */
static bool sBlankGapPainted = false;

/** Circular ring (NMS_ACCENT_BORDER_PX thick) at panel edge — same RGB565 as word / NeoPixel accent. */
static void nmsDrawAccentBorder(uint16_t color565) {
#if NMS_ACCENT_BORDER_PX > 0
  const int w = tft.width();
  const int h = tft.height();
  const int cx = w / 2;
  const int cy = h / 2;
  const int t = NMS_ACCENT_BORDER_PX;
  const int Ro = (w <= h ? w : h) / 2;
  const int Ri = Ro - t;
  if (Ri < 1 || Ro < 2) {
    return;
  }

  const int64_t Ro2 = (int64_t)Ro * Ro;
  const int64_t Ri2 = (int64_t)Ri * Ri;

  for (int y = 0; y < h; y++) {
    const int dy = y - cy;
    const int64_t dy2 = (int64_t)dy * dy;
    if (dy2 > Ro2) {
      continue;
    }
    const float outerChord = sqrtf((float)((double)Ro2 - (double)dy2));
    const int dxo = (int)(outerChord + 0.5f);
    if (dy2 > Ri2) {
      tft.drawFastHLine(cx - dxo, y, 2 * dxo + 1, color565);
    } else {
      const float innerChord = sqrtf((float)((double)Ri2 - (double)dy2));
      const int dxi = (int)(innerChord + 0.5f);
      const int seg = dxo - dxi;
      if (seg > 0) {
        tft.drawFastHLine(cx - dxo, y, seg, color565);
        tft.drawFastHLine(cx + dxi + 1, y, seg, color565);
      }
    }
  }
#endif
}

/** Alien word only; returns baseline Y for the first English sentence line. */
static int drawWordAlienOnly(int cx, int alienTopBaseline, int maxW, const String& alienDraw, uint16_t accent, uint16_t alienGlow) {
  const int lh = nmsTftMainWordLineHeight(alienDraw);
  if (nmsTftTextWidth(alienDraw, true) <= maxW) {
    nmsTftDrawGlowCentered(cx, alienTopBaseline, alienDraw, accent, alienGlow, 2, true);
    nmsTftSetBodyFont();
    return alienTopBaseline + lh + NMS_GAP_ALIEN_TO_SENTENCE;
  }
  const int belowBaseline = nmsTftDrawWrappedCentered(alienDraw, alienTopBaseline, maxW, accent, true);
  nmsTftSetBodyFont();
  return belowBaseline + NMS_GAP_ALIEN_TO_SENTENCE;
}

/** Alien word only, vertically centered on the round panel. */
static void drawAlienOnlyCentered(int cx, int maxW, const String& alienDraw, uint16_t accent, uint16_t alienGlow) {
  const int lh = nmsTftMainWordLineHeight(alienDraw);
  if (nmsTftTextWidth(alienDraw, true) <= maxW) {
    const int cy = tft.height() / 2 + NMS_MAIN_WORD_Y_OFFSET;
    nmsTftDrawGlowCentered(cx, nmsTftBaselineForCenterY(cy), alienDraw, accent, alienGlow, 2, true);
    return;
  }
  const int nLines = nmsTftCountWrappedLines(alienDraw, maxW, true);
  const int totalH = nLines * lh;
  const int blockTop = (tft.height() - totalH) / 2 + NMS_MAIN_WORD_Y_OFFSET;
  nmsTftDrawWrappedCentered(alienDraw, blockTop, maxW, accent, true);
}

static void nmsFormatClockString(const struct tm& tmNow, char* buf, size_t bufLen) {
  if (bufLen == 0) {
    return;
  }
#if NMS_CLOCK_24H
  snprintf(buf, bufLen, "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
#else
  int h = tmNow.tm_hour % 12;
  if (h == 0) {
    h = 12;
  }
  const char* suffix = (tmNow.tm_hour < 12) ? "AM" : "PM";
  snprintf(buf, bufLen, "%d:%02d %s", h, tmNow.tm_min, suffix);
#endif
}

// col1 = reference (english param), col2 = large main word (alien param)
static void showWord(const String& english, const String& alien, bool drawBackdropFirst = true) {
  const int cx = tft.width() / 2;
  const int margin = 14;
  const int maxW = tft.width() - 2 * margin;
  const uint16_t accent = nmsDictMetaAccent();
  const uint16_t accentRef = nmsDictMetaAccentRef();
  const uint16_t alienGlow = tft.color565(28, 30, 42);
  const uint16_t refGlow = tft.color565(40, 42, 58);
  const uint16_t sentenceGlow = tft.color565(70, 70, 85);

  nmsNeoPixelSetAccent(accent);

  String alienDraw = alien;
  alienDraw.trim();
  String englishDraw = english;
  englishDraw.trim();

  if (drawBackdropFirst) {
    drawWordBackdrop();
  }

  const bool phraseLarge = nmsTftPhraseUsesTitleFont();
  const int lhPhrase = nmsTftLineHeightFor(phraseLarge);
  const uint8_t dispMode = nmsDictMetaDisplayMode();
  const bool quoteRef = nmsDictMetaQuoteRef();
  String refLine = englishDraw;
  if (quoteRef) {
    refLine = String("'") + englishDraw + "'";
  }

  if (dispMode == 0) {
    drawAlienOnlyCentered(cx, maxW, alienDraw, accent, alienGlow);
  } else if (dispMode == 2) {
    const int ySub = drawWordAlienOnly(cx, NMS_ALIEN_TOP_BASELINE, maxW, alienDraw, accent, alienGlow);
    nmsTftDrawGlowCentered(cx, ySub, refLine, accentRef, refGlow, 1, phraseLarge);
  } else {
    const String lineA = sNmsDictMeta.phrase1;
    const String lineB = sNmsDictMeta.phrase2;
    const int yAfterMain = drawWordAlienOnly(cx, NMS_ALIEN_TOP_BASELINE, maxW, alienDraw, accent, alienGlow);
    int yNext = yAfterMain;
    if (lineA.length() > 0) {
      nmsTftDrawGlowCentered(cx, yNext, lineA, TFT_WHITE, sentenceGlow, 1, phraseLarge);
      yNext += lhPhrase + NMS_SENTENCE_LINE_EXTRA;
    }
    if (lineB.length() > 0) {
      nmsTftDrawGlowCentered(cx, yNext, lineB, TFT_WHITE, sentenceGlow, 1, phraseLarge);
      yNext += lhPhrase + NMS_SENTENCE_LINE_EXTRA;
    }
    if (englishDraw.length() > 0) {
      nmsTftDrawGlowCentered(cx, yNext, refLine, accentRef, refGlow, 1, phraseLarge);
    }
  }

  nmsDictDrawNameHeader(cx);

#if NMS_SHOW_CLOCK
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[16];
  nmsFormatClockString(tmNow, buf, sizeof(buf));
  nmsTftDrawUtf8Centered(cx, tft.height() - 12, String(buf), TFT_DARKGREY);
#endif

  sLastAccent565 = accent;
  nmsDrawAccentBorder(accent);
}

static void ensureTimeSynced() {
  configTime(NMS_TZ_OFFSET_SECONDS, 0, NMS_NTP_SERVER_1, NMS_NTP_SERVER_2);
  time_t now = time(nullptr);
  int tries = 0;
  while (now < 1700000000 && tries < 40) {
    delay(250);
    now = time(nullptr);
    tries++;
  }
}

static void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    logInfo("Network operational");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(NMS_WIFI_SSID, NMS_WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    logInfo("Network operational");
  } else {
    logWarn("Network failed");
  }
}

void setup() {
  nmsDictionariesInit();

  sSetupPhaseUi = true;
  sTftBootLogReady = false;

  tft.init();
  tft.setRotation(NMS_TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  nmsTftTextBegin();
  nmsBootStatusReset();
  sTftBootLogReady = true;

  logInfo("initializing...");
  ensureWiFi();

  const String baseRaw = nmsGithubWordsBaseRaw(String(NMS_GITHUB_WORDS_BASE_URL));
  if (!nmsGithubRawBaseOk(baseRaw)) {
    logWarn("Library connection failed");
  } else {
    bool githubOk = false;
    if (nmsDictionariesUseAllowlist()) {
      String relPath;
      if (nmsPickRandomAllowedDictionary(relPath)) {
        String body;
        githubOk = nmsHttpsGetBody(nmsUrlJoin(baseRaw, relPath), body) && body.length() > 0;
      }
    } else {
      const String indexUrl = nmsUrlJoin(baseRaw, "index.txt");
      String indexBody;
      githubOk = nmsHttpsGetBody(indexUrl, indexBody) && indexBody.length() > 0;
    }
    if (githubOk) {
      logInfo("Library link successful");
    } else {
      logWarn("Library link failed");
    }
  }

  strip.begin();
  strip.setBrightness(NMS_LED_BRIGHTNESS);
  strip.clear();
  strip.show();

  randomSeed((uint32_t)esp_random());

  ensureTimeSynced();
  delay(800);
  sSetupPhaseUi = false;
}

/** Uniform random delay in seconds between min and max (inclusive). */
static uint32_t nmsPickRandomWordDelaySec(void) {
  uint32_t lo = (uint32_t)NMS_WORD_RANDOM_MIN_SEC;
  uint32_t hi = (uint32_t)NMS_WORD_RANDOM_MAX_SEC;
  if (hi < lo) {
    hi = lo;
  }
  const uint32_t span = hi - lo + 1;
  return lo + (uint32_t)random(span);
}

void loop() {
  ensureWiFi();

  time_t now = time(nullptr);
  if (now < 1700000000) {
    ensureTimeSynced();
    now = time(nullptr);
    if (now < 1700000000) {
      delay(2000);
      return;
    }
  }

  static uint32_t nextFetchAtMs = 0;
  static uint32_t blankGapMsEffective = 0;

  const uint32_t ms = millis();

  if (nextFetchAtMs != 0 && ms < nextFetchAtMs) {
    const uint32_t blankStartMs = nextFetchAtMs - blankGapMsEffective;
    if (blankGapMsEffective > 0 && ms >= blankStartMs) {
      if (!sBlankGapPainted) {
        drawWordBackdrop();
        nmsDrawAccentBorder(sLastAccent565);
        sBlankGapPainted = true;
      }
    }
    delay(50);
    return;
  }

  if (!sBlankGapPainted) {
    drawWordBackdrop();
    nmsDrawAccentBorder(sLastAccent565);
  }

  String en, alien;
  if (nmsFetchRandomWordFromGithub(en, alien)) {
    showWord(en, alien, false);

    const uint32_t delaySec = nmsPickRandomWordDelaySec();
    uint32_t periodMs = delaySec * 1000UL;
    uint32_t wantGapMs = (uint32_t)NMS_BACKGROUND_GAP_SEC * 1000UL;
    if (wantGapMs > periodMs) {
      wantGapMs = periodMs > 50 ? periodMs - 50 : 0;
    }
    blankGapMsEffective = wantGapMs;

    const uint32_t afterShowMs = millis();
    nextFetchAtMs = afterShowMs + periodMs;
    sBlankGapPainted = false;
  } else {
    drawWordBackdrop();
    nmsDrawAccentBorder(sLastAccent565);
    blankGapMsEffective = 0;
    nextFetchAtMs = millis() + 60000;
    sBlankGapPainted = true;
  }

  delay(50);
}
