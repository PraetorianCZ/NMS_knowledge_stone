#pragma once
// UI strings and accent from # @… lines in each dictionary .txt (see dictionaries/README.md).
//   # @phrase1: …   # @phrase2: … (empty = skip; drawn below the large word)
//   # @quote: 0 | 1
//   # @display: 0 | 1 | 2  — overrides NMS_WORD_DISPLAY_MODE for this file
//   # @accent: #RRGGBB — main word (2nd column, large)
//   # @accent_ref: #RRGGBB — reference / translation (1st column)
//   # @name: Label at top (optional; needs NMS_SHOW_DICT_NAME in config)
//   # @show_name: 0 | 1 — override global name visibility for this file
#include <Arduino.h>
#include <stdlib.h>

#ifndef NMS_WORD_DISPLAY_MODE
#define NMS_WORD_DISPLAY_MODE 1
#endif

#ifndef NMS_SHOW_DICT_NAME
#define NMS_SHOW_DICT_NAME 1
#endif

/** Strip trailing inline comment (space + # or //). Full-line # comments unchanged. */
static inline String nmsDictPrepareDataLine(String line) {
  line.trim();
  if (line.length() == 0 || line.charAt(0) == '#') {
    return line;
  }
  const int slash = line.indexOf("//");
  if (slash > 0) {
    line = line.substring(0, slash);
    line.trim();
  }
  const int hash = line.indexOf(" #");
  if (hash > 0) {
    line = line.substring(0, hash);
    line.trim();
  }
  return line;
}

struct NmsDictMeta {
  String phrase1;
  String phrase2;
  String name;
  uint16_t accent565 = TFT_WHITE;
  uint16_t accentRef565 = TFT_WHITE;
  bool hasMeta = false;
  bool hasAccentColor = false;
  bool hasAccentRefColor = false;
  bool quoteRef = false;
  bool hasQuote = false;
  bool hasDisplay = false;
  bool showName = true;
  bool hasShowName = false;
  uint8_t displayMode = 255;
};

static inline uint16_t nmsRgbTo565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static bool nmsDictMetaParseHexComponent(const String& s, size_t i, uint8_t& out) {
  if (i >= s.length()) {
    return false;
  }
  const char c = s.charAt((int)i);
  uint8_t v = 0;
  if (c >= '0' && c <= '9') {
    v = (uint8_t)(c - '0');
  } else if (c >= 'a' && c <= 'f') {
    v = (uint8_t)(c - 'a' + 10);
  } else if (c >= 'A' && c <= 'F') {
    v = (uint8_t)(c - 'A' + 10);
  } else {
    return false;
  }
  out = v;
  return true;
}

static bool nmsDictMetaParseAccentColor(const String& valRaw, uint16_t& out565) {
  String s = valRaw;
  s.trim();
  if (s.length() == 0) {
    return false;
  }

  if (s.startsWith("rgb(") || s.startsWith("RGB(")) {
    String inner = s.substring(4);
    if (inner.endsWith(")")) {
      inner.remove(inner.length() - 1);
    }
    inner.trim();
    const int c1 = inner.indexOf(',');
    const int c2 = inner.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      const int r = inner.substring(0, c1).toInt();
      const int g = inner.substring(c1 + 1, c2).toInt();
      const int b = inner.substring(c2 + 1).toInt();
      if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
        out565 = nmsRgbTo565((uint8_t)r, (uint8_t)g, (uint8_t)b);
        return true;
      }
    }
    return false;
  }

  if (s.startsWith("0x") || s.startsWith("0X")) {
    const unsigned long v = strtoul(s.c_str() + 2, nullptr, 16);
    if (v <= 0xFFFFUL) {
      out565 = (uint16_t)v;
      return true;
    }
    return false;
  }

  const int c1 = s.indexOf(',');
  if (c1 > 0) {
    const int c2 = s.indexOf(',', c1 + 1);
    if (c2 > c1) {
      const int r = s.substring(0, c1).toInt();
      const int g = s.substring(c1 + 1, c2).toInt();
      const int b = s.substring(c2 + 1).toInt();
      if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
        out565 = nmsRgbTo565((uint8_t)r, (uint8_t)g, (uint8_t)b);
        return true;
      }
    }
    return false;
  }

  if (s.charAt(0) == '#') {
    s = s.substring(1);
    s.trim();
  }

  bool hexOnly = true;
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s.charAt((int)i);
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      hexOnly = false;
      break;
    }
  }
  if (!hexOnly) {
    return false;
  }

  if (s.length() == 6) {
    uint8_t r = 0, g = 0, b = 0;
    uint8_t hi = 0, lo = 0;
    if (!nmsDictMetaParseHexComponent(s, 0, hi) || !nmsDictMetaParseHexComponent(s, 1, lo)) {
      return false;
    }
    r = (uint8_t)((hi << 4) | lo);
    if (!nmsDictMetaParseHexComponent(s, 2, hi) || !nmsDictMetaParseHexComponent(s, 3, lo)) {
      return false;
    }
    g = (uint8_t)((hi << 4) | lo);
    if (!nmsDictMetaParseHexComponent(s, 4, hi) || !nmsDictMetaParseHexComponent(s, 5, lo)) {
      return false;
    }
    b = (uint8_t)((hi << 4) | lo);
    out565 = nmsRgbTo565(r, g, b);
    return true;
  }

  if (s.length() == 3) {
    uint8_t hi = 0;
    if (!nmsDictMetaParseHexComponent(s, 0, hi)) {
      return false;
    }
    const uint8_t r = (uint8_t)((hi << 4) | hi);
    if (!nmsDictMetaParseHexComponent(s, 1, hi)) {
      return false;
    }
    const uint8_t g = (uint8_t)((hi << 4) | hi);
    if (!nmsDictMetaParseHexComponent(s, 2, hi)) {
      return false;
    }
    const uint8_t b = (uint8_t)((hi << 4) | hi);
    out565 = nmsRgbTo565(r, g, b);
    return true;
  }

  return false;
}

static NmsDictMeta sNmsDictMeta;

static inline void nmsDictMetaReset(void) {
  sNmsDictMeta = NmsDictMeta();
  sNmsDictMeta.displayMode = 255;
}

static inline void nmsDictMetaApplyKey(const String& keyRaw, const String& valRaw) {
  String key = keyRaw;
  key.trim();
  key.toLowerCase();
  String val = valRaw;
  val.trim();

  if (key == "phrase1" || key == "line1") {
    sNmsDictMeta.phrase1 = val;
    sNmsDictMeta.hasMeta = true;
    return;
  }
  if (key == "phrase2" || key == "line2") {
    sNmsDictMeta.phrase2 = val;
    sNmsDictMeta.hasMeta = true;
    return;
  }
  if (key == "quote") {
    sNmsDictMeta.quoteRef = (val == "1" || val == "true" || val == "yes");
    sNmsDictMeta.hasQuote = true;
    sNmsDictMeta.hasMeta = true;
    return;
  }
  if (key == "display" || key == "mode") {
    if (val == "0" || val == "1" || val == "2") {
      sNmsDictMeta.displayMode = (uint8_t)val.toInt();
      sNmsDictMeta.hasDisplay = true;
      sNmsDictMeta.hasMeta = true;
    }
    return;
  }
  if (key == "accent" || key == "color") {
    uint16_t c565 = 0;
    if (nmsDictMetaParseAccentColor(val, c565)) {
      sNmsDictMeta.accent565 = c565;
      sNmsDictMeta.hasAccentColor = true;
      sNmsDictMeta.hasMeta = true;
    }
    return;
  }
  if (key == "accent_ref" || key == "accent_translation" || key == "accent2") {
    uint16_t c565 = 0;
    if (nmsDictMetaParseAccentColor(val, c565)) {
      sNmsDictMeta.accentRef565 = c565;
      sNmsDictMeta.hasAccentRefColor = true;
      sNmsDictMeta.hasMeta = true;
    }
    return;
  }
  if (key == "name" || key == "title" || key == "label") {
    sNmsDictMeta.name = val;
    sNmsDictMeta.hasMeta = true;
    return;
  }
  if (key == "show_name" || key == "showname") {
    sNmsDictMeta.showName = (val == "1" || val == "true" || val == "yes");
    sNmsDictMeta.hasShowName = true;
    sNmsDictMeta.hasMeta = true;
  }
}

static inline uint16_t nmsDictMetaAccent(void) {
  if (sNmsDictMeta.hasAccentColor) {
    return sNmsDictMeta.accent565;
  }
  return TFT_WHITE;
}

static inline uint16_t nmsDictMetaAccentRef(void) {
  if (sNmsDictMeta.hasAccentRefColor) {
    return sNmsDictMeta.accentRef565;
  }
  return TFT_WHITE;
}

static void nmsDictMetaParseFromFileBody(const String& fileBody) {
  nmsDictMetaReset();
  int start = 0;
  while (start < (int)fileBody.length()) {
    int nl = fileBody.indexOf('\n', start);
    if (nl < 0) {
      nl = fileBody.length();
    }
    String line = fileBody.substring(start, nl);
    line.trim();
    if (line.length() > 0 && line.charAt(0) == '#') {
      String inner = line.substring(1);
      inner.trim();
      if (inner.length() > 0 && inner.charAt(0) == '@') {
        inner = inner.substring(1);
        inner.trim();
        const int colon = inner.indexOf(':');
        if (colon > 0) {
          nmsDictMetaApplyKey(inner.substring(0, colon), inner.substring(colon + 1));
        }
      }
    }
    start = nl + 1;
  }
}

static inline bool nmsDictMetaQuoteRef(void) {
  return sNmsDictMeta.hasQuote ? sNmsDictMeta.quoteRef : false;
}

static inline uint8_t nmsDictMetaDisplayMode(void) {
  if (sNmsDictMeta.hasDisplay && sNmsDictMeta.displayMode <= 2) {
    return sNmsDictMeta.displayMode;
  }
  return (uint8_t)NMS_WORD_DISPLAY_MODE;
}

static inline bool nmsDictMetaShowName(void) {
  if (sNmsDictMeta.name.length() == 0) {
    return false;
  }
  if (sNmsDictMeta.hasShowName) {
    return sNmsDictMeta.showName;
  }
  return NMS_SHOW_DICT_NAME != 0;
}

#ifndef NMS_DICT_NAME_BASELINE_Y
#define NMS_DICT_NAME_BASELINE_Y 29
#endif

static inline void nmsDictDrawNameHeader(int cx) {
  if (!nmsDictMetaShowName()) {
    return;
  }
  nmsTftSetBodyFont();
  nmsTftDrawUtf8Centered(cx, NMS_DICT_NAME_BASELINE_Y, sNmsDictMeta.name, TFT_DARKGREY, false);
}
