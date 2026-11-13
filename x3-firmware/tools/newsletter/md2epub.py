#!/usr/bin/env python3
"""Newsletter (light markdown) -> EPUB for the Xteink X3 reader.

The Bülten routine writes each newsletter as markdown (text kept verbatim):

    ---
    baslik: SİNYAL #08 · Hasat: Döngüyü Tamamlamak
    gonderen: Neurospicegirl
    tarih: 2026-06-06
    web: https://neurospicegirl.substack.com/p/...
    ---
    ## 01 · SİNYAL
    Paragraph text...
    - list item

Usage: md2epub.py <in.md> <out.epub>
Standard library only. Drops unsubscribe/tracking lines and bare long URLs;
the result has no fonts or images, so it opens fast on the X3.
"""
import html
import re
import sys
import uuid
import xml.dom.minidom
import zipfile

DROP = re.compile(r"(unsubscribe|abonelikten çık|manage (your )?subscription|view this post on the web|"
                  r"web'de görüntüle|substack\.com/redirect|list-manage\.com|email preferences)", re.I)
URL = re.compile(r"https?://\S+")


def parse(text):
    meta, body = {}, text
    m = re.match(r"﻿?---\s*\n(.*?)\n---\s*\n", text, re.S)
    if m:
        for line in m.group(1).splitlines():
            if ":" in line:
                k, v = line.split(":", 1)
                meta[k.strip().lower()] = v.strip()
        body = text[m.end():]
    return meta, body


def emphasis(s):
    """**bold** and *italic* (also ***both*** and runs like *****) with the
    tags always properly nested, so the XHTML stays well formed."""
    parts = re.split(r"(\*+)", s)
    out, stack = [], []
    for i, part in enumerate(parts):
        if not part.startswith("*") or not part:
            out.append(part)
            continue
        before = parts[i - 1][-1:] if i > 0 else ""
        after = parts[i + 1][:1] if i + 1 < len(parts) else ""
        left_space = before == "" or before.isspace()
        right_space = after == "" or after.isspace()
        if left_space and right_space:  # a lone "*" between spaces is text
            out.append(part)
            continue
        n = len(part)
        closed = False
        if stack and not left_space:  # closes what is open, innermost first
            while n > 0 and stack:
                need = 2 if stack[-1] == "b" else 1
                if n < need:
                    break
                out.append("</%s>" % stack.pop())
                n -= need
                closed = True
        # Opens unless the run sits inside a word ("5*3"), as in Markdown.
        can_open = not right_space and (left_space or not before.isalnum() or closed)
        while n > 0 and can_open:  # opens
            if n >= 2 and "b" not in stack:
                stack.append("b"); out.append("<b>"); n -= 2
            elif "i" not in stack:
                stack.append("i"); out.append("<i>"); n -= 1
            else:
                break
        if n > 0:
            out.append("*" * n)
    while stack:
        out.append("</%s>" % stack.pop())
    return "".join(out)


def inline(s):
    s = html.escape(s, quote=False)
    s = re.sub(r"\[([^\]]+)\]\(https?://[^)]+\)", r"\1", s)  # keep link text, drop the URL
    return emphasis(s)


def body_html(body):
    out, in_list = [], False
    for raw in body.splitlines():
        line = raw.strip()
        if not line or DROP.search(line):
            if in_list:
                out.append("</ul>")
                in_list = False
            continue
        if URL.fullmatch(line):  # a bare URL on its own line: noise on paper
            continue
        if line.startswith(("- ", "* ", "• ")):
            if not in_list:
                out.append("<ul>")
                in_list = True
            out.append("<li>%s</li>" % inline(line[2:].strip()))
            continue
        if in_list:
            out.append("</ul>")
            in_list = False
        h = re.match(r"(#{1,3})\s+(.*)", line)
        if h:
            level = 3 if len(h.group(1)) >= 3 else 2  # "#"/"##" -> h2 (h1 is the title), "###" -> h3
            out.append("<h%d>%s</h%d>" % (level, inline(h.group(2)), level))
        else:
            out.append("<p>%s</p>" % inline(line))
    if in_list:
        out.append("</ul>")
    return "\n".join(out)


def guess_lang(text):
    return "tr" if re.search(r"[çğışöüÇĞİŞÖÜ]", text) else "en"


def build(md, out_path):
    meta, body = parse(md)
    title = meta.get("baslik") or "Bülten"
    sender = meta.get("gonderen", "")
    date = meta.get("tarih", "")
    web = meta.get("web", "")
    lang = meta.get("dil") or guess_lang(title + body)
    byline = " · ".join(x for x in (sender, date) if x)
    chapter = ('<?xml version="1.0" encoding="utf-8"?>\n'
               '<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="%s"><head><title>%s</title></head><body>\n'
               '<h1>%s</h1>\n%s\n%s\n%s\n</body></html>\n') % (
        lang, html.escape(title), html.escape(title),
        "<p><i>%s</i></p>" % html.escape(byline) if byline else "",
        body_html(body),
        "<p><i>Web'de: %s</i></p>" % html.escape(web) if web else "")
    ident = "urn:uuid:%s" % uuid.uuid5(uuid.NAMESPACE_URL, web or (title + date))
    opf = ('<?xml version="1.0" encoding="utf-8"?>\n'
           '<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">'
           '<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
           '<dc:identifier id="id">%s</dc:identifier><dc:title>%s</dc:title><dc:creator>%s</dc:creator>'
           '<dc:language>%s</dc:language><dc:date>%s</dc:date>'
           '<meta property="dcterms:modified">2026-01-01T00:00:00Z</meta></metadata>'
           '<manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
           '<item id="c1" href="bulten.xhtml" media-type="application/xhtml+xml"/></manifest>'
           '<spine><itemref idref="c1"/></spine></package>\n') % (
        ident, html.escape(title), html.escape(sender or "Bülten"), lang, html.escape(date))
    nav = ('<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml" '
           'xmlns:epub="http://www.idpf.org/2007/ops"><head><title>%s</title></head><body>'
           '<nav epub:type="toc"><ol><li><a href="bulten.xhtml">%s</a></li></ol></nav></body></html>\n') % (
        html.escape(title), html.escape(title))
    container = ('<?xml version="1.0"?><container version="1.0" '
                 'xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>'
                 '<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>'
                 '</rootfiles></container>\n')
    # The device rejects malformed XHTML: refuse to write a broken book.
    for name, data in (("content.opf", opf), ("nav.xhtml", nav), ("bulten.xhtml", chapter)):
        try:
            xml.dom.minidom.parseString(data.encode("utf-8"))
        except Exception as e:  # noqa: BLE001 - any parser error means a broken book
            raise SystemExit("md2epub: %s is not well-formed XML (%s)" % (name, e))
    with zipfile.ZipFile(out_path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        for name, data in (("META-INF/container.xml", container), ("OEBPS/content.opf", opf),
                           ("OEBPS/nav.xhtml", nav), ("OEBPS/bulten.xhtml", chapter)):
            z.writestr(name, data.encode("utf-8"), compress_type=zipfile.ZIP_DEFLATED)


def file_name(meta):
    """Name for kitaplar/: '2026-06-06 SİNYAL #08 · Hasat.epub' (FAT-safe, <= 79 bytes: the shelf keeps 80 with the NUL)."""
    title = re.sub(r'[\\/:*?"<>|]', "-", meta.get("baslik") or "Bülten").strip()
    name = ("%s %s" % (meta.get("tarih", ""), title)).strip()
    while len((name + ".epub").encode("utf-8")) > 79:
        name = name[:-1]
    return name.rstrip(" .-·") + ".epub"


if __name__ == "__main__":
    if len(sys.argv) == 2:  # print the file name the routine should use
        print(file_name(parse(open(sys.argv[1], encoding="utf-8").read())[0]))
    elif len(sys.argv) == 3:
        build(open(sys.argv[1], encoding="utf-8").read(), sys.argv[2])
    else:
        sys.exit(__doc__)
