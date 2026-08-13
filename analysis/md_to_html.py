#!/usr/bin/env python3
"""Minimal Markdown -> HTML converter (no dependencies).

Handles what our reports use: #/##/### headers, pipe tables, **bold**,
`inline code`, --- rules, and plain paragraphs.

Usage: python3 md_to_html.py <input.md> [output.html]
"""
import html
import re
import sys


def inline(s):
    s = html.escape(s, quote=False)
    s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"`(.+?)`", r"<code>\1</code>", s)
    return s


CSS = """
body { font-family: -apple-system, "Segoe UI", sans-serif; max-width: 1100px;
       margin: 2rem auto; padding: 0 1rem; color: #1f2328; line-height: 1.5; }
table { border-collapse: collapse; margin: 1rem 0; font-size: 0.9rem; }
th, td { border: 1px solid #d1d9e0; padding: 4px 10px; text-align: left; }
th { background: #f6f8fa; position: sticky; top: 0; }
tr:nth-child(even) { background: #fafbfc; }
code { background: #f6f8fa; padding: 1px 5px; border-radius: 4px;
       font-size: 0.85em; }
h1, h2, h3 { border-bottom: 1px solid #d8dee4; padding-bottom: 0.3em; }
hr { border: none; border-top: 2px solid #d8dee4; margin: 2rem 0; }
"""


def convert(md_text, title):
    lines = md_text.splitlines()
    out = []
    in_table = False
    for line in lines:
        stripped = line.strip()
        is_row = stripped.startswith("|") and stripped.endswith("|")
        if in_table and not is_row:
            out.append("</table>")
            in_table = False
        if is_row:
            cells = [c.strip() for c in stripped.strip("|").split("|")]
            if all(re.fullmatch(r":?-{3,}:?", c) for c in cells):
                continue  # separator row
            tag = "td" if in_table else "th"
            if not in_table:
                out.append("<table>")
                in_table = True
            out.append("<tr>" + "".join(f"<{tag}>{inline(c)}</{tag}>" for c in cells) + "</tr>")
        elif stripped.startswith("#"):
            level = len(stripped) - len(stripped.lstrip("#"))
            out.append(f"<h{level}>{inline(stripped[level:].strip())}</h{level}>")
        elif re.fullmatch(r"-{3,}", stripped):
            out.append("<hr>")
        elif stripped:
            out.append(f"<p>{inline(stripped)}</p>")
    if in_table:
        out.append("</table>")
    return (f"<!doctype html><html><head><meta charset='utf-8'>"
            f"<title>{html.escape(title)}</title><style>{CSS}</style></head>"
            f"<body>\n" + "\n".join(out) + "\n</body></html>")


def main():
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else re.sub(r"\.md$", "", src) + ".html"
    with open(src) as f:
        md_text = f.read()
    title = next((l.lstrip("# ").strip() for l in md_text.splitlines()
                  if l.startswith("#")), src)
    with open(dst, "w") as f:
        f.write(convert(md_text, title))
    print(f"wrote {dst}")


if __name__ == "__main__":
    main()
