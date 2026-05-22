# Dictionaries

## File format

Each `*.txt` is UTF-8, tab-separated, with a **metadata header** (`# @…`) and data lines.

| Column 1 | Column 2 |
|----------|----------|
| Reference (shown in phrase area) | **Large word on TFT** |

Firmware ignores lines that start with `#` when picking a random word. Section markers are allowed: `# --- Evropa ---`.

### Metadata (`# @…`)

```text
# @name: Español
# @phrase1: You have learned the
# @phrase2: Spanish word for
# @quote: 0
# @accent: #FFC83C
# @accent_ref: #C83C50
# @show_name: 1
# @display: 1
```

| Key | Meaning |
|-----|---------|
| `name` | Label at top (if `NMS_SHOW_DICT_NAME` in `nms_config.h`) |
| `show_name` | `0` / `1` — override global name visibility |
| `phrase1` / `phrase2` | Text below the large word (`phrase2` empty = skip) |
| `quote` | `1` = wrap column 1 in `'…'` |
| `accent` / `accent_ref` | Colour of column 2 / column 1 (`#RRGGBB`, `r,g,b`, `rgb(...)`, `0xABCD`) |
| `display` | `0` / `1` / `2` — layout override (see main README) |

**Inline notes** on data lines (optional): `word\tanswer  # hint` or `word\tanswer  // note` (space before `#` or `//`).

### Dictionary types

**Language pairs** — filename `learned_reference.txt` (first code = language of the **large** word):

| File | Column 1 | Column 2 (large) |
|------|----------|------------------|
| `es_cs.txt` | Czech | Spanish |
| `en_cs.txt` | Czech | English |
| `es_en.txt` | English | Spanish |

**NMS races** — `english<TAB>alien` (column 2 = alien word).

**Topic lists** (monolingual) — e.g. country → capital, element name → symbol:

| File | Column 1 | Column 2 (large) |
|------|----------|------------------|
| `capitals_cs.txt` | země | hlavní město |
| `elements_en.txt` | element name | symbol |

## `index.txt`

One dictionary filename per line (e.g. `gek.txt`). Lines starting with `#` are comments.

Do **not** write `gek.txt // comment` on one line — the firmware only accepts a bare filename (or use a `#` comment on its own line).

## Device config

```c
#define NMS_DICTIONARIES ""              // all files from index.txt on GitHub
#define NMS_DICTIONARIES "es_cs,gek"   // allowlist (max 8), names without .txt
```

Upload `index.txt` and all listed `*.txt` files to the folder pointed to by `NMS_GITHUB_WORDS_BASE_URL` in `nms_config.h` (RAW URL, or `github.com/.../blob/...` — firmware converts it).

Edit `# @…` metadata at the top of each dictionary file by hand when you change phrases or colours. Language pair files are typically **≥1000** data lines; topic lists are smaller (about 100–250).
