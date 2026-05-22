#pragma once
// Dictionary allowlist from nms_config.h (NMS_DICTIONARIES).
#include <Arduino.h>
#include "nms_config.h"

#ifndef NMS_DICTIONARY_MAX
#define NMS_DICTIONARY_MAX 8
#endif

#ifndef NMS_DICTIONARIES
#define NMS_DICTIONARIES ""
#endif

static String sNmsDictAllow[NMS_DICTIONARY_MAX];
static uint8_t sNmsDictAllowCount = 0;

static bool nmsDictNameCharsOk(const String& name) {
  if (name.length() == 0 || name.length() > 64) {
    return false;
  }
  for (size_t i = 0; i < name.length(); i++) {
    const char c = name.charAt((int)i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
      continue;
    }
    return false;
  }
  return true;
}

static String nmsDictNormalizeName(String name) {
  name.trim();
  name.toLowerCase();
  if (name.endsWith(".txt")) {
    name.remove(name.length() - 4, 4);
  }
  return name;
}

/** Parse comma-separated NMS_DICTIONARIES (e.g. "gek,korvax"). Empty = use index.txt on GitHub. */
static void nmsDictionariesInit(void) {
  sNmsDictAllowCount = 0;
  String cfg = String(NMS_DICTIONARIES);
  cfg.trim();
  if (cfg.length() == 0) {
    return;
  }

  int start = 0;
  while (start <= (int)cfg.length() && sNmsDictAllowCount < NMS_DICTIONARY_MAX) {
    int comma = cfg.indexOf(',', start);
    if (comma < 0) {
      comma = cfg.length();
    }
    String part = cfg.substring(start, comma);
    part = nmsDictNormalizeName(part);
    if (part.length() > 0 && nmsDictNameCharsOk(part)) {
      sNmsDictAllow[sNmsDictAllowCount++] = part;
    }
    start = comma + 1;
  }
}

static bool nmsDictionariesUseAllowlist(void) {
  return sNmsDictAllowCount > 0;
}

/** Random dictionary filename (e.g. gek.txt) from allowlist. */
static bool nmsPickRandomAllowedDictionary(String& relPathOut) {
  if (sNmsDictAllowCount == 0) {
    return false;
  }
  const uint8_t pick = (uint8_t)random(sNmsDictAllowCount);
  relPathOut = sNmsDictAllow[pick] + ".txt";
  return true;
}

/** True if relPath (e.g. gek.txt or cs/gek.txt) matches an allowlisted dictionary name. */
static bool nmsDictionaryPathAllowed(const String& relPath) {
  if (sNmsDictAllowCount == 0) {
    return true;
  }
  String base = relPath;
  const int sl = base.lastIndexOf('/');
  if (sl >= 0) {
    base = base.substring(sl + 1);
  }
  base = nmsDictNormalizeName(base);
  for (uint8_t i = 0; i < sNmsDictAllowCount; i++) {
    if (base == sNmsDictAllow[i]) {
      return true;
    }
  }
  return false;
}
