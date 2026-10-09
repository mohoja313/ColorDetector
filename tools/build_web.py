#!/usr/bin/env python3
"""
esp-color-detector — web asset builder
======================================
Inlines web/style.css and web/app.js into web/index.html, minifies the
result with a small, conservative (string-aware) minifier, gzips it and
writes src/web_assets.h as a PROGMEM byte array.

Runs automatically as a PlatformIO pre-build script (extra_scripts in
platformio.ini) and can also be invoked directly:

    python3 tools/build_web.py

Pure standard-library Python; no network access required.
"""

import gzip
import io
import os
import sys

# PlatformIO SCons environment provides PROJECT_DIR
try:
    Import("env")
    PROJECT_DIR = env.get("PROJECT_DIR", os.getcwd())
except Exception:
    # Running standalone
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

WEB = os.path.join(PROJECT_DIR, "web")
OUT = os.path.join(PROJECT_DIR, "src", "web_assets.h")


# ── JS: string-aware comment stripper + whitespace collapse ───────────────────
def minify_js(src: str) -> str:
    out = []
    i, n = 0, len(src)
    mode = None  # None | "'" | '"' | "line" | "block"
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if mode is None:
            if c == "/" and nxt == "/":
                mode = "line"
                i += 2
                continue
            if c == "/" and nxt == "*":
                mode = "block"
                i += 2
                continue
            if c in ("'", '"'):
                mode = c
                out.append(c)
                i += 1
                continue
            if c.isspace():
                if out and out[-1] != " ":
                    out.append(" ")
                i += 1
                continue
            out.append(c)
            i += 1
        elif mode == "line":
            if c == "\n":
                mode = None
                if out and out[-1] != " ":
                    out.append(" ")
            i += 1
        elif mode == "block":
            if c == "*" and nxt == "/":
                mode = None
                if out and out[-1] != " ":
                    out.append(" ")
                i += 2
            else:
                i += 1
        else:  # inside a string literal — copy verbatim
            out.append(c)
            if c == "\\" and nxt:
                out.append(nxt)
                i += 2
                continue
            if c == mode:
                mode = None
            i += 1
    res = "".join(out).strip()
    return res


# ── CSS: comment stripper + conservative whitespace squeeze ──────────────────
def minify_css(src: str) -> str:
    out = []
    i, n = 0, len(src)
    mode = None  # None | "block" | "url"
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if mode == "block":
            if c == "*" and nxt == "/":
                mode = None
                i += 2
                if out and out[-1] != " ":
                    out.append(" ")
            else:
                i += 1
            continue
        if mode == "url":
            out.append(c)
            if c == ")":
                mode = None
            i += 1
            continue
        if c == "/" and nxt == "*":
            mode = "block"
            i += 2
            continue
        if c == "u" and src[i : i + 4] == "url(":
            mode = "url"
            out.append(c)
            i += 1
            continue
        if c.isspace():
            if out and out[-1] != " ":
                out.append(" ")
            i += 1
            continue
        if c in "{};,:>":
            if out and out[-1] == " ":
                out.pop()
            out.append(c)
            i += 1
            while i < n and src[i].isspace():
                i += 1
            continue
        out.append(c)
        i += 1
    res = "".join(out).strip()
    return res.replace(";}", "}")


# ── HTML: strip comments + whitespace between tags ───────────────────────────
def minify_html(src: str) -> str:
    import re

    src = re.sub(r"<!--.*?-->", "", src, flags=re.S)
    src = re.sub(r">\s+<", "><", src)
    src = re.sub(r"^\s+|\s+$", "", src, flags=re.M)
    src = re.sub(r"\n{2,}", "\n", src)
    return src.strip()


def build() -> None:
    with open(os.path.join(WEB, "index.html"), encoding="utf-8") as f:
        html = f.read()
    with open(os.path.join(WEB, "style.css"), encoding="utf-8") as f:
        css = f.read()
    with open(os.path.join(WEB, "app.js"), encoding="utf-8") as f:
        js = f.read()

    css_min = minify_css(css)
    js_min = minify_js(js)

    html = html.replace(
        '<link rel="stylesheet" href="style.css">',
        "<style>" + css_min + "</style>",
    )
    html = html.replace(
        '<script src="app.js"></script>', "<script>" + js_min + "</script>"
    )
    html_min = minify_html(html)

    raw = html_min.encode("utf-8")
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as gz:
        gz.write(raw)
    data = buf.getvalue()

    lines = []
    for off in range(0, len(data), 16):
        chunk = data[off : off + 16]
        lines.append(", ".join("0x%02x" % b for b in chunk))
    array = ",\n  ".join(lines)

    header = (
        "// AUTO-GENERATED by tools/build_web.py — DO NOT EDIT BY HAND.\n"
        "// Source: web/index.html + style.css + app.js (minified, gzipped).\n"
        "// Regenerate with: python3 tools/build_web.py\n"
        "#pragma once\n"
        "#include <Arduino.h>\n\n"
        "static const unsigned int INDEX_HTML_GZ_LEN = %d;\n"
        "static const uint8_t INDEX_HTML_GZ[] PROGMEM = {\n  %s\n};\n"
    ) % (len(data), array)

    if os.path.exists(OUT):
        with open(OUT, encoding="utf-8") as f:
            if f.read() == header:
                print("web_assets.h up to date (%d bytes gz)" % len(data))
                return

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(header)

    print(
        "web_assets.h: html %dB + css %dB + js %dB -> minified %dB -> gzipped %dB"
        % (len(html.encode()), len(css.encode()), len(js.encode()), len(raw), len(data))
    )


if __name__ == "__main__":
    build()