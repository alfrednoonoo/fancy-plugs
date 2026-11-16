# Builds the Fancy Plugs manual PDF: HTML printed by headless Edge, then page backgrounds, the
# running title strip, page numbers and bookmarks added with reportlab and pypdf.
#   py -3.14 build.py <repo> <out.pdf>

import html
import json
import os
import re
import subprocess
import sys

import markdown
from pypdf import PdfReader, PdfWriter
from reportlab.lib.colors import HexColor
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

sys.path.insert(0, os.path.dirname(__file__))
import content as C

REPO = sys.argv[1]
OUT = sys.argv[2]
SCREENS = sys.argv[3]
WORK = os.path.join(os.path.dirname(__file__), "work")
os.makedirs(WORK, exist_ok=True)
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
FONTS = os.path.join(REPO, "assets", "fonts")
REF = json.load(open(os.path.join(os.path.dirname(__file__), "ref2.json"), encoding="utf-8"))

BG_PAGE = "#303030"
BG_COVER = "#1E1E1E"
STRIP = "#141414"
LINE = "#262626"
SECONDARY = "#8D8D8D"

ORDER = [name for _, _, names in C.PARTS for name in names]


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def marker(key):
    # Invisible text the second pass finds, to learn which page each section starts on.
    return f'<span class="mk">@@{key}@@</span>'


def fix_range(text):
    if not text:
        return ""
    text = text.replace("`", "")
    text = re.sub(r"(?<=[\dfF%])-(?=[\d+\-]|inf)", "–", text)
    text = re.sub(r"(?<=inf)-(?=[\d+])", "–", text)
    return text


def inline_md(text):
    out = markdown.markdown(text or "")
    return re.sub(r"^<p>|</p>$", "", out.strip())


def clean_manual(md_text, plugin):
    lines = md_text.splitlines()
    if lines and lines[0].startswith("# "):
        lines = lines[1:]
    out, skip = [], False
    for line in lines:
        if line.startswith("## "):
            skip = line[3:].strip() in C.DROP_SECTIONS
        if not skip:
            out.append(line)
    text = "\n".join(out)
    # Developer references don't belong in a user manual.
    text = re.sub(r"\s*Design: `docs/[^`]+`\.", "", text)
    text = re.sub(r"\s*\((?:see |in )?`docs/[^`]+`[^)]*\)", "", text)
    text = re.sub(r"`docs/[^`]+`", "the design notes", text)
    return text


def manual_html(plugin, sections):
    meta = C.PLUGINS[plugin]
    md_text = open(os.path.join(REPO, "docs", f"{meta['file']}-manual.md"), encoding="utf-8").read()
    body = markdown.markdown(clean_manual(md_text, plugin), extensions=["tables", "sane_lists"])

    # Chapter sections: h2 -> h3 with anchors; h3 -> h4.
    body = body.replace("<h3>", "<h4>").replace("</h3>", "</h4>")

    def h2(match):
        title = re.sub(r"<[^>]+>", "", match.group(1))
        anchor = f"{slug(plugin)}-{slug(title)}"
        sections.append((title, anchor))
        return f'<h3 id="{anchor}">{match.group(1)}</h3>'

    body = re.sub(r"<h2>(.*?)</h2>", h2, body)
    return body


def reference_table(plugin):
    rows = []
    per = {"Delay": "each tap", "EQ": "each band", "MultiComp": "each band"}.get(plugin, "")
    for item in REF[plugin]:
        name = item["name"]
        default = (item["default"] or "").replace("\ufffd", "×")
        rng = C.RANGE.get(plugin, {}).get(name) or item.get("range") or ""
        unit = (item.get("unit") or "").strip()
        if unit and name not in C.RANGE.get(plugin, {}) and not re.search(r"[A-Za-z%]", re.sub(r"inf|bottom|top|Off|off|on|is", "", rng)):
            head, sep, tail = rng.partition(",")
            rng = f"{head.strip()} {unit}{sep}{tail}"
        desc = C.DESC.get(plugin, {}).get(name) or item.get("desc") or C.GENERIC.get(name, "")
        scope = f'<span class="scope">{per}</span>' if item.get("repeat") and per else ""
        rows.append(f"<tr><td class='n'>{html.escape(name)}{scope}</td><td>{html.escape(fix_range(rng))}</td>"
                    f"<td>{html.escape(default)}</td><td>{inline_md(desc)}</td></tr>")
    return ("<table class='ref'><thead><tr><th>Control</th><th>Range</th><th>Default</th><th>What it does</th></tr></thead>"
            f"<tbody>{''.join(rows)}</tbody></table>")


def shot(name):
    return "file:///" + os.path.join(SCREENS, name + "@2x.png").replace(os.sep, "/")


def more_views(plugin, narrow):
    items = C.MORE.get(plugin, [])
    if not items:
        return ""
    figs = "".join(f'<figure><img src="{shot(f)}"><figcaption>{html.escape(cap)}</figcaption></figure>' for f, cap in items)
    return f'<div class="more {"narrow" if narrow else ""}">{figs}</div>'


def palette_bar(meta):
    colours = meta.get("palette") or [meta["accent"]]
    if len(colours) == 1:
        return f'<div class="bar"><i style="background:{colours[0]};width:46mm"></i></div>'
    return '<div class="bar">' + "".join(f'<i style="background:{c}"></i>' for c in colours) + "</div>"


def knob_svg(size, value, colour, track="#4F4F4F"):
    # The suite's knob: 300 degrees of travel from 7 o'clock, the arc lit from the start.
    import math
    cx = cy = size / 2
    r = size * 0.42
    start = math.radians(210)
    end = start - math.radians(300 * value)

    def point(angle):
        return cx + r * math.cos(angle), cy - r * math.sin(angle)

    def arc(a0, a1):
        x0, y0 = point(a0)
        x1, y1 = point(a1)
        large = 1 if abs(a1 - a0) > math.pi else 0
        return f"M{x0:.2f},{y0:.2f} A{r:.2f},{r:.2f} 0 {large} 1 {x1:.2f},{y1:.2f}"

    px, py = cx + r * 0.62 * math.cos(end), cy - r * 0.62 * math.sin(end)
    return (f'<svg width="{size}" height="{size}" viewBox="0 0 {size} {size}">'
            f'<path d="{arc(start, start - math.radians(300))}" stroke="{track}" stroke-width="{size*0.035:.2f}" fill="none" stroke-linecap="round"/>'
            f'<path d="{arc(start, end)}" stroke="{colour}" stroke-width="{size*0.035:.2f}" fill="none" stroke-linecap="round"/>'
            f'<circle cx="{cx}" cy="{cy}" r="{r*0.74:.2f}" fill="url(#face)" stroke="rgba(0,0,0,.2)"/>'
            f'<line x1="{cx}" y1="{cy}" x2="{px:.2f}" y2="{py:.2f}" stroke="#FFFFFF" stroke-width="{size*0.02:.2f}" stroke-linecap="round"/>'
            '<defs><linearGradient id="face" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4F4F4F"/>'
            '<stop offset="1" stop-color="#454545"/></linearGradient></defs></svg>')


def build_html(page_numbers):
    def num(key):
        return str(page_numbers.get(key, "")) if page_numbers else "00"

    chapters = []  # (key, title, html)
    toc_items = []

    # Shared chapters.
    for key, title, md_text, lede in [
        ("first-steps", "First steps", C.FIRST_STEPS, "What a plugin is, how to install these, and how to read their levels."),
        ("the-window", "Every plugin's window", C.WINDOW, "The header, the gestures and the switches all fourteen plugins share."),
    ]:
        sections = []
        body = markdown.markdown(md_text, extensions=["tables", "sane_lists"])

        def h2(match, sections=sections, key=key):
            title_text = re.sub(r"<[^>]+>", "", match.group(1))
            anchor = f"{key}-{slug(title_text)}"
            sections.append((title_text, anchor))
            return f'<h3 id="{anchor}">{match.group(1)}</h3>'

        body = re.sub(r"<h2>(.*?)</h2>", h2, body)
        chapters.append(f'''
<section class="chapter plain" id="{key}">
  {marker("ch:" + key)}
  <div class="eyebrow">FANCY PLUGS</div>
  <h2>{title}</h2>
  <p class="lede">{lede}</p>
  <div class="bar"><i style="background:#00A985;width:46mm"></i></div>
  <div class="prose">{body}</div>
</section>''')
        toc_items.append(("chapter", key, title, lede, sections))

    for part_title, part_lede, names in C.PARTS:
        part_key = "part-" + slug(part_title)
        roster = "".join(
            f'<li><a href="#{slug(n)}"><span style="color:{C.PLUGINS[n]["accent"]}">Fancy {n}</span>'
            f'<em>{C.PLUGINS[n]["kind"]}</em></a></li>' for n in names)
        chapters.append(f'''
<section class="part" id="{part_key}">
  {marker("ch:" + part_key)}
  <div class="eyebrow">PART</div>
  <h2>{part_title}</h2>
  <p class="lede">{part_lede}</p>
  <ul class="roster">{roster}</ul>
</section>''')
        toc_items.append(("part", part_key, part_title, part_lede, []))

        for name in names:
            meta = C.PLUGINS[name]
            key = slug(name)
            sections = [("Quick start", f"{key}-quick-start")]
            body = manual_html(name, sections)
            intro = ""
            first = re.match(r"\s*(<p>.*?</p>)", body, re.S)
            if first:
                intro, body = first.group(1), body[first.end():]
            if C.MORE.get(name):
                sections.append(("More views", f"{key}-more-views"))
            sections.append(("Control reference", f"{key}-control-reference"))
            imgs = meta["images"]
            narrow = meta.get("narrow")
            figures = "".join(
                f'<figure class="{"half" if narrow else "full"}"><img src="{shot(f)}">'
                f"<figcaption>{html.escape(cap)}</figcaption></figure>" for f, cap in imgs)
            steps = "".join(f"<li><span>{i + 1}</span><p>{inline_md(s)}</p></li>" for i, s in enumerate(meta["quick"]))
            chapters.append(f'''
<section class="chapter" id="{key}" style="--accent:{meta["accent"]}">
  {marker("ch:" + key)}
  <div class="eyebrow">FANCY <b>{html.escape(name.upper())}</b></div>
  <h2>Fancy {html.escape(name)}</h2>
  <p class="lede">{html.escape(meta["kind"])}</p>
  {palette_bar(meta)}
  <div class="figures {"pair" if narrow else ""}">{figures}</div>
  <div class="intro">{intro}</div>
  <h3 id="{key}-quick-start">Quick start</h3>
  <ol class="quick">{steps}</ol>
  <div class="prose">{body}</div>
  {f'<h3 id="{key}-more-views" class="morehead">More views</h3>' + more_views(name, narrow) if C.MORE.get(name) else ""}
  <h3 id="{key}-control-reference" class="refhead">Control reference</h3>
  <p class="note">Every control, by the name your host shows for automation. Defaults are those of a new instance.</p>
  {reference_table(name)}
</section>''')
            toc_items.append(("plugin", key, f"Fancy {name}", meta["kind"], sections))

    # Glossary and credits.
    gl = "".join(f"<dt>{html.escape(t)}</dt><dd>{html.escape(d)}</dd>" for t, d in C.GLOSSARY)
    chapters.append(f'''
<section class="chapter plain" id="glossary">
  {marker("ch:glossary")}
  <div class="eyebrow">FANCY PLUGS</div>
  <h2>Glossary</h2>
  <p class="lede">The words this manual uses, in plain terms.</p>
  <div class="bar"><i style="background:#00A985;width:46mm"></i></div>
  <dl class="gloss">{gl}</dl>
  <h3 id="credits">Credits</h3>
  <p>Fancy Plugs are original effects built with JUCE. The user-interface design is based on "ReaPlugs (vst GUI)" by Eugene Reznik, licensed CC BY 4.0, and modified from the original. Type is Inter, by Rasmus Andersson, under the SIL Open Font License.</p>
</section>''')
    toc_items.append(("chapter", "glossary", "Glossary and credits", "", []))

    # Contents.
    toc_rows = []
    for kind, key, title, lede, sections in toc_items:
        if kind == "part":
            toc_rows.append(f'<div class="toc-part"><a href="#{key}">{html.escape(title)}</a><span>{num("ch:" + key)}</span></div>')
            continue
        accent = C.PLUGINS[title.replace("Fancy ", "")]["accent"] if kind == "plugin" else "#00A985"
        subs = " · ".join(f'<a href="#{a}">{html.escape(t)}</a>' for t, a in sections)
        toc_rows.append(
            f'<div class="toc-row"><a class="t" href="#{key}"><i style="background:{accent}"></i>{html.escape(title)}'
            f'<em>{html.escape(lede if kind == "plugin" else "")}</em></a><span>{num("ch:" + key)}</span></div>'
            + (f'<div class="toc-subs">{subs}</div>' if subs else ""))

    accents = []
    for name in ORDER:
        meta = C.PLUGINS[name]
        accents.append(f'<span style="color:{meta["accent"]}">{name.upper()}</span>')

    cover = f'''
<section class="cover">
  {marker("cover")}
  <div class="cover-knob">{knob_svg(260, 0.72, "#00A985")}</div>
  <div class="cover-text">
    <div class="eyebrow">FANCY PLUGS · MANUAL</div>
    <h1>Fancy Plugs</h1>
    <p class="cover-sub">Fourteen effects, every control explained.</p>
    <p class="cover-ver">Version {C.VERSION} · VST3 and Audio Unit · Windows and macOS</p>
  </div>
  <div class="cover-roster">{" ".join(accents)}</div>
</section>'''

    toc = f'''
<section class="toc" id="contents">
  {marker("toc")}
  <div class="eyebrow">FANCY PLUGS</div>
  <h2>Contents</h2>
  <div class="toc-list">{"".join(toc_rows)}</div>
</section>'''

    font_face = "".join(
        f"@font-face{{font-family:Inter;src:url('file:///{os.path.join(FONTS, f).replace(os.sep, '/')}');font-weight:{w}}}"
        for f, w in [("Inter-Regular.ttf", 400), ("Inter-Medium.ttf", 500), ("Inter-SemiBold.ttf", 600)])
    css = open(os.path.join(os.path.dirname(__file__), "manual.css"), encoding="utf-8").read()
    return f"<!doctype html><html><head><meta charset='utf-8'><title>Fancy Plugs manual</title><style>{font_face}{css}</style></head><body>{cover}{toc}{''.join(chapters)}</body></html>"


def render(html_text, name):
    path = os.path.join(WORK, name + ".html")
    pdf = os.path.join(WORK, name + ".pdf")
    open(path, "w", encoding="utf-8").write(html_text)
    if os.path.exists(pdf):
        os.remove(pdf)
    subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--print-to-pdf=" + pdf,
                    "file:///" + path.replace(os.sep, "/")], check=True, capture_output=True, timeout=600)
    return pdf


def page_map(pdf):
    found = {}
    for i, page in enumerate(PdfReader(pdf).pages):
        for key in re.findall(r"@@(.+?)@@", page.extract_text() or ""):
            found.setdefault(key, i + 1)
    return found


def finish(pdf, pages):
    pdfmetrics.registerFont(TTFont("Inter", os.path.join(FONTS, "Inter-Regular.ttf")))
    pdfmetrics.registerFont(TTFont("Inter-SemiBold", os.path.join(FONTS, "Inter-SemiBold.ttf")))
    reader = PdfReader(pdf)
    n = len(reader.pages)

    # Which chapter each page belongs to, for the title strip.
    starts = sorted((p, k[3:]) for k, p in pages.items() if k.startswith("ch:"))

    def chapter_of(page):
        current = None
        for p, k in starts:
            if p <= page:
                current = k
        return current

    labels = {"first-steps": ("PLUGS", "#00A985"), "the-window": ("PLUGS", "#00A985"), "glossary": ("PLUGS", "#00A985")}
    for name in ORDER:
        labels[slug(name)] = (name.upper(), C.PLUGINS[name]["accent"])
    for part_title, _, _ in C.PARTS:
        labels["part-" + slug(part_title)] = ("PLUGS", "#00A985")

    under = os.path.join(WORK, "under.pdf")
    w, h = reader.pages[0].mediabox.width, reader.pages[0].mediabox.height
    c = canvas.Canvas(under, pagesize=(float(w), float(h)))
    for i in range(1, n + 1):
        cover = i == pages.get("cover", 1)
        c.setFillColor(HexColor(BG_COVER if cover else BG_PAGE))
        c.rect(0, 0, float(w), float(h), stroke=0, fill=1)
        if not cover:
            # The plugins' title strip, across the top of every page.
            strip_h = 20
            c.setFillColor(HexColor(STRIP))
            c.rect(0, float(h) - strip_h, float(w), strip_h, stroke=0, fill=1)
            c.setFillColor(HexColor(LINE))
            c.rect(0, float(h) - strip_h - 1, float(w), 1, stroke=0, fill=1)
            name, colour = labels.get(chapter_of(i), ("PLUGS", "#00A985"))
            c.setFont("Inter-SemiBold", 7)
            c.setFillColor(HexColor(SECONDARY))
            x = 51
            c.drawString(x, float(h) - 13, "F A N C Y")
            c.setFillColor(HexColor(colour))
            c.drawString(x + pdfmetrics.stringWidth("F A N C Y", "Inter-SemiBold", 7) + 6, float(h) - 13, " ".join(name))
            c.setFillColor(HexColor(SECONDARY))
            c.setFont("Inter", 7.5)
            c.drawRightString(float(w) - 51, 26, str(i))
            c.drawString(51, 26, f"Fancy Plugs {C.VERSION} manual")
        c.showPage()
    c.save()

    bg = PdfReader(under)
    writer = PdfWriter()
    for i, page in enumerate(reader.pages):
        writer.add_page(bg.pages[i])
        writer.pages[i].merge_page(page)

    # Links: Edge writes named destinations, which merging drops, so point each link straight at
    # its target page and height instead.
    from pypdf.generic import ArrayObject, DictionaryObject, FloatObject, NameObject, NullObject, NumberObject
    named = reader.named_destinations
    link_count = 0
    for i, page in enumerate(reader.pages):
        annots = ArrayObject()
        for ref in page.get("/Annots", []):
            a = ref.get_object()
            if a.get("/Subtype") != "/Link" or a.get("/Dest") is None:
                continue
            key = str(a["/Dest"])
            dest = named.get(key) or named.get(key.lstrip("/"))
            if dest is None:
                continue
            target = reader.get_destination_page_number(dest)
            top = dest.top if dest.top is not None else float(h)
            link = DictionaryObject({
                NameObject("/Type"): NameObject("/Annot"),
                NameObject("/Subtype"): NameObject("/Link"),
                NameObject("/Rect"): ArrayObject([FloatObject(float(x)) for x in a["/Rect"]]),
                NameObject("/Border"): ArrayObject([NumberObject(0), NumberObject(0), NumberObject(0)]),
                NameObject("/Dest"): ArrayObject([writer.pages[target].indirect_reference, NameObject("/XYZ"),
                                                  NullObject(), FloatObject(float(top)), NullObject()]),
            })
            annots.append(writer._add_object(link))
            link_count += 1
        if "/Annots" in writer.pages[i]:
            del writer.pages[i]["/Annots"]
        if annots:
            writer.pages[i][NameObject("/Annots")] = annots
    print("links", link_count)

    # Bookmarks.
    outline = [("Contents", pages.get("toc")), ("First steps", pages.get("ch:first-steps")),
               ("Every plugin's window", pages.get("ch:the-window"))]
    for o_title, o_page in outline:
        if o_page:
            writer.add_outline_item(o_title, o_page - 1)
    for part_title, _, names in C.PARTS:
        parent = writer.add_outline_item(part_title, pages["ch:part-" + slug(part_title)] - 1)
        for name in names:
            writer.add_outline_item(f"Fancy {name}", pages["ch:" + slug(name)] - 1, parent=parent)
    writer.add_outline_item("Glossary and credits", pages["ch:glossary"] - 1)
    writer.add_metadata({"/Title": f"Fancy Plugs {C.VERSION}: manual", "/Author": "Fancy Plugs",
                         "/Subject": "User manual for the fourteen Fancy Plugs effects"})
    writer.page_mode = "/UseOutlines"
    with open(OUT, "wb") as f:
        writer.write(f)
    return n


def pypdf_name(text):
    from pypdf.generic import NameObject
    return NameObject(text)


if __name__ == "__main__":
    first = render(build_html(None), "pass1")
    pages1 = page_map(first)
    second = render(build_html(pages1), "pass2")
    pages2 = page_map(second)
    if pages1 != pages2:
        print("page map moved between passes; rendering a third time")
        second = render(build_html(pages2), "pass3")
        pages2 = page_map(second)
    total = finish(second, pages2)
    print(json.dumps({k: v for k, v in sorted(pages2.items(), key=lambda kv: kv[1])}))
    print("pages", total)
