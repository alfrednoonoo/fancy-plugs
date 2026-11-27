"""Builds the Fancy Plugs website into a folder of static files.

    python tools/site/build.py [out]          default out: site/ (git-ignored)

Needs Python 3.12+ (nested quotes in f-strings), Pillow, fontTools with brotli (for WOFF2), markdown and pypdf. The design is
docs/website-plan.md; the page text is site-src/data/*.json, README.md (What's new) and the manual's
shared chapters (tools/manual/content.py), so the site, the README and the manual never disagree.
Pages use relative links, so the folder works at any address. 404.html alone uses SITE_PATH.
"""
import html, importlib.util, json, math, os, re, shutil, sys
import markdown
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont
from pypdf import PdfReader

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(REPO, "site")
SRC = os.path.join(REPO, "site-src")
SITE_URL = "https://alfrednoonoo.github.io/fancy-plugs/"
SITE_PATH = "/fancy-plugs/"
GH = "https://github.com/alfrednoonoo/fancy-plugs"
# The newest manual in docs/manual/, by version number.
_pdfs = [f for f in os.listdir(os.path.join(REPO, "docs/manual")) if re.fullmatch(r"Fancy-Plugs-Manual-[\d.]+\.pdf", f)]
PDF_SRC = os.path.join(REPO, "docs/manual", max(_pdfs, key=lambda f: [int(x) for x in re.findall(r"\d+", f)]))

E = lambda s: html.escape(str(s), quote=True)


def load(path):
    spec = importlib.util.spec_from_file_location(os.path.basename(path)[:-3].replace("-", "_"), path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m


DATA = json.load(open(os.path.join(SRC, "data/plugins.json"), encoding="utf-8"))
REL = json.load(open(os.path.join(SRC, "data/release.json"), encoding="utf-8"))
PLUG, GROUPS = DATA["plugins"], DATA["groups"]
GROUP_OF = {s: g for g in GROUPS for s in g["plugins"]}
ORDER = [s for g in GROUPS for s in g["plugins"]]
BRAND = load(os.path.join(REPO, "design/website/make-brand.py"))
MANUAL = load(os.path.join(REPO, "tools/manual/content.py"))
assert [p[0] for p in MANUAL.PARTS] == [g["name"] for g in GROUPS], "groups differ from the manual's PARTS"

# Purple fails 4.5:1 as small text on #141414, so small text on purple pages uses a lighter purple
# (the user's choice, 16 November 2026). Shapes keep the plugin's own colour.
TEXT_ACCENT = {"#9A59B5": "#A970C2"}


# --- Contrast: every text colour pair the CSS uses, checked on each build ------------------------
def lum(h):
    c = [int(h[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    c = [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def ratio(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def check_contrast():
    pairs = [(t, b) for t in ("#FFFFFF", "#E2E2E2", "#EDEDED") for b in ("#1E1E1E", "#262626", "#141414", "#303030", "#383838", "#434343")]
    pairs += [("#8D8D8D", b) for b in ("#1E1E1E", "#262626", "#141414")]
    pairs += [("#B5B5B5", b) for b in ("#1E1E1E", "#262626", "#303030")]
    pairs += [("#141414", "#00A985"), ("#141414", "#12BE98"), ("#FFCF07", "#141414")]
    for p in PLUG.values():
        t = TEXT_ACCENT.get(p["accent"], p["accent"])
        pairs += [(t, "#141414"), ("#141414", t)]
    bad = [(a, b, round(ratio(a, b), 2)) for a, b in pairs if ratio(a, b) < 4.5]
    if bad:
        sys.exit(f"contrast below 4.5:1: {bad}")
    print(f"contrast: {len(pairs)} pairs at 4.5:1 or more")


# --- Assets --------------------------------------------------------------------------------------
IMG_SIZES = {}


def webp(im, name, widths):
    """Saves im at each width as img/<name>-<w>.webp. Returns the srcset."""
    out = []
    for w in sorted(set(widths)):
        if w > im.width: continue
        h = round(im.height * w / im.width)
        im.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, "img", f"{name}-{w}.webp"), quality=86, method=6)
        out.append((w, f"img/{name}-{w}.webp"))
    return out


def screenshots():
    os.makedirs(os.path.join(OUT, "img"), exist_ok=True)
    for f in sorted(os.listdir(os.path.join(REPO, "docs/images"))):
        if not f.endswith(".png"): continue
        name, im = f[:-4], Image.open(os.path.join(REPO, "docs/images", f)).convert("RGB")
        w1 = im.width // 2
        IMG_SIZES[name] = (w1, im.height // 2, webp(im, name, [480, w1, im.width]))
        # The full-size view for phones.
        IMG_SIZES[name + ":full"] = f"img/{name}-{im.width}.webp"
    for slug, p in PLUG.items():
        L = p["lead"]
        im = Image.open(os.path.join(REPO, "docs/images", L["image"] + ".png")).convert("RGB")
        x0, y0, x1, y1 = [v / 100 for v in L["box"]]
        c = im.crop((round(x0 * im.width), round(y0 * im.height), round(x1 * im.width), round(y1 * im.height)))
        IMG_SIZES[slug + "-lead"] = (c.width // 2, c.height // 2, webp(c, slug + "-lead", [480, c.width // 2, c.width]))
    comp = Image.open(os.path.join(REPO, "docs/images/comp.png")).convert("RGB")
    head = comp.crop((0, 0, comp.width, 146))
    IMG_SIZES["header"] = (head.width // 2, head.height // 2, webp(head, "header", [head.width // 2, head.width]))


def img(name, alt, sizes, cls="", lazy=True, extra=""):
    w, h, srcs = IMG_SIZES[name]
    srcset = ", ".join(f"{R}{s} {sw}w" for sw, s in srcs)
    src = next(s for sw, s in srcs if sw == w) if any(sw == w for sw, s in srcs) else srcs[-1][1]
    load_ = ' loading="lazy" decoding="async"' if lazy else ' fetchpriority="high"'
    c = f' class="{cls}"' if cls else ""
    return (f'<img src="{R}{src}" srcset="{srcset}" sizes="{sizes}" width="{w}" height="{h}" alt="{E(alt)}"{c}{load_}{extra}>')


def fonts():
    os.makedirs(os.path.join(OUT, "fonts"), exist_ok=True)
    # Subset to Latin-1 plus the punctuation and symbols the pages use, keeping kerning and features.
    # Characters outside it fall back to the system font.
    from fontTools import subset
    text_chars = set()
    for d in (os.path.join(SRC, "data"),):
        for f in os.listdir(d): text_chars |= set(open(os.path.join(d, f), encoding="utf-8").read())
    text_chars |= set(open(os.path.join(REPO, "README.md"), encoding="utf-8").read())
    text_chars |= set(MANUAL.FIRST_STEPS + MANUAL.WINDOW)
    uni = set(range(0x20, 0x7F)) | set(range(0xA0, 0x100)) | {ord(c) for c in text_chars if ord(c) >= 0x100}
    uni |= {0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x2039, 0x203A, 0x2212, 0x2192, 0x2248, 0x00D7, 0x2013}
    uni.discard(0x2014)  # no em dash in the site's text
    for n in ("Regular", "Medium", "SemiBold"):
        f = TTFont(os.path.join(REPO, f"assets/fonts/Inter-{n}.ttf"))
        o = subset.Options(); o.layout_features = ["*"]; o.flavor = "woff2"; o.name_IDs = ["*"]; o.notdef_outline = True
        sub = subset.Subsetter(o); sub.populate(unicodes=uni); sub.subset(f)
        f.flavor = "woff2"; f.save(os.path.join(OUT, "fonts", f"Inter-{n}.woff2"))
    shutil.copy(os.path.join(REPO, "assets/fonts/Inter-LICENSE.txt"), os.path.join(OUT, "fonts"))


def brand():
    d = os.path.join(REPO, "design/website")
    for f in ("favicon.svg", "favicon.ico", "favicon-32.png", "apple-touch-icon.png", "icon-192.png", "icon-512.png", "logo-lockup.svg", "logo-mark.svg"):
        shutil.copy(os.path.join(d, f), OUT)
    json.dump({"name": "Fancy Plugs", "short_name": "Fancy Plugs", "start_url": "./", "display": "browser",
               "background_color": "#1E1E1E", "theme_color": "#1E1E1E",
               "icons": [{"src": "icon-192.png", "sizes": "192x192", "type": "image/png"},
                         {"src": "icon-512.png", "sizes": "512x512", "type": "image/png"}]},
              open(os.path.join(OUT, "site.webmanifest"), "w"), indent=1)


def read_manual():
    shutil.copy(PDF_SRC, os.path.join(OUT, "manual.pdf"))
    r = PdfReader(PDF_SRC); pages = {}

    def walk(o):
        for x in o:
            if isinstance(x, list): walk(x)
            else: pages[x.title] = r.get_destination_page_number(x) + 1
    walk(r.outline)
    return pages, len(r.pages), round(os.path.getsize(PDF_SRC) / 1e6)


# Social preview images, drawn with Pillow so the build needs no browser.
def draw_mark(size):
    k = 4; S = size * k
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    u = S / 64
    for x in (25.75, 34.5):
        d.rounded_rectangle([x * u, 38 * u, (x + 3.75) * u, 60 * u], radius=1.6 * u, fill="#FFFFFF")
    cx, cy, r, sw = 32 * u, 27 * u, 21 * u, 5 * u
    steps = 240
    for i in range(steps):
        a0 = -150 + 300 * i / steps; a1 = a0 + 300 / steps + 0.6
        col = BRAND.colour_at((i + 0.5) / steps)
        d.arc([cx - r - sw / 2, cy - r - sw / 2, cx + r + sw / 2, cy + r + sw / 2], a0 - 90, a1 - 90, fill=col, width=round(sw))
    for ang, col in ((-150, BRAND.ACCENTS[0]), (150, BRAND.ACCENTS[-1])):
        x = cx + r * math.sin(math.radians(ang)); y = cy - r * math.cos(math.radians(ang))
        d.ellipse([x - sw / 2, y - sw / 2, x + sw / 2, y + sw / 2], fill=col)
    d.ellipse([cx - 14.5 * u, cy - 14.5 * u, cx + 14.5 * u, cy + 14.5 * u], fill="#4A4A4A")
    x0, y0, x1, y1 = 35.54 * u, 22.79 * u, 39.07 * u, 18.57 * u
    d.line([x0, y0, x1, y1], fill="#FFFFFF", width=round(2.4 * u))
    for x, y in ((x0, y0), (x1, y1)):
        d.ellipse([x - 1.2 * u, y - 1.2 * u, x + 1.2 * u, y + 1.2 * u], fill="#FFFFFF")
    return im.resize((size, size), Image.LANCZOS)


def og_image(name, title, kind, accent, shot):
    W, H = 1200, 630
    im = Image.new("RGB", (W, H), "#1E1E1E"); d = ImageDraw.Draw(im)
    f = lambda w, s: ImageFont.truetype(os.path.join(REPO, f"assets/fonts/Inter-{w}.ttf"), s)
    d.rectangle([0, 0, W, 44], fill="#141414")
    d.text((48, 14), "FANCY", font=f("SemiBold", 17), fill="#8D8D8D")
    d.text((48 + d.textlength("FANCY ", font=f("SemiBold", 17)) + 6, 14), title.upper().replace("FANCY ", ""), font=f("SemiBold", 17),
           fill=TEXT_ACCENT.get(accent, accent))
    m = draw_mark(120); im.paste(m, (40, 92), m)
    d.text((48, 250), title, font=f("SemiBold", 64), fill="#FFFFFF")
    y = 340
    for line in wrap(kind, f("Regular", 30), 470, d):
        d.text((48, y), line, font=f("Regular", 30), fill="#B5B5B5"); y += 42
    if shot:
        s = Image.open(os.path.join(REPO, "docs/images", shot + ".png")).convert("RGB")
        s.thumbnail((620, 520), Image.LANCZOS)
        x, y = W - s.width - 40, 44 + (H - 44 - s.height) // 2
        d.rounded_rectangle([x - 1, y - 1, x + s.width, y + s.height], radius=10, outline="#434343", width=2)
        im.paste(s, (x, y))
    os.makedirs(os.path.join(OUT, "og"), exist_ok=True)
    im.save(os.path.join(OUT, "og", name + ".png"), optimize=True)
    return f"og/{name}.png"


def wrap(text, font, width, d):
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if d.textlength(t, font=font) > width and cur: lines.append(cur); cur = w
        else: cur = t
    return lines + [cur] if cur else lines


# --- Page shell ----------------------------------------------------------------------------------
R = ""  # relative path from the current page to the site root


def ring_icon(c):
    return f'<i style="--c: {c}"></i>'


def plugin_href(slug):
    return f"{R}plugins/{slug}/"


def logo_svg():
    s = BRAND.lockup("#8D8D8D", "#FFFFFF", "#FFFFFF", draw_id="ringdraw")
    s = s.replace('role="img" aria-label="Fancy Plugs"><title>Fancy Plugs</title>', 'aria-hidden="true" focusable="false">')
    return s.strip()


LOGO = None


def header(current):
    menu = ""
    for g in GROUPS:
        items = "".join(f'<li><a href="{plugin_href(s)}">{ring_icon(PLUG[s]["accent"])}Fancy {E(PLUG[s]["name"])}</a></li>' for s in g["plugins"])
        menu += f'<div><h3><a href="{R}{g["slug"]}/">{E(g["name"])}</a></h3><ul>{items}</ul></div>'
    sheet_groups = ""
    for g in GROUPS:
        items = "".join(f'<li><a href="{plugin_href(s)}">{ring_icon(PLUG[s]["accent"])}Fancy {E(PLUG[s]["name"])}</a></li>' for s in g["plugins"])
        sheet_groups += f'<h3><a href="{R}{g["slug"]}/">{E(g["name"])}</a></h3><ul>{items}</ul>'
    cur = lambda k: ' aria-current="page"' if k == current else ""
    plug_cur = ' data-current=""' if current == "plugins" else ""
    chevron = '<svg width="10" height="6" viewBox="0 0 10 6" aria-hidden="true"><path d="M1 1l4 4 4-4" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    return f'''<header class="top"><div class="wrap">
  <a class="logo" href="{R}./" aria-label="Fancy Plugs, home">{LOGO}</a><script>try{{if(!sessionStorage.getItem("fp-ring")){{document.currentScript.previousElementSibling.classList.add("draw");sessionStorage.setItem("fp-ring","1")}}}}catch(e){{}}</script>
  <nav aria-label="Site"><ul>
    <li><details class="plugmenu"{plug_cur}><summary>Plugins {chevron}</summary><div class="menu-panel">{menu}</div></details></li>
    <li><a href="{R}guide/"{cur("guide")}>Guide</a></li>
    <li><a href="{R}whats-new/"{cur("whats-new")}>What’s new</a></li>
  </ul></nav>
  <a class="btn btn-main" href="{R}download/">Download</a>
  <button class="menu-btn" type="button" aria-label="Open the menu" aria-haspopup="dialog">
    <svg width="20" height="14" viewBox="0 0 20 14" aria-hidden="true"><path d="M1 1h18M1 7h18M1 13h18" stroke="#fff" stroke-width="2" stroke-linecap="round"/></svg>
  </button>
</div></header>
<dialog class="sheet" aria-label="Menu">
  <button class="btn btn-2 close" type="button">Close</button>
  {sheet_groups}
  <h3>More</h3>
  <a href="{R}guide/">Guide</a>
  <a href="{R}whats-new/">What’s new</a>
  <a href="{R}download/">Download {E(REL["version"])}</a>
  <a href="{R}about/">About</a>
</dialog>'''


def footer():
    return f'''<footer><div class="wrap">
  <div class="row">
    <img src="{R}logo-lockup.svg" width="233" height="64" alt="Fancy Plugs" loading="lazy">
    <a href="{R}manual.pdf">Manual (PDF)</a><a href="{GH}/releases">Releases</a><a href="{GH}">Source on GitHub</a><a href="{R}about/">About</a><span>Version {E(REL["version"])}</span>
  </div>
  <p>Free to download and use, and open source under the <a href="{GH}/blob/main/LICENSE">GNU AGPL v3</a>. UI design based on “ReaPlugs (vst GUI)” by Eugene Reznik, <a href="https://www.figma.com/community/file/967866927752067729">CC BY 4.0</a>, modified from the original. The Inter typeface is used under the <a href="{R}fonts/Inter-LICENSE.txt">SIL Open Font License</a>.</p>
</div></footer>'''


def page(path, title, desc, strip, body, accent=None, current="", og="og/fancy-plugs.png", base=None):
    """path: the page's folder relative to the site root ('' for home), or a file name for 404."""
    body_html = body() if callable(body) else body
    style = ""
    if accent:
        style = f' style="--accent: {accent}; --accent-text: {TEXT_ACCENT.get(accent, accent)}"'
    url = SITE_URL + ("" if path.endswith(".html") else path)
    doc = f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
{f'<base href="{base}">' if base else ""}
<title>{E(title)}</title>
<meta name="description" content="{E(desc)}">
<link rel="canonical" href="{url}">
<meta property="og:type" content="website">
<meta property="og:site_name" content="Fancy Plugs">
<meta property="og:title" content="{E(title)}">
<meta property="og:description" content="{E(desc)}">
<meta property="og:url" content="{url}">
<meta property="og:image" content="{SITE_URL}{og}">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="theme-color" content="#1E1E1E">
<link rel="icon" href="{R}favicon.ico" sizes="48x48">
<link rel="icon" href="{R}favicon.svg" type="image/svg+xml">
<link rel="apple-touch-icon" href="{R}apple-touch-icon.png">
<link rel="manifest" href="{R}site.webmanifest">
<link rel="preload" href="{R}fonts/Inter-Regular.woff2" as="font" type="font/woff2" crossorigin>
<link rel="preload" href="{R}fonts/Inter-SemiBold.woff2" as="font" type="font/woff2" crossorigin>
<link rel="stylesheet" href="{R}site.css">
<script src="{R}site.js" defer></script>
</head>
<body{style}>
<a class="skip" href="#main">Skip to the content</a>
<div class="strip"><div class="wrap">{strip}</div></div>
{header(current)}
<main id="main"{' class="inner"' if path != "" else ""}>
{body_html}
</main>
{footer()}
</body>
</html>
'''
    doc = doc.replace("\n\n<title>", "\n<title>")
    dest = os.path.join(OUT, path) if path.endswith(".html") else os.path.join(OUT, path, "index.html")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    open(dest, "w", encoding="utf-8", newline="\n").write(doc)
    if not path.endswith(".html"):
        SITEMAP.append(url)


SITEMAP = []


def strip_html(name, right=""):
    return f'<span><a href="{R}./">Fancy</a><b>{E(name)}</b></span>{right}'


def band():
    w, m = REL["windows"], REL["mac"]
    return f'''<div class="wrap"><div class="band">
  <div>
    <h2>Download Fancy Plugs {E(REL["version"])}</h2>
    <p>All sixteen plugins, free to download and use. VST3 for Windows 10 and 11 (x64); VST3 and Audio Unit for macOS 11 or later, Intel and Apple silicon.</p>
  </div>
  <div class="actions">
    <a class="btn btn-main" href="{w["url"]}">Windows <small>{E(REL["version"])} · {w["size_mb"]} MB zip</small></a>
    <a class="btn btn-2" href="{m["url"]}">Mac <small>{E(REL["version"])} · {m["size_mb"]} MB zip</small></a>
  </div>
</div></div>'''


def md(text):
    return markdown.markdown(text, extensions=["tables"])


def link_plugins(h):
    """Links each 'Fancy <Name>' in rendered HTML to its page (outside existing links)."""
    names = sorted(((p["name"], s) for s, p in PLUG.items()), key=lambda x: -len(x[0]))
    pat = re.compile(r"Fancy (" + "|".join(re.escape(n) for n, _ in names) + r")(?![-\w])")
    by = {n: s for n, s in names}
    parts = re.split(r"(<a\b.*?</a>)", h, flags=re.S)
    return "".join(p if p.startswith("<a") else pat.sub(lambda m: f'<a href="{plugin_href(by[m.group(1)])}">{m.group(0)}</a>', p) for p in parts)


# --- Demo clips (section 7.1) --------------------------------------------------------------------
# site-src/video/<slug>.webm and .mp4, from CI's demos-<sha> artifact. A plugin without them shows
# its screenshot alone.
def demo_files(slug):
    d = os.path.join(SRC, "video")
    have = all(os.path.exists(os.path.join(d, f"{slug}.{x}")) for x in ("webm", "mp4"))
    if have:
        os.makedirs(os.path.join(OUT, "video"), exist_ok=True)
        for x in ("webm", "mp4"):
            if not os.path.exists(os.path.join(OUT, "video", f"{slug}.{x}")):
                shutil.copy(os.path.join(d, f"{slug}.{x}"), os.path.join(OUT, "video"))
    return {x: f"{R}video/{slug}.{x}" for x in ("webm", "mp4")} if have else None


def demo(slug):
    """A silent looping clip laid over the screenshot. site.js loads it only near the screen."""
    f = demo_files(slug)
    if not f: return ""
    return (f'<video class="demo" muted loop playsinline preload="none" aria-hidden="true" tabindex="-1" '
            f'data-webm="{f["webm"]}" data-mp4="{f["mp4"]}"></video>')


# --- Pages ---------------------------------------------------------------------------------------
def tile(slug, wide=False, lazy=True):
    p = PLUG[slug]
    sizes = "(max-width: 720px) 100vw, (max-width: 1040px) 50vw, " + ("560px" if wide else "370px")
    return (f'<li class="{"wide" if wide else ""}"><a class="tile" href="{plugin_href(slug)}"><div class="stage">'
            f'{img(p["views"][0]["image"], "Fancy " + p["name"] + ", " + p["kind"].lower(), sizes, cls="reveal", lazy=lazy)}{demo(slug)}</div>'
            f'<h3>{ring_icon(p["accent"])}Fancy {E(p["name"])}</h3><p>{E(p["kind"])}. {E(p["line"])}</p></a></li>')


def home():
    comp = PLUG["comp"]
    roster = ""
    for s in ORDER:
        p = PLUG[s]; im = p["views"][0]["image"]; w, h, srcs = IMG_SIZES[im]
        srcset = ", ".join(f"{R}{x} {sw}w" for sw, x in srcs)
        src = next((x for sw, x in srcs if sw == w), srcs[-1][1])
        clip = demo_files(s) or {"webm": "", "mp4": ""}
        roster += (f'<li><button type="button" aria-pressed="{"true" if s == "comp" else "false"}" style="--c: {p["accent"]}" '
                   f'data-name="{E(p["name"])}" data-kind="{E(p["kind"])}" data-src="{R}{src}" data-srcset="{srcset}" '
                   f'data-w="{w}" data-h="{h}" data-href="{plugin_href(s)}" data-webm="{clip["webm"]}" data-mp4="{clip["mp4"]}"><i></i>{E(p["name"])}</button></li>')
    groups = ""
    for g in GROUPS:
        tiles = "".join(tile(s, wide=(s == "eq")) for s in g["plugins"])
        groups += f'''<section class="wrap" id="{g["slug"]}">
  <div class="group-head"><h2><a href="{R}{g["slug"]}/">{E(g["name"])}</a></h2><p>{E(dict((x[0], x[1]) for x in MANUAL.PARTS)[g["name"]])}</p></div>
  <ul class="tiles {g["shape"]}">{tiles}</ul>
</section>
'''
    spots = [(12, 14, 1), (14.9, 100, 2), (59.3, 100, 3), (71, 100, 4), (92.5, 100, 5)]
    newest = whats_new_entries()[0]
    return f'''<div class="wrap">
  <div class="hero">
    <div>
      <h1>Free effects for the whole mix: dynamics, tone and space.</h1>
      <p class="lede">Sixteen plugins for Windows and Mac that all work the same way. Learn one window and you know them all.</p>
      <div class="actions"><a class="btn btn-main" href="{R}download/">Download {E(REL["version"])}</a><a class="link" href="#plugins">See all sixteen</a></div>
    </div>
    <figure>
      <span class="hero-shot">{img("comp", "Fancy Comp, compressor", "(max-width: 1040px) 100vw, 620px", cls="window", lazy=False)}{demo("comp") or '<video class="demo" muted loop playsinline preload="none" aria-hidden="true" tabindex="-1"></video>'}</span>
      <figcaption><strong>Fancy Comp</strong> · <span class="k">Compressor</span><br><a class="link" href="{plugin_href("comp")}">See what Fancy Comp can do</a></figcaption>
    </figure>
  </div>
  <nav class="roster hero-roster" aria-label="Show a plugin in the window above"><ul>{roster}</ul></nav>
</div>

<div id="plugins">
{groups}</div>

<section class="wrap shared">
  <h2><a href="{R}guide/#the-window">One window, sixteen times</a></h2>
  <p class="lede">Every plugin opens with the same strip and header, so what you learn in one works in the rest.</p>
  <div class="spots">
    {img("header", "The header of Fancy Comp: preset name, New, Parameter, input and output count, undo, redo, A and B, Compare, the output knob and the ON switch", "(max-width: 1168px) 100vw, 1120px", cls="window", extra=' style="width: 100%"')}
    {"".join(f'<span class="spot" style="left: {x}%; top: {y}%" aria-hidden="true">{n}</span>' for x, y, n in spots)}
  </div>
  <ul class="shared-list">
    <li><span class="num">1</span><b>Title strip</b><span class="t">The plugin’s name in its own colour. It reads LISTENING while a Listen switch is on.</span></li>
    <li><span class="num">2</span><b>Preset browser</b><span class="t">Categories, favourites, search and your own folders. One click loads a preset and keeps the browser open.</span></li>
    <li><span class="num">3</span><b>Undo and redo</b><span class="t">Up to 100 steps, for every control, in every plugin.</span></li>
    <li><span class="num">4</span><b>A, B and Compare</b><span class="t">Hold two settings side by side, and compare with the input at the same loudness.</span></li>
    <li><span class="num">5</span><b>Output and ON</b><span class="t">A level knob and a bypass, always in the same corner.</span></li>
    <li><span class="num" aria-hidden="true"></span><b>Typed values</b><span class="t">Click any value and type it. Frequencies take 2500 or 2.5k.</span></li>
    <li><span class="num" aria-hidden="true"></span><b>Any size</b><span class="t">Drag a window’s edge to scale it from 100 % to 200 %.</span></li>
  </ul>
</section>

<section class="wrap">
  <div class="split">
    <div>
      <h2>New in {E(newest["version"])}</h2>
      <p><strong>Fancy FIR has a Resonance mode.</strong> It finds narrow peaks that stand above the spectrum around them, such as a ringing snare or a boxy vocal, and turns each one down only while it’s there.</p>
      <p>Projects saved with 1.0 to 1.5 load and sound exactly as before.</p>
      <p class="more"><a class="link" href="{R}whats-new/">What’s new in every release</a></p>
    </div>
    <div class="stage">{img("fir-resonance", "Fancy FIR in Resonance mode, turning down resonances at 700 Hz and 3 kHz", "(max-width: 1040px) 100vw, 640px", cls="reveal")}</div>
  </div>
</section>

{band()}'''


def hub(g):
    rows = ""
    for s in g["plugins"]:
        p = PLUG[s]; what, can = p["hub"]
        rows += f'''<article class="row-plugin" id="{s}" style="--c: {p["accent"]}">
  <div class="stage">{img(p["views"][0]["image"], "Fancy " + p["name"] + ", " + p["kind"].lower(), "(max-width: 1040px) 100vw, 640px", cls="reveal", lazy=s != g["plugins"][0])}</div>
  <div>
    <h2><a href="{plugin_href(s)}">Fancy {E(p["name"])}</a></h2>
    <p class="kind">{E(p["kind"])}</p>
    <p>{E(what)}</p>
    <ul>{"".join(f"<li>{E(x)}</li>" for x in can)}</ul>
    <a class="link" href="{plugin_href(s)}">See Fancy {E(p["name"])}</a>
  </div>
</article>
'''
    table = "".join(f'<tr><th scope="row">{E(task)}</th><td><a href="{plugin_href(s)}">Fancy {E(PLUG[s]["name"])}</a></td><td class="why">{E(why)}</td></tr>'
                    for task, s, why in g["which"])
    desc = dict((x[0], x[1]) for x in MANUAL.PARTS)[g["name"]]
    return f'''<div class="wrap">
  <div class="page-head"><div><h1>{E(g["name"])}</h1><p class="lede">{E(desc)}</p></div></div>
  <nav class="onpage" aria-label="On this page"><span>On this page</span>
    {"".join(f'<a href="#{s}">{E(PLUG[s]["name"])}</a>' for s in g["plugins"])}<a href="#which">Which one do I need?</a>
  </nav>
  {rows}
  <section id="which">
    <h2>Which one do I need?</h2>
    <table>
      <thead><tr><th scope="col">I want to</th><th scope="col">Use</th><th scope="col">Because</th></tr></thead>
      <tbody>{table}</tbody>
    </table>
  </section>
</div>
{band()}'''


# The before/after player is switched off for now (user, 17 November 2026). The code, its CSS and
# its script stay in place: set SHOW_PLAYER to True and add clips in site-src/audio/<slug>/.
SHOW_PLAYER = False


def player(slug):
    if not SHOW_PLAYER: return ""
    d = os.path.join(SRC, "audio", slug)
    if not os.path.isdir(d): return ""
    files = {v: next((f for f in sorted(os.listdir(d)) if f.startswith(v + ".")), None) for v in ("before", "after")}
    if not all(files.values()): return ""
    out = os.path.join(OUT, "audio", slug); os.makedirs(out, exist_ok=True)
    for f in files.values(): shutil.copy(os.path.join(d, f), out)
    cap = open(os.path.join(d, "caption.txt"), encoding="utf-8").read().strip() if os.path.exists(os.path.join(d, "caption.txt")) else ""
    src = lambda v: f'{R}audio/{slug}/{files[v]}'
    return f'''<section>
    <h2>Hear it</h2>
    <div class="player">
      <audio data-v="before" src="{src("before")}" preload="metadata"></audio><audio data-v="after" src="{src("after")}" preload="metadata"></audio>
      <button class="play" type="button" aria-pressed="false" aria-label="Play"><svg class="i-play" width="16" height="18" viewBox="0 0 16 18" aria-hidden="true"><path d="M1 1v16l14-8z" fill="#fff"/></svg><svg class="i-pause" width="14" height="16" viewBox="0 0 14 16" aria-hidden="true"><path d="M1 1h4v14H1zM9 1h4v14H9z" fill="#fff"/></svg></button>
      <div class="seg" role="group" aria-label="Version"><button type="button" data-v="before" aria-pressed="false">Before</button><button type="button" data-v="after" aria-pressed="true">After</button></div>
      <div class="bar" role="presentation"><i></i></div>
      <span class="time">0:00</span>
      <p class="status" role="status"></p>
      {f'<p class="cap">{E(cap)}</p>' if cap else ""}
    </div>
  </section>'''


def plugin_page(slug, manual_pages):
    p = PLUG[slug]; g = GROUP_OF[slug]; members = g["plugins"]; i = members.index(slug)
    prev_, next_ = members[i - 1], members[(i + 1) % len(members)]
    mpage = manual_pages.get("Fancy " + p["name"])
    manual_href = f'{R}manual.pdf#page={mpage}' if mpage else f"{R}manual.pdf"
    tabs = "".join(f'<button type="button" role="tab" id="tab-{v["id"]}" aria-controls="view-{v["id"]}" aria-selected="{"true" if k == 0 else "false"}"'
                   f'{"" if k == 0 else " tabindex=" + chr(34) + "-1" + chr(34)}>{E(v["label"])}</button>' for k, v in enumerate(p["views"]))
    views = ""
    for k, v in enumerate(p["views"]):
        w, h, _ = IMG_SIZES[v["image"]]
        marks = "".join(f'<button type="button" class="spot" data-n="{n}" style="left: {x}%; top: {y}%" aria-label="{n}: {E(t)}">{n}</button>'
                        for n, (x, y, t, _d) in enumerate(v["points"], 1))
        lis = "".join(f'<li data-n="{n}"><span class="num">{n}</span><b>{E(t)}</b><span class="t">{E(d)}</span></li>' for n, (_x, _y, t, d) in enumerate(v["points"], 1))
        role = f'role="tabpanel" aria-labelledby="tab-{v["id"]}"' if len(p["views"]) > 1 else 'style="margin-top: 0"'
        views += f'''<div class="view" {role} id="view-{v["id"]}"{"" if k == 0 else " hidden"}>
  <div class="shot" style="max-width: {w}px">{img(v["image"], v["alt"], f"(max-width: {w + 80}px) 100vw, {w}px", lazy=k != 0)}{marks}</div>
  <a class="full" href="{R}{IMG_SIZES[v["image"] + ":full"]}">Open the screenshot full size</a>
  <ol class="points">{lis}</ol>
</div>
'''
    L = p["lead"]
    details = [["Formats", "VST3 on Windows 10 and 11 (x64). VST3 and Audio Unit on macOS 11 or later (Intel and Apple silicon)."]]
    if not any(k == "Inputs" for k, _ in p["details"]):
        details.append(["Inputs", "Mono or stereo."])
    details += p["details"] + [["Window", f'{p["window"][0]} × {p["window"][1]} at 100 %, resizable to 200 %.']]
    if mpage:
        details.append(["Manual", f'<a href="{manual_href}">Chapter on page {mpage} of the manual</a>'])
    rows = "".join(f'<tr><th scope="row">{E(k)}</th><td>{v if k == "Manual" else E(v)}</td></tr>' for k, v in details)
    pairs = "".join(f'<a href="{plugin_href(s)}"><b>{ring_icon(PLUG[s]["accent"])}Fancy {E(PLUG[s]["name"])}</b><span>{E(why)}</span></a>' for s, why in p["pairs"])
    return f'''<div class="wrap">
  <div class="page-head">
    <div>
      <h1>Fancy {E(p["name"])}</h1>
      <p class="kind">{E(p["kind"])}</p>
      <p class="lede">{E(p["lede"])}</p>
    </div>
    <div class="actions"><a class="link" href="{manual_href}">Manual chapter</a><a class="btn btn-main" href="{R}download/">Download</a></div>
  </div>

  <div class="explorer">
    {f'<div class="tabs" role="tablist" aria-label="Views of Fancy {E(p["name"])}">{tabs}</div>' if len(p["views"]) > 1 else ""}
    {views}
  </div>

  <section>
    <h2>What you can do with it</h2>
    <div class="lead">
      <div><h3>{E(L["title"])}</h3><p>{E(L["text"])}</p></div>
      <div class="crop reveal" style="aspect-ratio: {IMG_SIZES[slug + "-lead"][0]} / {IMG_SIZES[slug + "-lead"][1]}">{img(slug + "-lead", L["alt"], "(max-width: 1040px) 100vw, 640px")}</div>
    </div>
    <ul class="can">{"".join(f"<li><b>{E(a)}</b><span>{E(b)}</span></li>" for a, b in p["can"])}</ul>
  </section>

  {player(slug)}

  <div class="two">
    <section>
      <h2>Details</h2>
      <table><tbody>{rows}</tbody></table>
    </section>
    <section>
      <h2>Works well with</h2>
      <div class="pairs">{pairs}</div>
    </section>
  </div>

  <nav class="prevnext" aria-label="More {E(g["name"])} plugins">
    <a href="{plugin_href(prev_)}">‹ Fancy {E(PLUG[prev_]["name"])}</a><a class="mid" href="{R}{g["slug"]}/">{E(g["name"])}</a><a href="{plugin_href(next_)}">Fancy {E(PLUG[next_]["name"])} ›</a>
  </nav>
</div>
{band()}'''


def download():
    w, m = REL["windows"], REL["mac"]
    xattr = "xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Fancy*.vst3 ~/Library/Audio/Plug-Ins/Components/Fancy*.component"
    return f'''<div class="wrap">
  <div class="page-head"><div>
    <h1>Download</h1>
    <p class="lede">All sixteen plugins, free to download and use, in one zip for Windows and one for Mac. Each zip has a <code>How to install.txt</code>. They’re open source under the <a href="{GH}/blob/main/LICENSE">GNU AGPL v3</a>, and the source is <a href="{GH}">on GitHub</a>.</p>
  </div></div>
  <div class="dl">
    <div id="windows">
      <h2>Windows</h2>
      <p class="file">{E(w["file"])} · {w["size_mb"]} MB · version {E(REL["version"])}, {date(REL["date"])}</p>
      <a class="btn btn-main" href="{w["url"]}">Download for Windows</a>
      <div class="prose">
        <p>VST3 for Windows 10 and 11 (x64).</p>
        <ol>
          <li>Close your DAW, and unzip the download.</li>
          <li>Drag the <code>.vst3</code> folders from its <code>VST3</code> folder into <code>C:\\Program Files\\Common Files\\VST3</code>, the folder every Windows DAW looks in. Windows asks once for administrator approval: click <em>Continue</em>.</li>
          <li>Open your DAW and rescan its plugins (REAPER: <em>Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; Re-scan</em>).</li>
        </ol>
        <p>To update, close the DAW first and copy the new folders over the old ones. <a href="{w["page"]}">Release page on GitHub</a></p>
      </div>
    </div>
    <div id="mac">
      <h2>Mac</h2>
      <p class="file">{E(m["file"])} · {m["size_mb"]} MB · version {E(REL["version"])}, {date(REL["date"])}</p>
      <a class="btn btn-2" href="{m["url"]}">Download for Mac</a>
      <div class="prose">
        <p>VST3 and Audio Unit for macOS 11 or later, Intel and Apple silicon.</p>
        <ol>
          <li>Close your DAW, and double-click the download to unzip it.</li>
          <li>Double-click <code>Install Fancy Plugs.command</code>. The first time, macOS blocks it, because the plugins aren’t signed with an Apple Developer ID: open <em>System Settings &gt; Privacy &amp; Security</em>, click <em>Open Anyway</em> next to its name, and confirm with your password.</li>
          <li>A Terminal window installs the VST3s to <code>~/Library/Audio/Plug-Ins/VST3</code> and the Audio Units to <code>~/Library/Audio/Plug-Ins/Components</code>, your own folders, and says how many it installed. Press Enter to close it, then open your DAW and rescan.</li>
        </ol>
        <p>If you copy the plugins into those folders by hand instead, macOS won’t load them until you clear the flag it puts on downloaded files: paste this into Terminal and press Return.</p>
      </div>
      <div class="cmd"><code>{E(xattr)}</code><button class="btn btn-2" type="button" data-copy="{E(xattr)}">Copy</button></div>
      <div class="prose"><p>On a Mac, <strong>Cmd-click</strong> a control to type a value. <a href="{m["page"]}">Release page on GitHub</a></p></div>
    </div>
  </div>
  <section class="hosts">
    <h2>Hosts</h2>
    <table><thead><tr><th scope="col">Format</th><th scope="col">Use it in</th></tr></thead><tbody>
      <tr><th scope="row">VST3</th><td>REAPER, Ableton Live, Cubase, Bitwig Studio and Studio One, on Windows and Mac.</td></tr>
      <tr><th scope="row">Audio Unit</th><td>Logic Pro and GarageBand, on Mac. Fancy Gate’s Audio Unit is a music effect, so it can send MIDI.</td></tr>
    </tbody></table>
  </section>
  <section>
    <h2>Next</h2>
    <p class="lede">The <a class="link" href="{R}guide/">guide</a> shows the window all sixteen share, and the <a class="link" href="{R}manual.pdf">manual</a> explains every control. Projects saved with any earlier version load and sound exactly as before.</p>
  </section>
</div>'''


def date(iso):
    y, m, d = iso.split("-")
    return f"{int(d)} {['January','February','March','April','May','June','July','August','September','October','November','December'][int(m) - 1]} {y}"


def guide(manual_pages, n_pages, mb):
    text = MANUAL.FIRST_STEPS + "\n" + MANUAL.WINDOW
    h = md(text)
    toc = []

    def anchor(m):
        title = re.sub("<.*?>", "", m.group(1))
        slug = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
        toc.append((slug, title))
        return f'<h2 id="{slug}">{m.group(1)}</h2>'
    h = re.sub(r"<h2>(.*?)</h2>", anchor, h)
    nav = "".join(f'<a href="#{s}">{E(t)}</a>' for s, t in toc)
    return f'''<div class="wrap">
  <div class="page-head"><div>
    <h1>Guide</h1>
    <p class="lede">Everything the sixteen plugins share, from installing them to the header every window has. Each plugin’s own controls are on its page, and every one of them is in the manual.</p>
  </div></div>
  <div class="guide">
    <aside class="toc" aria-label="On this page"><p>On this page</p><nav>{nav}</nav></aside>
    <div class="prose">{h}
      <a class="manual-link" href="{R}manual.pdf"><svg width="44" height="56" viewBox="0 0 44 56" aria-hidden="true"><rect x="1" y="1" width="42" height="54" rx="4" fill="#141414" stroke="#434343" stroke-width="2"/><path d="M10 14h24M10 22h24M10 30h16" stroke="#8D8D8D" stroke-width="2.4" stroke-linecap="round"/></svg>
        <div><b>The Fancy Plugs manual</b><span>Version {E(MANUAL.VERSION)} · {n_pages} pages · PDF, {mb} MB. A quick start, every control of all sixteen plugins with its range and default, and a glossary.</span></div></a>
    </div>
  </div>
</div>'''


def whats_new_entries():
    readme = open(os.path.join(REPO, "README.md"), encoding="utf-8").read()
    out = []
    for m in re.finditer(r"^## What(?:'s| was) new in ([\d.]+)\n(.*?)(?=^## )", readme, flags=re.S | re.M):
        v = m.group(1)
        out.append({"version": v, "date": REL["dates"].get(v), "md": m.group(2).strip()})
    for o in REL["older"]:
        out.append({"version": o["version"], "date": o["date"], "md": "\n".join("- " + x for x in o["items"])})
    return out


def whats_new():
    items = ""
    for k, e in enumerate(whats_new_entries()):
        body = link_plugins(md(e["md"]))
        items += f'''<details class="rel" id="v{e["version"].replace(".", "-")}"{" open" if k == 0 else ""}>
  <summary><svg class="chev" width="12" height="12" viewBox="0 0 12 12" aria-hidden="true"><path d="M4 2l4 4-4 4" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg><h2>{E(e["version"])}</h2><span>{date(e["date"]) if e["date"] else ""}</span></summary>
  <div class="prose">{body}</div>
</details>'''
    m = REL["mac"]
    return f'''<div class="wrap">
  <div class="page-head"><div>
    <h1>What’s new</h1>
    <p class="lede">Every release, newest first. A project saved with any version loads and sounds exactly the same in every later one.</p>
  </div></div>
  <p class="prose" style="margin-top: var(--s4)"><strong>{E(m["version"])}</strong> <span class="tag">Beta</span> is a Mac test build of {E(REL["version"])}, published on {date(m["date"])}. <a href="{R}download/#mac">Mac download</a></p>
  {items}
</div>'''


def about():
    t = REL["tests"]
    checks = [
        ("DSP tests", f'{t["count"]} test cases for the release build, in <a href="{GH}/actions/runs/{t["run"]}">run {t["run"]}</a>.'),
        ("Steinberg’s VST3 validator", "Every plugin, on every build."),
        ("pluginval", "At its strictest level, on every plugin."),
        ("Host harness", "Each plugin loaded the way a host loads it."),
        ("Screenshots", "Each window is rendered by the build itself; the images on this site are those renders."),
        ("By ear", "Each new plugin and feature is checked by ear in REAPER."),
    ]
    return f'''<div class="wrap">
  <div class="page-head"><div>
    <h1>About</h1>
    <p class="lede">Sixteen original effects with one shared design, built for personal use and given away free. There are no accounts, licence keys, automatic updates or tracking, and this site sets no cookies.</p>
  </div></div>
  <div class="about-grid">
    <section>
      <h2>How a build is checked</h2>
      <p class="prose" style="margin-top: var(--s3)">Nothing is compiled by hand. Each change is built on a GitHub runner, for Windows and for Mac, and runs these checks before it can be released.</p>
    </section>
    <section><ul class="checks">{"".join(f"<li><b>{E(a)}</b><span>{b}</span></li>" for a, b in checks)}</ul></section>
  </div>
  <section class="prose">
    <h2>Credits</h2>
    <p>UI design based on “ReaPlugs (vst GUI)” by Eugene Reznik, <a href="https://www.figma.com/community/file/967866927752067729">CC BY 4.0</a>. Modified from the original.</p>
    <p>The Inter typeface is used under the <a href="{R}fonts/Inter-LICENSE.txt">SIL Open Font License</a>.</p>
    <p>Built with <a href="https://juce.com">JUCE</a>, used under the GNU AGPL v3.</p>
  </section>
  <section class="prose">
    <h2>Source</h2>
    <p>The plugins, this site and the manual are built from one repository: <a href="{GH}">{GH.replace("https://", "")}</a>. Bugs and questions go to its <a href="{GH}/issues">issues</a>.</p>
    <p>Fancy Plugs is free software under the <a href="{GH}/blob/main/LICENSE">GNU Affero General Public License, version 3</a>: you may use, study, change and share it, and anything you share that’s built from it must be under the same licence, with its source.</p>
  </section>
</div>'''


def not_found():
    roster = "".join(f'<li><a href="plugins/{s}/" style="--c: {PLUG[s]["accent"]}"><i></i>{E(PLUG[s]["name"])}</a></li>' for s in ORDER)
    return f'''<div class="wrap lost">
  <h1>There’s no page here.</h1>
  <p class="lede">The address may have changed. Every plugin is one click away:</p>
  <nav class="roster" aria-label="Plugins"><ul>{roster}</ul></nav>
  <p><a class="link" href="./">Go to the home page</a></p>
</div>'''


def main():
    global LOGO
    check_contrast()
    # Empty the folder rather than delete it, so a preview server running in it keeps working.
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        x = os.path.join(OUT, f)
        shutil.rmtree(x) if os.path.isdir(x) else os.remove(x)
    screenshots(); fonts(); brand()
    for f in ("site.css", "site.js"):
        shutil.copy(os.path.join(SRC, "assets", f), OUT)
    mpages, n_pages, mb = read_manual()
    LOGO = logo_svg()

    og_default = og_image("fancy-plugs", "Fancy Plugs", "Free effects for the whole mix: dynamics, tone and space.", "#00A985", "comp")
    # The strip and body need R, which render() sets first, so pages pass callables.
    render("", "Fancy Plugs: free effects for the whole mix",
           "Sixteen free VST3 and Audio Unit effects for Windows and Mac: compressors, EQs, reverbs, a delay, a chorus and more, all with the same window.",
           lambda: strip_html("Plugs", f'<span>Version {E(REL["version"])}</span>'), home, og=og_default)
    for g in GROUPS:
        og = og_image(g["slug"], g["name"], dict((x[0], x[1]) for x in MANUAL.PARTS)[g["name"]], "#00A985", PLUG[g["plugins"][0]]["views"][0]["image"])
        render(g["slug"] + "/", f'{g["name"]} plugins: Fancy Plugs',
               f'{len(g["plugins"])} free plugins: ' + ", ".join("Fancy " + PLUG[s]["name"] for s in g["plugins"]) + ".",
               lambda g=g: strip_html(g["strip"], f'<span>{len(g["plugins"])} of 14 plugins</span>'), lambda g=g: hub(g), current="plugins", og=og)
    for s in ORDER:
        p = PLUG[s]; g = GROUP_OF[s]
        og = og_image(s, "Fancy " + p["name"], p["kind"], p["accent"], p["views"][0]["image"])
        render(f"plugins/{s}/", f'Fancy {p["name"]}, a free {p["kind"].lower()}: Fancy Plugs', p["lede"],
               lambda p=p, g=g: strip_html(p["name"], f'<a href="{R}{g["slug"]}/">{E(g["name"])}</a>'),
               lambda s=s: plugin_page(s, mpages), accent=p["accent"], current="plugins", og=og)
    render("download/", "Download Fancy Plugs", f'Download Fancy Plugs {REL["version"]} for Windows and Mac. Free to download and use.',
           lambda: strip_html("Download", f'<span>Version {E(REL["version"])}</span>'), download, og=og_default)
    render("guide/", "Guide: Fancy Plugs", "Installing Fancy Plugs, and the window every plugin shares: presets, undo, A/B, typed values, resizing and latency.",
           lambda: strip_html("Guide"), lambda: guide(mpages, n_pages, mb), current="guide", og=og_default)
    render("whats-new/", "What’s new: Fancy Plugs", "Every Fancy Plugs release, newest first.",
           lambda: strip_html("What’s new"), whats_new, current="whats-new", og=og_default)
    render("about/", "About Fancy Plugs", "How Fancy Plugs is built and tested, the credits, and the source.",
           lambda: strip_html("About"), about, og=og_default)
    render("404.html", "Page not found: Fancy Plugs", "There’s no page at this address.",
           lambda: strip_html("404"), not_found, og=og_default, base=SITE_PATH)

    open(os.path.join(OUT, "sitemap.xml"), "w", encoding="utf-8").write(
        '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        + "".join(f"  <url><loc>{u}</loc></url>\n" for u in SITEMAP) + "</urlset>\n")
    open(os.path.join(OUT, "robots.txt"), "w").write(f"User-agent: *\nAllow: /\nSitemap: {SITE_URL}sitemap.xml\n")
    open(os.path.join(OUT, ".nojekyll"), "w").close()
    n = sum(len(f) for _, _, f in os.walk(OUT))
    size = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(OUT) for f in fs)
    print(f"wrote {len(SITEMAP) + 1} pages, {n} files, {size / 1e6:.1f} MB, to {OUT}")


def render(path, title, desc, strip, body, **kw):
    # page() sets R before calling strip() and body(), so links are relative to the right folder.
    global R
    depth = 0 if kw.get("base") or path.endswith(".html") or path == "" else path.strip("/").count("/") + 1
    R = "" if kw.get("base") else "../" * depth
    page(path, title, desc, strip(), body, **kw)


if __name__ == "__main__":
    main()
