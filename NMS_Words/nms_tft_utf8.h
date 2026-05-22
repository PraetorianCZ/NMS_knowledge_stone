#pragma once
// UTF-8 on TFT_eSPI via U8g2_for_TFT_eSPI.
// Czech: use fonts with Latin Extended (_te suffix or unifont_t_extended).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>

extern TFT_eSPI tft;
extern U8g2_for_TFT_eSPI u8g2;

#ifndef NMS_MAIN_WORD_FONT_SIZE
#define NMS_MAIN_WORD_FONT_SIZE 1
#endif

// Main word font (override NMS_FONT_TITLE in nms_config.h to pick a font directly)
#ifndef NMS_FONT_TITLE
#if NMS_MAIN_WORD_FONT_SIZE == 2
#define NMS_FONT_TITLE u8g2_font_helvB24_te
#elif NMS_MAIN_WORD_FONT_SIZE == 1
#define NMS_FONT_TITLE u8g2_font_helvB18_te
#else
#define NMS_FONT_TITLE u8g2_font_unifont_t_extended
#endif
#endif

#ifndef NMS_FONT_BODY
#define NMS_FONT_BODY u8g2_font_unifont_t_extended
#endif

// Greek + Latin (Autophage). helvB*_te has no Greek; unifont_t_greek is tiny — use 10x20_t_greek (~title size).
#ifndef NMS_FONT_GREEK
#if NMS_MAIN_WORD_FONT_SIZE >= 1
#define NMS_FONT_GREEK u8g2_font_10x20_t_greek
#else
#define NMS_FONT_GREEK u8g2_font_unifont_t_greek
#endif
#endif

// Phrase / translation lines: 0 = small (body font) | 1 = same size as main word (title font)
#ifndef NMS_PHRASE_FONT_SIZE
#define NMS_PHRASE_FONT_SIZE 1
#endif

static inline bool nmsTftPhraseUsesTitleFont(void) {
  return NMS_PHRASE_FONT_SIZE != 0;
}

// 1 = legacy small unifont_t_greek. 0 = large 10x20_t_greek when main word contains Greek (default).
#ifndef NMS_MAIN_WORD_USE_GREEK_FONT
#define NMS_MAIN_WORD_USE_GREEK_FONT 0
#endif

static uint8_t sNmsUtf8State = 0;
static uint16_t sNmsUtf8Acc = 0;

static inline void nmsUtf8ResetDecoder(void) {
  sNmsUtf8State = 0;
  sNmsUtf8Acc = 0;
}

static uint32_t nmsUtf8NextCp(const String& in, size_t& idx) {
  if (idx >= in.length()) {
    return 0;
  }
  const uint8_t b = (uint8_t)in[idx++];
  if (sNmsUtf8State == 0) {
    if (b < 0x80u) {
      return b;
    }
    if ((b & 0xE0u) == 0xC0u) {
      sNmsUtf8State = 1;
      sNmsUtf8Acc = b & 0x1Fu;
      return 0xFFFFFEu;
    }
    if ((b & 0xF0u) == 0xE0u) {
      sNmsUtf8State = 2;
      sNmsUtf8Acc = b & 0x0Fu;
      return 0xFFFFFEu;
    }
    if ((b & 0xF8u) == 0xF0u) {
      sNmsUtf8State = 3;
      sNmsUtf8Acc = b & 0x07u;
      return 0xFFFFFEu;
    }
    return 0xFFFDu;
  }
  if ((b & 0xC0u) != 0x80u) {
    sNmsUtf8State = 0;
    return 0xFFFDu;
  }
  sNmsUtf8Acc = (sNmsUtf8Acc << 6) | (b & 0x3Fu);
  if (sNmsUtf8State > 1) {
    sNmsUtf8State--;
    return 0xFFFFFEu;
  }
  sNmsUtf8State = 0;
  return sNmsUtf8Acc;
}

static bool nmsUtf8ContainsGreek(const String& s) {
  nmsUtf8ResetDecoder();
  for (size_t i = 0; i < s.length();) {
    const uint32_t cp = nmsUtf8NextCp(s, i);
    if (cp == 0) {
      break;
    }
    if (cp == 0xFFFFFEu) {
      continue;
    }
    if (cp >= 0x0370u && cp <= 0x03FFu) {
      return true;
    }
  }
  return false;
}

static inline bool nmsTftMainWordNeedsGreekFont(const String& s) {
  if (!nmsUtf8ContainsGreek(s)) {
    return false;
  }
  return NMS_MAIN_WORD_USE_GREEK_FONT == 0;
}

static inline void nmsTftSetFontForText(const String& s, bool title) {
  if (title) {
    if (nmsTftMainWordNeedsGreekFont(s)) {
      u8g2.setFont(NMS_FONT_GREEK);
    } else if (NMS_MAIN_WORD_USE_GREEK_FONT != 0 && nmsUtf8ContainsGreek(s)) {
      u8g2.setFont(u8g2_font_unifont_t_greek);
    } else {
      u8g2.setFont(NMS_FONT_TITLE);
    }
  } else if (nmsTftPhraseUsesTitleFont()) {
    u8g2.setFont(NMS_FONT_TITLE);
  } else {
    u8g2.setFont(NMS_FONT_BODY);
  }
}

static inline void nmsTftTextBegin(void) {
  u8g2.begin(tft);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setBackgroundColor(TFT_BLACK);
  nmsUtf8ResetDecoder();
}

static inline void nmsTftSetBodyFont(void) {
  u8g2.setFont(NMS_FONT_BODY);
}

static inline void nmsTftSetTitleFont(void) {
  u8g2.setFont(NMS_FONT_TITLE);
}

static inline int nmsTftLineHeightFor(bool title, const String& sample = String()) {
  if (title && sample.length() > 0) {
    nmsTftSetFontForText(sample, true);
  } else if (title) {
    u8g2.setFont(NMS_FONT_TITLE);
  } else {
    u8g2.setFont(NMS_FONT_BODY);
  }
  return (int)u8g2.getFontAscent() - (int)u8g2.getFontDescent() + 4;
}

/** Line height for main word using the same font as draw (incl. Greek / Autophage). */
static inline int nmsTftMainWordLineHeight(const String& mainWord) {
  return nmsTftLineHeightFor(true, mainWord);
}

static inline int nmsTftLineHeight(void) {
  return nmsTftLineHeightFor(false);
}

static inline int nmsTftTextWidth(const String& s, bool title = false) {
  nmsTftSetFontForText(s, title);
  return u8g2.getUTF8Width(s.c_str());
}

static inline int nmsTftBaselineForCenterY(int cy) {
  return cy + (((int)u8g2.getFontAscent() - (int)u8g2.getFontDescent()) / 2);
}

static void nmsUtf8PopLastCodepoint(String& s) {
  if (s.length() == 0) {
    return;
  }
  size_t i = s.length();
  while (i > 0 && (((uint8_t)s[i - 1]) & 0xC0u) == 0x80u) {
    i--;
  }
  if (i > 0) {
    s.remove(i - 1);
  }
}

static void nmsTftDrawUtf8At(int x, int baselineY, const String& s, uint16_t fg, bool title) {
  nmsTftSetFontForText(s, title);
  u8g2.setForegroundColor(fg);
  u8g2.drawUTF8(x, baselineY, s.c_str());
}

static void nmsTftDrawUtf8Centered(int cx, int baselineY, const String& s, uint16_t fg, bool title = false) {
  nmsTftSetFontForText(s, title);
  const int w = u8g2.getUTF8Width(s.c_str());
  u8g2.setForegroundColor(fg);
  u8g2.drawUTF8(cx - w / 2, baselineY, s.c_str());
}

static void nmsTftDrawGlowCentered(int cx, int baselineY, const String& s, uint16_t fg, uint16_t glow, int radius, bool title = false) {
  if (radius > 0) {
    for (int dy = -radius; dy <= radius; dy++) {
      for (int dx = -radius; dx <= radius; dx++) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        nmsTftDrawUtf8Centered(cx + dx, baselineY + dy, s, glow, title);
      }
    }
  }
  nmsTftDrawUtf8Centered(cx, baselineY, s, fg, title);
}

static int nmsTftCountWrappedLines(const String& text, int maxWidth, bool title = false) {
  nmsTftSetFontForText(text, title);
  int lines = 0;
  String line;
  int start = 0;
  while (start < (int)text.length()) {
    int space = text.indexOf(' ', start);
    if (space < 0) {
      space = text.length();
    }
    const String word = text.substring(start, space);
    const String trial = (line.length() == 0) ? word : (line + " " + word);
    nmsTftSetFontForText(trial, title);
    if (u8g2.getUTF8Width(trial.c_str()) <= maxWidth) {
      line = trial;
    } else {
      if (line.length() > 0) {
        lines++;
      }
      line = word;
    }
    start = space + 1;
  }
  if (line.length() > 0) {
    lines++;
  }
  return lines > 0 ? lines : 1;
}

static int nmsTftDrawWrappedCentered(const String& text, int topBaselineY, int maxWidth, uint16_t color, bool title = false) {
  nmsTftSetFontForText(text, title);
  const int cx = tft.width() / 2;
  const int lh = nmsTftLineHeightFor(title, text);
  int baselineY = topBaselineY;
  String line;

  auto flushLine = [&]() {
    if (line.length() == 0) {
      return;
    }
    nmsTftDrawUtf8Centered(cx, baselineY, line, color, title);
    baselineY += lh;
    line = "";
  };

  int start = 0;
  while (start < (int)text.length()) {
    int space = text.indexOf(' ', start);
    if (space < 0) {
      space = text.length();
    }
    const String word = text.substring(start, space);
    const String trial = (line.length() == 0) ? word : (line + " " + word);
    nmsTftSetFontForText(trial, title);
    if (u8g2.getUTF8Width(trial.c_str()) <= maxWidth) {
      line = trial;
    } else {
      flushLine();
      line = word;
    }
    start = space + 1;
  }
  flushLine();
  return baselineY;
}
