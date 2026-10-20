#!/usr/bin/env python3
"""Converts docs/nexus/page.md to Nexus Mods BBCode (docs/nexus/page.bbcode).

Usage: python3 scripts/nexus-bbcode.py [page.md] [page.bbcode]
Handles the Markdown the page uses: headings, paragraphs, nested lists, tables (as lists; Nexus has no tables),
code blocks, links, bold, and inline code.
"""
import re, sys
src = sys.argv[1] if len(sys.argv) > 1 else "docs/nexus/page.md"
dst = sys.argv[2] if len(sys.argv) > 2 else "docs/nexus/page.bbcode"
lines = open(src, encoding="utf-8").read().splitlines()

KEY_WORDS = {"`[`": "Left Bracket", "`]`": "Right Bracket", "`\\`": "Backslash"}

def inline(t):
    # Literal [ and ] would read as BBCode tags; name the bracket keys instead.
    t = re.sub(r"`\[(Overlay|Playback|Hotkeys)\] ", r"`\1 / ", t)  # INI sections: "[Overlay] Corner" -> "Overlay / Corner"
    for code, words in KEY_WORDS.items():
        t = t.replace(code, "[b]" + words + "[/b]")
    t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"[url=\2]\1[/url]", t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"[b]\1[/b]", t)
    t = re.sub(r"`([^`]+)`", r"[font=Courier New]\1[/font]", t)
    return t

out, i, stack = [], 0, []  # stack of (indent, kind)
def close_lists(indent=-1):
    while stack and stack[-1][0] > indent:
        stack.pop(); out.append("[/list]")

while i < len(lines):
    line = lines[i]
    if line.startswith("```"):
        close_lists(); block = []; i += 1
        while not lines[i].startswith("```"):
            block.append(lines[i]); i += 1
        out.append("[code]" + "\n".join(block) + "[/code]"); i += 1; continue
    if line.startswith("|"):
        close_lists(); rows = []
        while i < len(lines) and lines[i].startswith("|"):
            cells = [c.strip() for c in lines[i].strip("|").split("|")]
            if not all(re.fullmatch(r"-+", c) for c in cells if c):
                rows.append(cells)
            i += 1
        out.append("[list]")
        for cells in rows[1:]:
            parts = [inline(c) for c in cells if c]
            out.append("[*][b]" + parts[0] + "[/b]" + (" - " + " - ".join(parts[1:]) if len(parts) > 1 else ""))
        out.append("[/list]"); continue
    m = re.match(r"^(\s*)([-*]|\d+\.) (.*)", line)
    if m:
        indent, kind = len(m.group(1)), ("1" if m.group(2)[0].isdigit() else "*")
        text = m.group(3)
        while i + 1 < len(lines) and lines[i + 1].startswith(" " * (indent + 2)) and lines[i + 1].strip() and not re.match(r"^\s*([-*]|\d+\.) ", lines[i + 1]):
            i += 1; text += " " + lines[i].strip()
        if not stack or stack[-1][0] < indent:
            out.append("[list=1]" if kind == "1" else "[list]"); stack.append((indent, kind))
        else:
            close_lists(indent)
        out.append("[*]" + inline(text)); i += 1; continue
    close_lists()
    h = re.match(r"^(#+) (.*)", line)
    if h:
        size = {1: 5, 2: 4, 3: 3}.get(len(h.group(1)), 3)
        out.append(f"[size={size}][b]{inline(h.group(2))}[/b][/size]"); i += 1; continue
    if line.strip() == "":
        out.append(""); i += 1; continue
    para = [line.strip()]
    while i + 1 < len(lines) and lines[i + 1].strip() and not re.match(r"^(#|\||```|\s*([-*]|\d+\.) )", lines[i + 1]):
        i += 1; para.append(lines[i].strip())
    out.append(inline(" ".join(para))); i += 1
close_lists()
text = re.sub(r"\n{3,}", "\n\n", "\n".join(out)).strip() + "\n"
text = re.sub(r"\[b\]\[b\](.*?)\[/b\]\[/b\]", r"[b]\1[/b]", text)
open(dst, "w", encoding="utf-8").write(text)
