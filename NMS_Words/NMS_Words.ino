// NMS_Words — ESP32-C3 + round TFT (GC9A01). Words from GitHub RAW only.
#include "nms_config.h"

#ifndef NMS_SHOW_CLOCK
#define NMS_SHOW_CLOCK 1
#endif

// Optional nms_bg565.h background: if colours look wrong after pushImage(), toggle NMS_BG565_PUSH_SWAP_BYTES in nms_config.h
#ifndef NMS_BG565_PUSH_SWAP_BYTES
#define NMS_BG565_PUSH_SWAP_BYTES 1
#endif

// TFT_eSPI: 0 = 0°, 1 = 90°, 2 = 180°, 3 = 270° (clockwise)
#ifndef NMS_TFT_ROTATION
#define NMS_TFT_ROTATION 0
#endif

// Accent-coloured circular ring at panel edge (same RGB565 as alien word / NeoPixel). 0 = off.
#ifndef NMS_ACCENT_BORDER_PX
#define NMS_ACCENT_BORDER_PX 6
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
#include "nms_tft_latin_fold.h"

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

static bool nmsGithubLineOk(const String& line) {
  String t = line;
  t.trim();
  if (t.length() == 0) return false;
  if (t.charAt(0) == '#') return false;
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
    if (nmsGithubLineOk(line)) {
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
    if (nmsGithubLineOk(line)) {
      if (i == pick) {
        pathOut = line;
        pathOut.trim();
        return true;
      }
      i++;
    }
    start = nl + 1;
  }
  return false;
}

static bool nmsPickRandomTabLine(const String& fileBody, String& col1, String& col2) {
  int n = 0;
  int start = 0;
  while (start < (int)fileBody.length()) {
    int nl = fileBody.indexOf('\n', start);
    if (nl < 0) nl = fileBody.length();
    String line = fileBody.substring(start, nl);
    line.trim();
    if (line.length() > 0 && line.charAt(0) != '#') {
      const int tab = line.indexOf('\t');
      if (tab > 0 && tab < (int)line.length() - 1) {
        n++;
      }
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
    String line = fileBody.substring(start, nl);
    line.trim();
    if (line.length() > 0 && line.charAt(0) != '#') {
      const int tab = line.indexOf('\t');
      if (tab > 0 && tab < (int)line.length() - 1) {
        if (i == pick) {
          col1 = line.substring(0, tab);
          col2 = line.substring(tab + 1);
          col1.trim();
          col2.trim();
          return true;
        }
        i++;
      }
    }
    start = nl + 1;
  }
  return false;
}

/** Race label for colours from path (e.g. cs/gek.txt → Gek). */
static String nmsRaceLabelFromWordPath(const String& relPath) {
  String base = relPath;
  const int sl = base.lastIndexOf('/');
  if (sl >= 0) {
    base = base.substring(sl + 1);
  }
  if (base.endsWith(".txt")) {
    base.remove(base.length() - 4, 4);
  }
  base.trim();
  base.toLowerCase();
  if (base == "gek") return "Gek";
  if (base == "korvax") return "Korvax";
  if (base == "vykeen") return "Vy'keen";
  if (base == "atlas") return "Atlas";
  if (base == "autophage") return "Autophage";
  if (base.length() == 0) return "Gek";
  base.setCharAt(0, (char)toupper((unsigned char)base.charAt(0)));
  return base;
}

static bool nmsFetchRandomWordFromGithub(String& englishOut, String& alienOut, String& raceLabelOut) {
  englishOut = "";
  alienOut = "";
  raceLabelOut = "Gek";

  const String baseRaw = nmsGithubWordsBaseRaw(String(NMS_GITHUB_WORDS_BASE_URL));
  if (!nmsGithubRawBaseOk(baseRaw)) {
    return false;
  }

  const String indexUrl = nmsUrlJoin(baseRaw, "index.txt");
  String indexBody;
  if (!nmsHttpsGetBody(indexUrl, indexBody)) {
    return false;
  }
  String relPath;
  if (!nmsPickRandomIndexPath(indexBody, relPath)) {
    return false;
  }
  const String fileUrl = nmsUrlJoin(baseRaw, relPath);
  String fileBody;
  if (!nmsHttpsGetBody(fileUrl, fileBody)) {
    return false;
  }
  if (!nmsPickRandomTabLine(fileBody, englishOut, alienOut)) {
    return false;
  }
  raceLabelOut = nmsRaceLabelFromWordPath(relPath);
  return true;
}

// ----- Display -----
TFT_eSPI tft = TFT_eSPI();

// textSize: alien word (with wrapping) vs English sentence lines vs optional clock
static const uint8_t NMS_UI_TEXT_SIZE = 2;
static const uint8_t NMS_SENTENCE_TEXT_SIZE = 1;
#if NMS_SHOW_CLOCK
static const uint8_t NMS_CLOCK_TEXT_SIZE = 1;
#endif

// Vertical layout for showWord() (pixels)
static const int NMS_ALIEN_TOP_Y = 80;
static const int NMS_GAP_SINGLELINE_ALIEN_TO_SENTENCE = 12;
static const int NMS_GAP_WRAPPED_ALIEN_TO_SENTENCE = 10;
static const int NMS_SENTENCE_LINE_EXTRA = 5;

/** Wrapped lines, each centred horizontally (topY = top of first line). */
static int drawCenteredWrapped(const String& text, int topY, int maxWidth, int font, uint16_t color, uint8_t textSize) {
  const uint8_t prevDatum = tft.getTextDatum();
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(textSize);
  tft.setTextFont(font);
  tft.setTextColor(color);

  const int cx = tft.width() / 2;
  const int lineHeight = tft.fontHeight(font) + 4;
  int yPos = topY;
  String line;

  auto flushLine = [&](const String& l) {
    if (l.length() == 0) return;
    const int w = tft.textWidth(l);
    tft.drawString(l, cx - w / 2, yPos);
    yPos += lineHeight;
  };

  int start = 0;
  while (start < (int)text.length()) {
    int space = text.indexOf(' ', start);
    if (space < 0) space = text.length();
    String word = text.substring(start, space);
    String trial = (line.length() == 0) ? word : (line + " " + word);

    const int w = tft.textWidth(trial);
    if (w <= maxWidth) {
      line = trial;
    } else {
      flushLine(line);
      line = word;
    }
    start = space + 1;
  }
  flushLine(line);
  tft.setTextDatum(prevDatum);
  return yPos;
}

// Soft glow: cx = screen centre; cy = vertical centre of text block (TL datum + offset).
static void drawGlowStringAt(int cx, int cy, int font, const String& s, uint16_t fg, uint16_t glowCol, int radius, uint8_t textSize) {
  const uint8_t prevDatum = tft.getTextDatum();
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(textSize);
  tft.setTextFont(font);
  const int w = tft.textWidth(s);
  const int h = tft.fontHeight(font);
  const int left = cx - w / 2;
  const int top = cy - h / 2;
  if (radius > 0) {
    for (int dy = -radius; dy <= radius; dy++) {
      for (int dx = -radius; dx <= radius; dx++) {
        if (dx == 0 && dy == 0) continue;
        tft.setTextColor(glowCol);
        tft.drawString(s, left + dx, top + dy);
      }
    }
  }
  tft.setTextColor(fg);
  tft.drawString(s, left, top);
  tft.setTextDatum(prevDatum);
}

// Built-in GLCD fonts lack accented glyphs — UTF-8 Latin is folded to ASCII (nms_tft_latin_fold.h); Greek is transliterated.
static size_t utf8DecodeCp(const String& in, size_t i, uint32_t& cp) {
  const size_t n = in.length();
  if (i >= n) {
    cp = 0;
    return 0;
  }
  const uint8_t c0 = (uint8_t)in[i];
  if (c0 < 0x80u) {
    cp = c0;
    return 1;
  }
  if ((c0 & 0xE0u) == 0xC0u) {
    if (i + 1 >= n || (((uint8_t)in[i + 1]) & 0xC0u) != 0x80u) {
      cp = 0xFFFD;
      return 1;
    }
    cp = ((uint32_t)(c0 & 0x1Fu) << 6) | ((uint8_t)in[i + 1] & 0x3Fu);
    if (cp < 0x80u) {
      cp = 0xFFFD;
      return 1;
    }
    return 2;
  }
  if ((c0 & 0xF0u) == 0xE0u) {
    if (i + 2 >= n || (((uint8_t)in[i + 1]) & 0xC0u) != 0x80u || (((uint8_t)in[i + 2]) & 0xC0u) != 0x80u) {
      cp = 0xFFFD;
      return 1;
    }
    cp = ((uint32_t)(c0 & 0x0Fu) << 12) | (((uint32_t)(uint8_t)in[i + 1] & 0x3Fu) << 6) | ((uint8_t)in[i + 2] & 0x3Fu);
    if (cp < 0x800u) {
      cp = 0xFFFD;
      return 1;
    }
    return 3;
  }
  if ((c0 & 0xF8u) == 0xF0u) {
    if (i + 3 >= n || (((uint8_t)in[i + 1]) & 0xC0u) != 0x80u || (((uint8_t)in[i + 2]) & 0xC0u) != 0x80u || (((uint8_t)in[i + 3]) & 0xC0u) != 0x80u) {
      cp = 0xFFFD;
      return 1;
    }
    cp = ((uint32_t)(c0 & 0x07u) << 18) | (((uint32_t)(uint8_t)in[i + 1] & 0x3Fu) << 12) | (((uint32_t)(uint8_t)in[i + 2] & 0x3Fu) << 6) | ((uint8_t)in[i + 3] & 0x3Fu);
    if (cp < 0x10000u) {
      cp = 0xFFFD;
      return 1;
    }
    return 4;
  }
  cp = 0xFFFD;
  return 1;
}

static void nmsAppendGreekLatin(String& out, uint32_t cp) {
  switch (cp) {
    case 0x0391:
    case 0x03B1:
      out += 'a';
      break;
    case 0x0392:
    case 0x03B2:
      out += 'b';
      break;
    case 0x0393:
    case 0x03B3:
      out += 'g';
      break;
    case 0x0394:
    case 0x03B4:
      out += 'd';
      break;
    case 0x0395:
    case 0x03B5:
      out += 'e';
      break;
    case 0x0396:
    case 0x03B6:
      out += 'z';
      break;
    case 0x0397:
    case 0x03B7:
      out += 'h';
      break;
    case 0x0398:
    case 0x03B8:
      out += "th";
      break;
    case 0x0399:
    case 0x03B9:
      out += 'i';
      break;
    case 0x039A:
    case 0x03BA:
      out += 'k';
      break;
    case 0x039B:
    case 0x03BB:
      out += 'l';
      break;
    case 0x039C:
    case 0x03BC:
      out += 'm';
      break;
    case 0x039D:
    case 0x03BD:
      out += 'n';
      break;
    case 0x039E:
    case 0x03BE:
      out += 'x';
      break;
    case 0x039F:
    case 0x03BF:
      out += 'o';
      break;
    case 0x03A0:
    case 0x03C0:
      out += 'p';
      break;
    case 0x03A1:
    case 0x03C1:
      out += 'r';
      break;
    case 0x03A3:
    case 0x03C3:
    case 0x03C2:
      out += 's';
      break;
    case 0x03A4:
    case 0x03C4:
      out += 't';
      break;
    case 0x03A5:
    case 0x03C5:
      out += 'y';
      break;
    case 0x03A6:
    case 0x03C6:
      out += 'f';
      break;
    case 0x03A7:
    case 0x03C7:
      out += "ch";
      break;
    case 0x03A8:
    case 0x03C8:
      out += "ps";
      break;
    case 0x03A9:
    case 0x03C9:
      out += 'w';
      break;
    default:
      out += '?';
      break;
  }
}

static String nmsTftSanitize(const String& in) {
  String out;
  if (in.length() == 0) {
    return out;
  }
  out.reserve(in.length());
  for (size_t i = 0; i < in.length();) {
    uint32_t cp = 0;
    const size_t adv = utf8DecodeCp(in, i, cp);
    if (adv == 0) {
      break;
    }
    if (cp >= 0x0300u && cp <= 0x036Fu) {
      i += adv;
      continue;
    }
    if (cp < 0x80u) {
      out += (char)cp;
      i += adv;
      continue;
    }
    if (cp >= 0x0370u && cp <= 0x03FFu) {
      nmsAppendGreekLatin(out, cp);
    } else if (nmsTryAppendLatinFolded(out, cp)) {
    } else {
      out += '?';
    }
    i += adv;
  }
  return out;
}

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

  String line = nmsTftSanitize(msg);
  tft.setTextFont(2);
  tft.setTextSize(1);
  const int maxW = tft.width() - 16;
  while (line.length() > 0 && tft.textWidth(line) > maxW) {
    line.remove(line.length() - 1);
  }
  if (sBootCount < NMS_BOOT_STATUS_MAX_LINES) {
    sBootText[sBootCount] = line;
    sBootCol[sBootCount] = fg;
    sBootCount++;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  const int cx = tft.width() / 2;
  const int fh = tft.fontHeight(2) + 4;
  const int n = (int)sBootCount;
  const int totalH = n * fh;
  int y = (tft.height() - totalH) / 2 + fh / 2;
  for (int i = 0; i < n; i++) {
    tft.setTextColor(sBootCol[(uint8_t)i], TFT_BLACK);
    tft.drawString(sBootText[(uint8_t)i], cx, y + i * fh);
  }
  tft.setTextDatum(TL_DATUM);
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

/** Alien word only; returns vertical centre Y of first English sentence line (font 2). */
static int drawWordAlienOnly(int cx, int alienTopY, int maxW, const String& alienDraw, uint16_t accent, uint16_t alienGlow) {
  tft.setTextSize(NMS_UI_TEXT_SIZE);
  tft.setTextFont(4);
  if (tft.textWidth(alienDraw) <= maxW) {
    const int aFont = 4;
    const int hA = tft.fontHeight(aFont);
    const int cya = alienTopY + hA / 2;
    drawGlowStringAt(cx, cya, aFont, alienDraw, accent, alienGlow, 2, NMS_UI_TEXT_SIZE);
    tft.setTextSize(NMS_SENTENCE_TEXT_SIZE);
    tft.setTextFont(2);
    return alienTopY + hA + NMS_GAP_SINGLELINE_ALIEN_TO_SENTENCE + tft.fontHeight(2) / 2;
  }
  const int belowAlienTop = drawCenteredWrapped(alienDraw, alienTopY, maxW, 2, accent, NMS_UI_TEXT_SIZE);
  tft.setTextSize(NMS_SENTENCE_TEXT_SIZE);
  tft.setTextFont(2);
  return belowAlienTop + NMS_GAP_WRAPPED_ALIEN_TO_SENTENCE + tft.fontHeight(2) / 2;
}

static uint16_t accentForRace(const String& race) {
  if (race == "Gek") return tft.color565(255, 205, 65);
  if (race == "Korvax") return tft.color565(95, 220, 255);
  if (race == "Vy'keen") return tft.color565(255, 75, 45);
  if (race == "Atlas") return tft.color565(235, 65, 130);
  if (race == "Autophage") return tft.color565(105, 255, 145);
  return TFT_WHITE;
}

static void showWord(const String& race, const String& english, const String& alien, bool drawBackdropFirst = true) {
  const int cx = tft.width() / 2;
  const int margin = 14;
  const int maxW = tft.width() - 2 * margin;
  const uint16_t accent = accentForRace(race);
  const uint16_t alienGlow = tft.color565(28, 30, 42);
  const uint16_t sentenceGlow = tft.color565(70, 70, 85);

  nmsNeoPixelSetAccent(accent);

  String alienDraw(nmsTftSanitize(alien));
  alienDraw.trim();

  const int alienTopY = NMS_ALIEN_TOP_Y;
  const String lineA = "You have learned the";
  const String lineB = nmsTftSanitize(race) + String(" word for");
  const String quoted = String("'") + nmsTftSanitize(english) + "'";

  if (drawBackdropFirst) {
    drawWordBackdrop();
  }
  const int yA = drawWordAlienOnly(cx, alienTopY, maxW, alienDraw, accent, alienGlow);

  tft.setTextSize(NMS_SENTENCE_TEXT_SIZE);
  tft.setTextFont(2);
  const int fhSentence = tft.fontHeight(2);
  const int yB = yA + fhSentence + NMS_SENTENCE_LINE_EXTRA;
  const int yC = yB + fhSentence + NMS_SENTENCE_LINE_EXTRA;

  drawGlowStringAt(cx, yA, 2, lineA, TFT_WHITE, sentenceGlow, 1, NMS_SENTENCE_TEXT_SIZE);
  drawGlowStringAt(cx, yB, 2, lineB, TFT_WHITE, sentenceGlow, 1, NMS_SENTENCE_TEXT_SIZE);
  drawGlowStringAt(cx, yC, 2, quoted, TFT_WHITE, sentenceGlow, 1, NMS_SENTENCE_TEXT_SIZE);

#if NMS_SHOW_CLOCK
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(NMS_CLOCK_TEXT_SIZE);
  tft.setTextFont(2);
#if defined(NMS_HAS_BG565)
  tft.setTextColor(TFT_DARKGREY);
#else
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
#endif
  tft.drawString(String(buf), cx, tft.height() - 16);
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
    logInfo("WiFi OK");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(NMS_WIFI_SSID, NMS_WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    logInfo("Network connection operational");
  } else {
    logWarn("Network connection failed");
  }
}


void setup() {
  sSetupPhaseUi = true;
  sTftBootLogReady = false;

  tft.init();
  tft.setRotation(NMS_TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  nmsBootStatusReset();
  sTftBootLogReady = true;

  logInfo("Initializing...");
  ensureWiFi();

  const String baseRaw = nmsGithubWordsBaseRaw(String(NMS_GITHUB_WORDS_BASE_URL));
  if (!nmsGithubRawBaseOk(baseRaw)) {
    logWarn("Library connection failed");
  } else {
    const String indexUrl = nmsUrlJoin(baseRaw, "index.txt");
    String indexBody;
    if (nmsHttpsGetBody(indexUrl, indexBody) && indexBody.length() > 0) {
      logInfo("Library connection successful");
    } else {
      logWarn("Library connection failed");
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

  String en, alien, raceLb;
  if (nmsFetchRandomWordFromGithub(en, alien, raceLb)) {
    showWord(raceLb, en, alien, false);

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
