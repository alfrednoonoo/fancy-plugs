"""Builds the website mockup: index.html (home), dynamics.html (a hub) and comp.html (a plugin page).

    py -3.14 design/website/mockup/build.py           writes the three pages and the image crops
    py -3.14 design/website/mockup/build.py --png     also renders png/<page>-desktop.png and -phone.png (Edge)

Design: docs/website-plan.md. Styles: site.css. Behaviour: site.js. Screenshots: docs/images/.
"""
import os, subprocess, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
IMG = "../../../docs/images/"
GH = "https://github.com/alfrednoonoo/fancy-plugs"
WIN = GH + "/releases/tag/v1.6.0"
MAC = GH + "/releases/tag/v1.7.0-beta.1"
PDF = GH + "/blob/main/docs/manual/Fancy-Plugs-Manual-1.6.pdf"
VERSION = "1.6.0"

RED, TEAL, YELLOW, GREEN, BLUE, ORANGE, PURPLE = "#E74D3D", "#00A985", "#F1C315", "#2DCC70", "#3598DB", "#E77E23", "#9A59B5"
# name, slug, kind, accent, image, width and height at 100 %, one line
PLUGINS = [
    ("Comp", "comp", "Compressor", TEAL, "comp", 934, 547, "The threshold sits on the input meter, with a filtered detector and a sidechain."),
    ("MultiComp", "multicomp", "Multiband compressor", RED, "multicomp", 760, 609, "Up to 8 bands of compression that sum back flat."),
    ("Gate", "gate", "Gate, ducker and trigger", YELLOW, "gate", 740, 441, "Closes between hits, ducks from a sidechain and sends MIDI."),
    ("Limit", "limit", "True-peak limiter", RED, "limit", 760, 465, "Holds a master under a ceiling, with a loudness meter beside it."),
    ("DS", "ds", "De-esser", GREEN, "ds", 760, 465, "Turns down \u201cs\u201d sounds only while they\u2019re too loud."),
    ("DS-Mini", "ds-mini", "One-knob de-esser", BLUE, "dsmini", 392, 492, "The same engine as Fancy DS, behind one knob."),
    ("EQ", "eq", "Parametric equalizer", RED, "eq-dynamic", 802, 645, "Up to 24 bands on a live analyzer, with dynamic bands, mid/side and Match."),
    ("FIR", "fir", "Spectral processor", PURPLE, "fir-resonance", 760, 465, "Removes steady noise and ringing, or draws an EQ freehand."),
    ("Exciter", "exciter", "Harmonic exciter", RED, "exciter", 392, 452, "Adds brightness above a frequency you choose."),
    ("Sature", "sature", "Saturator", ORANGE, "sature", 760, 465, "Tape, Tube, Transistor and Clip colour at a steady level."),
    ("Kicker", "kicker", "Low-end enhancer", RED, "kicker", 392, 492, "Swells a kick\u2019s own note, or adds a sub below it."),
    ("Delay", "delay", "Multi-tap delay", YELLOW, "delay", 760, 448, "Up to 16 taps, in time with the song."),
    ("Verb", "verb", "Convolution reverb", PURPLE, "verb", 866, 697, "Real spaces from 32 built-in impulse responses, or your own files."),
    ("Room", "room", "Algorithmic reverb", PURPLE, "room", 760, 465, "From a small room to a long hall, with Freeze."),
]
BY = {p[1]: p for p in PLUGINS}
GROUPS = [
    ("Dynamics", "dynamics", "Plugins that react to level: they turn things down, or up, when the sound gets louder or quieter.",
     ["comp", "multicomp", "gate", "limit", "ds", "ds-mini"], "six"),
    ("Tone", "tone", "Plugins that change the colour of a sound: which frequencies are loud, and what the sound is made of.",
     ["eq", "fir", "exciter", "sature", "kicker"], "five"),
    ("Space and time", "space", "Plugins that add echoes and rooms around a sound.", ["delay", "verb", "room"], "three"),
]


def shot(p, cls="", lazy=True, alt=None):
    name, slug, kind, c, img, w, h, line = p
    return (f'<img src="{IMG}{img}.png" width="{w}" height="{h}" alt="{alt or f"Fancy {name}, {kind.lower()}"}"'
            f'{" class=" + chr(34) + cls + chr(34) if cls else ""}{" loading=" + chr(34) + "lazy" + chr(34) if lazy else " fetchpriority=" + chr(34) + "high" + chr(34)}>')


def page(file, title, desc, strip, body, accent=None, current=""):
    cur = lambda k: ' aria-current="page"' if k == current else ""
    style = f' style="--accent: {accent}"' if accent else ""
    return f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="icon" href="../favicon.svg" type="image/svg+xml">
<link rel="icon" href="../favicon-32.png" sizes="32x32">
<link rel="apple-touch-icon" href="../apple-touch-icon.png">
<link rel="stylesheet" href="site.css">
<script src="site.js" defer></script>
</head>
<body{style}>
<a class="skip" href="#main">Skip to the content</a>
<div class="strip"><div class="wrap">{strip}</div></div>
<header class="top"><div class="wrap">
  <a class="logo" href="index.html"><img src="../logo-lockup.svg" width="233" height="64" alt="Fancy Plugs, home"></a>
  <nav aria-label="Site"><ul>
    <li><a href="index.html#plugins"{cur("plugins")}>Plugins</a></li>
    <li><a href="{PDF}">Manual</a></li>
    <li><a href="{GH}/releases">What\u2019s new</a></li>
  </ul></nav>
  <a class="btn btn-main" href="{WIN}">Download</a>
  <button class="menu-btn" type="button" aria-label="Open the menu" aria-haspopup="dialog">
    <svg width="20" height="14" viewBox="0 0 20 14" aria-hidden="true"><path d="M1 1h18M1 7h18M1 13h18" stroke="#fff" stroke-width="2" stroke-linecap="round"/></svg>
  </button>
</div></header>
<dialog class="sheet" aria-label="Menu">
  <button class="btn btn-2 close" type="button">Close</button>
  <a href="index.html#plugins">Plugins</a>
  <a href="dynamics.html">Dynamics</a>
  <a href="{PDF}">Manual</a>
  <a href="{GH}/releases">What\u2019s new</a>
  <a href="{WIN}">Download {VERSION}</a>
</dialog>
<main id="main"{' class="inner"' if file != "index.html" else ""}>
{body}
</main>
<footer><div class="wrap">
  <div class="row">
    <img src="../logo-lockup.svg" width="233" height="64" alt="Fancy Plugs">
    <a href="{GH}">GitHub</a><a href="{PDF}">Manual (PDF)</a><a href="{GH}/releases">Releases</a><span>Version {VERSION}</span>
  </div>
  <p>Free to download and use. UI design based on \u201cReaPlugs (vst GUI)\u201d by Eugene Reznik, <a href="https://www.figma.com/community/file/967866927752067729">CC BY 4.0</a>, modified from the original. The Inter typeface is used under the SIL Open Font License.</p>
</div></footer>
</body>
</html>
'''


def band():
    return f'''<div class="wrap"><div class="band">
  <div>
    <h2>Download Fancy Plugs {VERSION}</h2>
    <p>All fourteen plugins in one zip, free to download and use. VST3 for Windows 10 and 11 (x64). On a Mac: VST3 and Audio Unit for macOS 11 or later, Intel and Apple silicon, as a beta. The installer copies them to your own folders and needs no administrator password.</p>
  </div>
  <div class="actions">
    <a class="btn btn-main" href="{WIN}">Windows <small>{VERSION}</small></a>
    <a class="btn btn-2" href="{MAC}">Mac <span class="tag">Beta</span></a>
  </div>
</div></div>'''


def tile(slug, group, lazy=True):
    p = BY[slug]; name, _, kind, c, img, w, h, line = p
    href = "comp.html" if slug == "comp" else (f"dynamics.html#{slug}" if group == "dynamics" else None)
    wide = " wide" if slug == "eq" else ""
    inner = (f'<div class="stage">{shot(p, lazy=lazy)}</div>'
             f'<h3><i style="--c: {c}"></i>Fancy {name}</h3><p>{kind}. {line}</p>')
    return (f'<li class="{wide.strip()}"><a class="tile" href="{href}">{inner}</a></li>' if href
            else f'<li class="{wide.strip()}"><div class="tile">{inner}</div></li>')


def home():
    comp = BY["comp"]
    roster = "".join(
        f'<li><button type="button" aria-pressed="{"true" if s == "comp" else "false"}" style="--c: {c}" data-name="{n}" data-kind="{k}" '
        f'data-src="{IMG}{i}.png" data-w="{w}" data-h="{h}"><i></i>{n}</button></li>' for n, s, k, c, i, w, h, _ in PLUGINS)
    groups = ""
    for name, slug, desc, members, shape in GROUPS:
        head = f'<a href="{slug}.html">{name}</a>' if slug == "dynamics" else name
        groups += f'''<section class="wrap" id="{slug}">
  <div class="group-head"><h2>{head}</h2><p>{desc}</p></div>
  <ul class="tiles {shape}">{"".join(tile(m, slug) for m in members)}</ul>
</section>
'''
    spots = [(12, 14, 1), (14.9, 100, 2), (59.3, 100, 3), (71, 100, 4), (92.5, 100, 5)]
    body = f'''<div class="wrap">
  <div class="hero">
    <div>
      <h1>Free effects for the whole mix: dynamics, tone and space.</h1>
      <p class="lede">Fourteen plugins for Windows and Mac that all work the same way. Learn one window and you know them all.</p>
      <div class="actions"><a class="btn btn-main" href="{WIN}">Download {VERSION}</a><a class="link" href="#plugins">See all fourteen</a></div>
    </div>
    <figure>
      {shot(comp, cls="window", lazy=False)}
      <figcaption><strong>Fancy Comp</strong> · <span>Compressor</span></figcaption>
    </figure>
  </div>
  <nav class="roster" aria-label="Show a plugin in the window above"><ul>{roster}</ul></nav>
</div>

<div id="plugins">
{groups}</div>

<section class="wrap shared">
  <h2>One window, fourteen times</h2>
  <p class="lede">Every plugin opens with the same strip and header, so what you learn in one works in the rest.</p>
  <div class="spots">
    <img class="window" src="img/header.png" width="934" height="73" alt="The header of Fancy Comp: preset name, New, Parameter, input and output count, undo, redo, A and B, Compare, the output knob and the ON switch" loading="lazy" style="width: 100%">
    {"".join(f'<span class="spot" style="left: {x}%; top: {y}%" aria-hidden="true">{n}</span>' for x, y, n in spots)}
  </div>
  <ul class="shared-list">
    <li><span class="num">1</span><b>Title strip</b><span class="t">The plugin\u2019s name in its own colour. It reads LISTENING while a Listen switch is on.</span></li>
    <li><span class="num">2</span><b>Preset browser</b><span class="t">Categories, favourites, search and your own folders. One click loads a preset and keeps the browser open.</span></li>
    <li><span class="num">3</span><b>Undo and redo</b><span class="t">For every control, in every plugin.</span></li>
    <li><span class="num">4</span><b>A, B and Compare</b><span class="t">Hold two settings side by side, copy one to the other, and compare.</span></li>
    <li><span class="num">5</span><b>Output and ON</b><span class="t">A level knob and a bypass, always in the same corner.</span></li>
    <li><span class="num" aria-hidden="true"></span><b>Typed values</b><span class="t">Click any value and type it. Frequencies take 2500 or 2.5k.</span></li>
    <li><span class="num" aria-hidden="true"></span><b>Any size</b><span class="t">Drag a window\u2019s edge to scale it from 100 % to 200 %.</span></li>
  </ul>
</section>

<section class="wrap">
  <div class="split">
    <div>
      <h2>New in 1.6.0</h2>
      <p><strong>Fancy FIR has a Resonance mode.</strong> It finds narrow peaks that stand above the spectrum around them, such as a ringing snare or a boxy vocal, and turns each one down only while it\u2019s there.</p>
      <p>Projects saved with 1.0 to 1.5 load and sound exactly as before.</p>
      <p class="more"><a class="link" href="{GH}/releases">Every release</a></p>
    </div>
    <div class="stage"><img src="{IMG}fir-resonance.png" width="760" height="465" loading="lazy" alt="Fancy FIR in Resonance mode, turning down resonances at 700 Hz and 3 kHz"></div>
  </div>
</section>

{band()}'''
    strip = '<span><a href="index.html">Fancy</a><b>Plugs</b></span><span>Version ' + VERSION + '</span>'
    return page("index.html", "Fancy Plugs: free effects for the whole mix",
                "Fourteen free VST3 and Audio Unit effects for Windows and Mac: compressors, EQs, reverbs, delay and more.", strip, body)


HUB = {
    "comp": ("For evening out a vocal, a bass or a drum bus, and for anything that needs a sidechain.",
             ["Set the threshold right on the input meter", "Compress from a sidechain or a filtered detector", "Lift quiet passages with upward compression"]),
    "multicomp": ("For a bus or a full mix where only part of the spectrum needs control.",
                  ["Split the signal into up to 8 bands that sum back flat", "Drag crossovers and thresholds on the graph", "Put a band on Mid or Side only"]),
    "gate": ("For drum mics and noisy tracks, and for ducking one track under another.",
             ["Close a mic between hits", "Look ahead 50 ms so no attack is cut", "Send a MIDI note every time it opens"]),
    "limit": ("For the end of a mix or a master.",
              ["Hold peaks under a true-peak ceiling", "Work to a loudness target with the built-in meter", "Hear only what the limiter removes"]),
    "ds": ("For harsh \u201cs\u201d, \u201csh\u201d and \u201ct\u201d sounds in a voice, or harsh highs on cymbals.",
           ["Follow the voice\u2019s level with a relative threshold", "Cut only the harsh band, or the whole signal", "Watch each cut on the history display"]),
    "ds-mini": ("For de-essing a vocal in a few seconds.",
                ["De-ess with one knob", "Pick a profile that matches the voice or instrument", "Move the setting to Fancy DS when you need more control"]),
}
WHICH = [
    ("Even out a vocal, bass or drum bus", "comp", "One compressor over the whole signal, with every control on one page."),
    ("Control only the lows or highs of a bus", "multicomp", "Each band has its own compressor, and untouched bands pass through flat."),
    ("Silence a drum mic between hits", "gate", "It opens on the hit and closes after it, with a filter on what it listens to."),
    ("Stop peaks at the end of a master", "limit", "A brickwall ceiling measured in true peak."),
    ("Soften \u201cs\u201d sounds, with full control", "ds", "Threshold, frequency, range and two ways of cutting."),
    ("Soften \u201cs\u201d sounds, quickly", "ds-mini", "One Amount knob on the same engine."),
]


def hub():
    name, slug, desc, members, _ = GROUPS[0]
    rows = ""
    for m in members:
        p = BY[m]; n, _, kind, c, img, w, h, line = p
        what, can = HUB[m]
        more = '<a class="link" href="comp.html">See Fancy Comp</a>' if m == "comp" else ""
        rows += f'''<article class="row-plugin" id="{m}" style="--c: {c}">
  <div class="stage">{shot(p, lazy=m != "comp")}</div>
  <div>
    <h2>Fancy {n}</h2>
    <p class="kind">{kind}</p>
    <p>{what}</p>
    <ul>{"".join(f"<li>{x}</li>" for x in can)}</ul>
    {more}
  </div>
</article>
'''
    table = "".join(
        f'<tr><th scope="row">{task}</th><td><a href="{"comp.html" if s == "comp" else "#" + s}">Fancy {BY[s][0]}</a></td><td class="why">{why}</td></tr>'
        for task, s, why in WHICH)
    body = f'''<div class="wrap">
  <div class="page-head">
    <div>
      <h1>{name}</h1>
      <p class="lede">{desc}</p>
    </div>
  </div>
  <nav class="onpage" aria-label="On this page"><span>On this page</span>
    {"".join(f'<a href="#{m}">{BY[m][0]}</a>' for m in members)}<a href="#which">Which one do I need?</a>
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
    strip = '<span><a href="index.html">Fancy</a><b>Dynamics</b></span><span>6 of 14 plugins</span>'
    return page("dynamics.html", "Dynamics plugins: Fancy Plugs",
                "Six free dynamics plugins: a compressor, a multiband compressor, a gate, a limiter and two de-essers.", strip, body, current="plugins")


VIEWS = [
    ("main", "Main view", "comp", "Fancy Comp\u2019s main view", [
        (5.8, 23.5, "Threshold fader", "Set the level where compression starts, on top of the input meter, so you see what crosses it."),
        (17.2, 23.5, "GR meter", "See how much is being turned down, for each channel."),
        (29.8, 17.9, "Detector input", "Compress from the track itself or from a sidechain, such as a kick ducking a bass."),
        (38.5, 39, "Ratio and Knee", "Choose how firmly it compresses, and how gradually it starts."),
        (63, 39, "Attack and Release", "Let the punch through or catch it, and let Auto set the release for you."),
        (65, 75, "Lowpass and Highpass", "Filter what the detector hears, so a bass drum doesn\u2019t pump the whole mix."),
        (92.6, 30, "Wet and Dry", "Blend compressed and uncompressed sound for parallel compression."),
        (39.6, 95.6, "Checkbox row", "Link the channels, make up the lost level automatically, and limit the output."),
    ]),
    ("upward", "Upward page", "comp-upward", "Fancy Comp\u2019s EDIT drawer on the Upward page", [
        (50, 21, "EDIT pages", "Open four pages of deeper settings without leaving the window: Detector, Character, Upward and Options."),
        (31.2, 49, "Up threshold and ratio", "Lift the quiet parts of a performance towards the loud ones."),
        (31.2, 68, "Max lift and Floor", "Limit how far it lifts, and keep it from raising the noise between phrases."),
        (54.3, 58, "Transfer curve", "See both directions of compression on one curve, with the lift being applied right now."),
        (5.8, 52, "Threshold meter", "The meter keeps showing the threshold in use while you work in the drawer."),
    ]),
]
CAN = [
    ("Compress from somewhere else", "Key it from a sidechain input, or from a filtered copy of the track."),
    ("Lift what\u2019s too quiet", "Upward compression raises soft passages, with a limit on the lift and a floor under it."),
    ("Keep it steady from verse to chorus", "Auto threshold follows the input\u2019s long-term level, so the amount of compression stays the same."),
    ("Add colour on the way", "A Warm or Bright stage, before or after the compressor, with the curves from Fancy Sature."),
    ("Shape what the detector hears", "A low shelf, a bell and a high shelf on a curve you can drag."),
    ("Catch the last peaks", "Limit output puts a limiter after the compressor."),
    ("Compress in parallel", "Separate Wet and Dry faders, kept in time with each other."),
    ("Avoid aliasing", "Oversampling up to 8x."),
]


def comp():
    p = BY["comp"]
    tabs = "".join(f'<button type="button" role="tab" id="tab-{i}" aria-controls="view-{i}" aria-selected="{"true" if k == 0 else "false"}"'
                   f'{"" if k == 0 else " tabindex=" + chr(34) + "-1" + chr(34)}>{label}</button>' for k, (i, label, *_r) in enumerate(VIEWS))
    views = ""
    for k, (i, label, img, alt, pts) in enumerate(VIEWS):
        marks = "".join(f'<button type="button" class="spot" data-n="{n}" style="left: {x}%; top: {y}%" aria-label="{n}: {t}">{n}</button>'
                        for n, (x, y, t, _d) in enumerate(pts, 1))
        lis = "".join(f'<li data-n="{n}"><span class="num">{n}</span><b>{t}</b><span class="t">{d}</span></li>' for n, (_x, _y, t, d) in enumerate(pts, 1))
        views += f'''<div class="view" role="tabpanel" id="view-{i}" aria-labelledby="tab-{i}"{"" if k == 0 else " hidden"}>
  <div class="shot"><img src="{IMG}{img}.png" width="934" height="547" alt="{alt}"{' fetchpriority="high"' if k == 0 else ' loading="lazy"'}>{marks}</div>
  <ol class="points">{lis}</ol>
</div>
'''
    body = f'''<div class="wrap">
  <div class="page-head">
    <div>
      <h1>Fancy Comp</h1>
      <p class="kind">Compressor</p>
      <p class="lede">A compressor with its threshold on the input meter, a sidechain, a filtered detector and an output limiter.</p>
    </div>
    <div class="actions"><a class="link" href="{PDF}">Manual chapter</a><a class="btn btn-main" href="{WIN}">Download</a></div>
  </div>

  <div class="explorer">
    <div class="tabs" role="tablist" aria-label="Views of Fancy Comp">{tabs}</div>
    {views}
  </div>

  <section>
    <h2>What you can do with it</h2>
    <div class="lead">
      <div>
        <h3>Set the threshold where you can see the signal.</h3>
        <p>The Threshold fader sits on top of the input meter. Pull it down until the peaks cross it, and the GR meter beside it shows what that costs.</p>
      </div>
      <div class="crop" style="aspect-ratio: 944 / 560"><img src="img/comp-threshold.png" width="944" height="560" loading="lazy" alt="The Threshold fader over the input meter, next to the GR meter and the Ratio and Knee knobs"></div>
    </div>
    <ul class="can">{"".join(f"<li><b>{a}</b><span>{b}</span></li>" for a, b in CAN)}</ul>
  </section>

  <section>
    <h2>Hear it</h2>
    <p class="note">Mockup only: this section appears on a plugin\u2019s page once its before and after clips are recorded. Until then the page has no player.</p>
    <div class="player">
      <button class="play" type="button" disabled aria-label="Play"><svg width="16" height="18" viewBox="0 0 16 18" aria-hidden="true"><path d="M1 1v16l14-8z" fill="#fff"/></svg></button>
      <div class="seg" role="group" aria-label="Version"><button type="button" disabled aria-pressed="false">Before</button><button type="button" disabled aria-pressed="true">After</button></div>
      <div class="bar"><i></i></div>
      <span class="time">0:06 / 0:16</span>
      <p class="cap">Clip name and the settings used go here.</p>
    </div>
  </section>

  <div class="two">
    <section>
      <h2>Details</h2>
      <table>
        <tbody>
          <tr><th scope="row">Formats</th><td>VST3 on Windows 10 and 11 (x64). VST3 and Audio Unit on macOS 11 or later (beta).</td></tr>
          <tr><th scope="row">Inputs</th><td>Mono or stereo, plus a stereo sidechain.</td></tr>
          <tr><th scope="row">Latency</th><td>None at the default settings. Pre-comp, oversampling and Limit output add some, and the host compensates for it.</td></tr>
          <tr><th scope="row">Oversampling</th><td>Up to 8x.</td></tr>
          <tr><th scope="row">Window</th><td>934 × 547 at 100 %, resizable to 200 %.</td></tr>
        </tbody>
      </table>
    </section>
    <section>
      <h2>Works well with</h2>
      <div class="pairs">
        <a href="dynamics.html#multicomp"><b><i style="--c: {RED}"></i>Fancy MultiComp</b><span>The same upward compression, on each band of its own.</span></a>
        <a href="dynamics.html#limit"><b><i style="--c: {RED}"></i>Fancy Limit</b><span>After the compressor, at the end of the chain.</span></a>
        <a href="index.html#tone"><b><i style="--c: {ORANGE}"></i>Fancy Sature</b><span>Comp\u2019s Character stage uses its curves. Use Sature for the full set.</span></a>
      </div>
    </section>
  </div>

  <nav class="prevnext" aria-label="More Dynamics plugins">
    <a href="dynamics.html#ds-mini">\u2039 Fancy DS-Mini</a><a class="mid" href="dynamics.html">Dynamics</a><a href="dynamics.html#multicomp">Fancy MultiComp \u203a</a>
  </nav>
</div>
{band()}'''
    strip = '<span><a href="index.html">Fancy</a><b>Comp</b></span><a href="dynamics.html">Dynamics</a>'
    return page("comp.html", "Fancy Comp, a free compressor: Fancy Plugs",
                "Fancy Comp is a free compressor with a sidechain, upward compression, Auto threshold and an output limiter.", strip, body,
                accent=TEAL, current="plugins")


os.makedirs(os.path.join(HERE, "img"), exist_ok=True)
src = Image.open(os.path.join(REPO, "docs/images/comp.png"))
src.crop((0, 0, 1868, 146)).save(os.path.join(HERE, "img/header.png"))
src.crop((0, 246, 1430, 1094)).save(os.path.join(HERE, "img/comp-threshold.png"))
for name, html in (("index.html", home()), ("dynamics.html", hub()), ("comp.html", comp())):
    open(os.path.join(HERE, name), "w", encoding="utf-8", newline="\n").write(html)
print("wrote index.html, dynamics.html, comp.html")

if "--png" in sys.argv:
    edge = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
    os.makedirs(os.path.join(HERE, "png"), exist_ok=True)
    for name in ("index", "dynamics", "comp"):
        for label, w, h, scale in (("desktop", 1440, 7000, 1), ("phone", 390, 9000, 2)):
            out = os.path.join(HERE, "png", f"{name}-{label}.png")
            url = "file:///" + os.path.join(HERE, name + ".html").replace("\\", "/")
            if label == "phone":
                # Headless Edge won't lay out narrower than about 500 px, so the phone view is an iframe 390 px wide.
                wrap = os.path.join(HERE, "png", "_phone.html")
                open(wrap, "w").write(f'<!doctype html><body style="margin:0;background:#1E1E1E"><iframe src="{url}" '
                                      f'style="border:0;display:block;width:390px;height:{h}px"></iframe>')
                url, w = "file:///" + wrap.replace("\\", "/"), 600
            subprocess.run([edge, "--headless", "--disable-gpu", "--hide-scrollbars", f"--force-device-scale-factor={scale}",
                            f"--window-size={w},{h}", "--virtual-time-budget=4000", f"--screenshot={out}",
                            url], check=True, capture_output=True)
            im = Image.open(out).convert("RGB")
            if label == "phone":
                im = im.crop((0, 0, 390 * scale, im.height)); os.remove(wrap)
            # Trim the empty page below the footer (#262626 is the footer, #1E1E1E the page).
            px, y = im.load(), im.height - 1
            while y > 0 and px[im.width // 2, y] == (30, 30, 30) and px[4, y] == (30, 30, 30):
                y -= 1
            im.crop((0, 0, im.width, y + 1)).save(out)
            print(f"{name}-{label}.png", im.width, y + 1)
