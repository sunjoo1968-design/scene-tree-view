#!/usr/bin/env python3
"""Validate data/locale files.

Run from the repository root:

    python tools/check-locales.py
"""

import glob
import io
import os
import re
import unicodedata

MINE = "data/locale"
OBS = ".deps/obs-studio-32.0.2/frontend/data/locale"
SRC = ["src/tree_dock.cpp", "src/tree_dock.h", "src/obs_bridge.cpp", "src/obs_bridge.h", "src/module.cpp"]

problems = []


def note(sev, where, msg):
    problems.append((sev, where, msg))


def parse(path):
    raw = io.open(path, "rb").read()
    issues = []
    if raw.startswith(b"\xef\xbb\xbf"):
        issues.append("file starts with a UTF-8 BOM")
    if b"\xef\xbb\xbf" in raw[3:]:
        issues.append("file contains an embedded UTF-8 BOM")
    if b"\r\n" in raw:
        issues.append("file uses CRLF line endings")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        issues.append(f"not valid UTF-8: {e}")
        return {}, [], issues

    data = {}
    seen = {}
    lines = text.split("\n")
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            issues.append(f"line {i} is not key=value: {stripped[:40]!r}")
            continue
        key, value = stripped.split("=", 1)
        key, value = key.strip(), value.strip()
        if not (value.startswith('"') and value.endswith('"') and len(value) >= 2):
            issues.append(f"{key} value has incomplete quotes: {value[:40]!r}")
        if key in seen:
            issues.append(f"{key} is duplicated on lines {seen[key]} and {i}")
        seen[key] = i
        data[key] = value[1:-1] if len(value) >= 2 else value
    if lines and lines[-1].strip():
        issues.append("file is missing a final newline")
    return data, lines, issues


def width(text):
    return sum(2 if unicodedata.east_asian_width(ch) in "WF" else 1 for ch in text)


files = sorted(glob.glob(MINE + "/*.ini"))
base_path = MINE + "/en-US.ini"
base, _, base_issues = parse(base_path)
for msg in base_issues:
    note("!", "en-US", msg)

print(f"Locale files: {len(files)}; baseline en-US keys: {len(base)}\n")

print("1. Locale IDs vs OBS locale files")
obs_langs = {os.path.basename(path)[:-4] for path in glob.glob(OBS + "/*.ini")}
if obs_langs:
    for path in files:
        lang = os.path.basename(path)[:-4]
        if lang not in obs_langs:
            note("!", lang, "OBS does not ship this locale file")
    all_supported = all(os.path.basename(path)[:-4] in obs_langs for path in files)
    print(f"   OBS locales found: {len(obs_langs)}; all project locales supported: {all_supported}")
else:
    print(f"   OBS locale directory not found at {OBS}; support check skipped")

print("\n2. Key set, structure, and placeholders")
for path in files:
    lang = os.path.basename(path)[:-4]
    data, _, issues = parse(path)
    for msg in issues:
        note("!", lang, msg)
    if lang == "en-US":
        continue
    missing = sorted(set(base) - set(data))
    extra = sorted(set(data) - set(base))
    if missing:
        note("!", lang, f"missing {len(missing)} key(s): {missing[:4]}")
    if extra:
        note("!", lang, f"has {len(extra)} extra key(s): {extra[:4]}")
    for key in set(base) & set(data):
        base_placeholders = sorted(re.findall(r"%\d", base[key]))
        local_placeholders = sorted(re.findall(r"%\d", data[key]))
        if base_placeholders != local_placeholders:
            note("!", lang, f"{key} placeholder mismatch: en={base_placeholders} local={local_placeholders}")
        if not data[key].strip():
            note("!", lang, f"{key} is empty")
print("   See summary below for issues")

print("\n3. Values identical to English")
exempt = {"SceneAnchor.DockTitle", "SceneAnchor.Color.Teal"}
for path in files:
    lang = os.path.basename(path)[:-4]
    if lang == "en-US":
        continue
    data, _, _ = parse(path)
    same = [key for key in set(base) & set(data) if data[key] == base[key] and key not in exempt]
    if same:
        examples = [(key.split(".")[-1], base[key]) for key in sorted(same)[:6]]
        print(f"   {lang}: {len(same)} value(s), e.g. {examples}")

print("\n4. Ellipsis style")
base_ellipsis_count = len([key for key, value in base.items() if "..." in value or "\u2026" in value])
for path in files:
    lang = os.path.basename(path)[:-4]
    data, _, _ = parse(path)
    ascii_ellipsis = [key for key, value in data.items() if "..." in value]
    unicode_ellipsis = [key for key, value in data.items() if "\u2026" in value]
    if ascii_ellipsis:
        note("!", lang, f"uses three dots instead of an ellipsis character: {ascii_ellipsis}")
    if lang != "en-US" and len(unicode_ellipsis) != base_ellipsis_count:
        note("~", lang, f"ellipsis key count differs from en-US ({len(unicode_ellipsis)} vs {base_ellipsis_count})")

print("\n5. Narrow dock string length check")
watch = {
    "SceneAnchor.Search": 30,
    "SceneAnchor.EmptyHint": 130,
    "SceneAnchor.DockTitle": 24,
}
for key, limit in watch.items():
    row = []
    for path in files:
        lang = os.path.basename(path)[:-4]
        data, _, _ = parse(path)
        if key in data:
            row.append((lang, width(data[key])))
    row.sort(key=lambda item: -item[1])
    over = [f"{lang}={value}" for lang, value in row if value > limit]
    if row:
        print(f"   {key.split('.')[-1]:<12} limit {limit:>4}; longest {row[0][0]}={row[0][1]}; "
              f"{'over: ' + ', '.join(over) if over else 'all within limit'}")

print("\n6. Code references vs locale definitions")
used = set()
for path in SRC:
    text = io.open(path, encoding="utf-8").read()
    used |= set(re.findall(r'obs_module_text\(\s*"([^"]+)"', text))
    used |= {"SceneAnchor.Color." + name for name in ["Red", "Orange", "Yellow", "Green",
                                                      "Teal", "Blue", "Purple", "Magenta"]}
undef = sorted(used - set(base))
unused = sorted(set(base) - used)
print(f"   referenced keys: {len(used)}; undefined: {len(undef)}; unused: {unused if unused else 'none'}")
if undef:
    note("!", "code", f"undefined referenced keys: {undef}")
for key in unused:
    note("~", "en-US", f"{key} is defined but not referenced by code")

print("\n" + "=" * 60)
if not problems:
    print("All checks passed")
else:
    hard = [item for item in problems if item[0] == "!"]
    soft = [item for item in problems if item[0] == "~"]
    print(f"Hard issues: {len(hard)}; notes: {len(soft)}")
    for sev, where, msg in hard + soft:
        print(f"   [{'error' if sev == '!' else 'note'}] {where}: {msg}")
