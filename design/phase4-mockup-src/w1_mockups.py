"""Phase 4 Wave 1 mockups: each plugin's real screenshot (docs/images, 2x) with the new controls
drawn over it at the plugin's own 1x coordinates, outlined in dashed yellow, and notes beside it.

    py -3 design/phase4-mockup-src/w1_mockups.py [plugin ...]

Writes design/phase4-mockup-src/w1-<plugin>.html and renders reference/phase4/<nn>__<plugin>_w1_mockup.png
with headless Edge at 2x. Coordinates and sizes in the overlays are 1x (the plugin's own units).
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "design" / "phase4-mockup-src"
OUT = ROOT / "reference" / "phase4"
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

CSS = """
*{box-sizing:border-box;margin:0;padding:0}
body{background:#121212;color:#EDEDED;font-family:Inter,sans-serif;padding:24px;width:1500px}
h1{font-size:15px;font-weight:700;color:#fff;margin-bottom:6px}
.lead{font-size:11.5px;color:#8D8D8D;line-height:1.5;max-width:1400px;margin-bottom:18px}
.sec{margin-bottom:26px}
.h{font-size:13px;font-weight:700;color:#fff;margin-bottom:4px}
.n{font-size:11px;color:#8D8D8D;line-height:1.5;max-width:1400px;margin-bottom:8px}
.row{display:flex;gap:24px;align-items:flex-start}
.shot{position:relative;flex:none;overflow:hidden;border:1px solid #2a2a2a}
.shot img{display:block}
.side{font-size:11px;color:#8D8D8D;line-height:1.55;width:330px}
.side b{color:#ddd}
.o{position:absolute}
.new{outline:1.5px dashed #FFCF07;outline-offset:2px;border-radius:3px}
.cover{position:absolute;background:var(--bg,#303030)}
.cb{display:flex;align-items:center;gap:7px;font-size:14px;color:#fff;white-space:nowrap}
.cb i{width:14px;height:14px;border-radius:3px;background:#1E1E1E;border:1px solid #8D8D8D;display:inline-block;position:relative;flex:none}
.cb i.x{background:var(--a,#00A985);border-color:var(--a,#00A985)}
.cb i.x:after{content:"";position:absolute;left:4px;top:1px;width:3.5px;height:7px;border:solid #141414;border-width:0 1.6px 1.6px 0;transform:rotate(45deg)}
.btn{background:#303030;border-radius:3px;font-size:13px;color:#fff;display:flex;align-items:center;justify-content:center;gap:6px;white-space:nowrap}
.pill{border-radius:12px;font-size:12px;display:flex;align-items:center;justify-content:center;white-space:nowrap}
.dd{background:#303030;border-radius:3px;font-size:13px;color:#fff;display:flex;align-items:center;justify-content:space-between;padding:0 8px;white-space:nowrap}
.dd:after{content:"\\2303\\2304";font-size:7px;color:#8D8D8D;writing-mode:vertical-lr;line-height:5px}
.lbl{font-size:14px;color:#fff;white-space:nowrap}
.cap{font-size:11px;color:#8D8D8D;white-space:nowrap}
.val{font-size:13px;white-space:nowrap}
.menu{background:#262626;border:1px solid #434343;border-radius:6px;padding:4px 0;font-size:13px;box-shadow:0 8px 24px rgba(0,0,0,.5)}
.menu div{padding:4px 12px;white-space:nowrap}
.menu .on{background:#383838}
"""

KNOB_JS = r"""
const P=(cx,cy,r,a)=>[cx+r*Math.sin(a),cy-r*Math.cos(a)];
function arc(cx,cy,r,a0,a1){const[x0,y0]=P(cx,cy,r,a0),[x1,y1]=P(cx,cy,r,a1);const lg=Math.abs(a1-a0)>Math.PI?1:0;return `M${x0},${y0} A${r},${r} 0 ${lg} ${a1>a0?1:0} ${x1},${y1}`}
document.querySelectorAll('[data-k]').forEach((el,i)=>{const v=+el.dataset.v,c=el.dataset.c||'#00A985',s=+(el.dataset.s||36),bi=el.dataset.bi!==undefined;
const R=s/2,cx=R+5,cy=R+5,A=Math.PI*5/6,a=-A+v*2*A,a0=bi?0:-A,tip=P(cx,cy,R*.62,a);
el.style.display='flex';el.style.flexDirection='column';el.style.alignItems='center';
el.innerHTML=`<div class="lbl" style="font-size:${el.dataset.fs||13}px">${el.dataset.l||''}</div><svg width="${s+10}" height="${s+10}">
<defs><linearGradient id="kg${i}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4F4F4F"/><stop offset="1" stop-color="#454545"/></linearGradient></defs>
<path d="${arc(cx,cy,R+2.5,Math.min(a0,a),Math.max(a0,a)+.0001)}" stroke="${c}" stroke-width="3.5" fill="none" stroke-linecap="round"/>
<circle cx="${cx}" cy="${cy}" r="${R-2}" fill="url(#kg${i})" stroke="rgba(0,0,0,.2)"/>
<polygon points="${tip.join(',')} ${P(cx,cy,R*.3,a-.42).join(',')} ${P(cx,cy,R*.3,a+.42).join(',')}" fill="${c}"/></svg>
<div class="val" style="color:${c}">${el.dataset.t||''}</div>`});
"""


def shot(image, width, height, overlays, side="", crop=None):
    """A screenshot at 1x with overlays. crop = (x, y, w, h) in 1x to show part of it."""
    x, y, w, h = crop or (0, 0, width, height)
    return (f'<div class="row"><div class="shot" style="width:{w}px;height:{h}px">'
            f'<div style="position:absolute;left:{-x}px;top:{-y}px;width:{width}px;height:{height}px">'
            f'<img src="../../docs/images/{image}" style="width:{width}px;height:{height}px">{overlays}</div></div>'
            f'<div class="side">{side}</div></div>')


def page(title, lead, sections):
    body = "".join(f'<div class="sec"><div class="h">{h}</div><div class="n">{n}</div>{content}</div>' for h, n, content in sections)
    return (f'<!doctype html><html><head><meta charset="utf-8"><title>{title}</title><style>{CSS}</style></head>'
            f'<body><h1>{title}</h1><div class="lead">{lead}</div>{body}<script>{KNOB_JS}</script></body></html>')


PLUGINS = {}


def plugin(key, number):
    def register(fn):
        PLUGINS[key] = (number, fn)
        return fn
    return register


# ---------------------------------------------------------------------------------------------
@plugin("eq", 30)
def eq():
    W, H = 802, 645
    solo = (
        '<div class="o" style="left:472px;top:229px;width:32px;height:32px;border-radius:16px;background:rgba(154,89,181,.25);border:2px solid #9A59B5;box-shadow:0 0 12px #9A59B5"></div>'
        '<div class="o new" style="left:512px;top:262px;padding:2px 8px;background:#1e1e1e;border-radius:4px;font-size:11px;color:#fff">Alt-drag: solo band 2</div>'
        '<div class="o new pill" style="left:356px;top:2px;width:96px;height:16px;background:#9A59B5;color:#141414;font-weight:700;font-size:10px;letter-spacing:.06em">LISTENING</div>'
    )
    freeze = (
        '<div class="o new btn" style="left:640px;top:78px;width:62px;height:22px;font-size:11px;background:#262626;border:1px solid #434343">&#10052; Freeze</div>'
    )
    reach = (
        '<div class="cover" style="left:588px;top:545px;width:90px;height:44px;--bg:#303030"></div>'
        '<div class="o new" style="left:590px;top:548px;display:flex;align-items:center;gap:6px"><span class="cap" style="color:#ddd;font-size:12px">Reach</span>'
        '<span class="dd" style="width:64px;height:22px;font-size:12px;color:#9A59B5">12 dB</span></div>'
    )
    autoread = '<div class="o new cap" style="left:741px;top:626px;width:54px;text-align:center;color:#00A985">auto &minus;1.8</div>'
    drawer = (
        '<div class="o new" style="left:500px;top:575px"><div class="cb"><i class="x"></i>Auto gain</div></div>'
        '<div class="o new menu" style="left:332px;top:494px;width:140px"><div>Off</div><div>Pre</div><div>Post</div><div class="on">&#10003; Pre + Post</div></div>'
    )
    return page("Fancy EQ: Phase 4 Wave 1 mockup", "docs/eq.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Band solo (Decision 2 A) and Reach (Decision 4 A)",
         "Holding Alt while dragging band 2's handle solos it: a purple halo on the handle and LISTENING in the title strip, for as long as the mouse is down. "
         "Reach sits under Sidechain in the DYN half, as a small value field in the band colour (the row has no room for a seventh knob); 12 dB is today's curve.",
         shot("eq-dynamic.png", W, H, solo + reach + autoread,
              "<b>Reach</b> types and drags like a knob's value (1 to 48 dB). <b>auto −1.8</b> under the Gain fader is Auto gain's trim, shown only while Auto gain is on.")),
        ("2 · Analyzer freeze and Pre + Post",
         "Freeze sits at the graph's top right while the analyzer is on (the F key too). The frozen spectrum stays, dimmer, under the live one until Freeze is clicked again. "
         "The Analyzer menu gains Pre + Post at the end; Auto gain goes in the drawer's free third column.",
         shot("eq.png", W, H, freeze, "Freeze is a view action: not saved, no undo step.")),
        ("3 · EDIT drawer: Auto gain, and the Analyzer menu open",
         "Auto gain under Peak hold. Off by default: today's output, to the bit.",
         shot("eq-edit.png", W, H, drawer, "Decision 3 A: the trim comes from the static curve, pink-weighted, so renders always match.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("limit", 31)
def limit():
    W, H = 760, 465
    main = (
        '<div class="o new" style="left:420px;top:436px"><div class="cb"><i></i>Gain lock</div></div>'
        '<div class="o new btn" style="left:300px;top:85px;width:112px;height:22px;font-size:12px;background:#E74D3D;color:#141414;font-weight:600">Auto to target</div>'
        '<div class="o new cap" style="left:580px;top:206px;color:#ddd;font-size:11px;line-height:1.35">max &minus;0.6<br><span style="color:#8D8D8D">0 overs</span></div>'
    )
    drawer = (
        '<div class="o new" style="left:24px;top:373px;width:346px;display:flex;justify-content:space-between;align-items:center">'
        '<span class="lbl" style="color:#8D8D8D">Release curve</span><span class="dd" style="width:170px;height:24px">Exponential</span></div>'
        '<div class="o new" style="left:24px;top:418px;width:346px;display:flex;justify-content:space-between;align-items:center">'
        '<span class="lbl" style="color:#8D8D8D">DC filter</span><span class="cb"><i></i>5 Hz high-pass</span></div>'
    )
    return page("Fancy Limit: Phase 4 Wave 1 mockup", "docs/limit.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Main window: Gain lock, Auto to target, peak statistics",
         "Gain lock joins the footer after Listen to removed. Auto to target sits in the top row, left of the Target menu it works towards; while it measures (10 s) it shows a progress ring, "
         "and with Gain lock on it's greyed out. The loudness panel's True peak gets its held maximum and the count of overs beside it; a click on either resets both.",
         shot("limit.png", W, H, main, "The button is an action, one undo step: it moves Threshold once, by <b>target − measured</b>.")),
        ("2 · EDIT drawer: Release curve and DC filter",
         "Two rows under Loudness, in the drawer's own style. Exponential and off are today's sound.",
         shot("limit-edit.png", W, H, drawer, "")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("gate", 32)
def gate():
    W, H = 740, 441
    edit = '<div class="o new" style="left:388px;top:83px;font-size:14px;font-weight:600;color:#F1C315;letter-spacing:.04em">&#9881; EDIT</div>'
    menu = ('<div class="o new menu" style="left:94px;top:104px;width:150px"><div>Main input L+R</div><div>Main L</div><div>Main R</div>'
            '<div>Sidechain L+R</div><div>Sidechain L</div><div>Sidechain R</div><div class="on">&#10003; Mid</div><div>Side</div></div>')
    drawer = (
        '<div class="o" style="left:120px;top:110px;width:456px;height:282px;background:#262626">'
        '<div class="o" style="left:18px;top:14px;font-size:14px;font-weight:600;letter-spacing:.06em">EDIT</div>'
        '<div class="o btn" style="left:380px;top:10px;width:62px;height:24px;border-radius:12px">Done</div>'
        '<div class="o" style="left:18px;top:56px"><div class="cb" style="--a:#F1C315"><i class="x"></i>MIDI trigger</div></div>'
        '<div class="o cap" style="left:44px;top:78px;width:180px;white-space:normal;line-height:1.4">Note-on opens, note-off closes. The detector is ignored.</div>'
        '<div class="o" style="left:18px;top:130px;display:flex;align-items:center;gap:10px"><span class="btn" style="width:84px;height:26px;background:#F1C315;color:#141414;font-weight:600">Learn</span>'
        '<span class="cap" style="width:150px;white-space:normal;line-height:1.4">Listens for 3 s and sets Threshold</span></div>'
        '<div class="o" style="left:250px;top:50px" data-k data-v="1" data-c="#F1C315" data-s="40" data-l="Ratio" data-t="inf:1"></div>'
        '<div class="o" style="left:350px;top:50px" data-k data-v="0" data-c="#3598DB" data-s="40" data-l="De-bleed" data-t="0 %"></div>'
        '<div class="o cap" style="left:250px;top:150px;width:190px;white-space:normal;line-height:1.4">Ratio below inf:1 makes Gate an expander, never deeper than Range. '
        'De-bleed needs Lowpass and Highpass set: out-of-band thumps can\'t open the gate.</div>'
        '</div>'
    )
    return page("Fancy Gate: Phase 4 Wave 1 mockup", "docs/gate.md, Phase 4 Wave 1 (Decision 4 A). New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Main window: an EDIT link, and Mid and Side in the detector menu",
         "Gate is the only plugin without an EDIT drawer, and its footer and cards are full. An EDIT link in the top row's free space (after the latency note) opens one. "
         "The Detector input menu gains Mid and Side at the end, as Comp has them.",
         shot("gate.png", W, H, edit + menu, "Nothing else on the main window moves.")),
        ("2 · The EDIT drawer, open over the knob cards",
         "MIDI trigger, Learn, Ratio and De-bleed. The threshold meter, the graph and the faders stay visible, so Learn's result shows as it lands.",
         shot("gate.png", W, H, edit + '<div class="new o" style="left:120px;top:110px;width:456px;height:282px"></div>' + drawer, "")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("ds", 33)
def ds():
    W, H = 760, 465
    main = (
        '<div class="o new btn" style="left:396px;top:433px;width:76px;height:24px;background:#2DCC70;color:#141414;font-weight:600">Learn</div>'
        '<div class="o new cap" style="left:480px;top:438px;color:#8D8D8D">2 s of playback</div>'
    )
    drawer = (
        '<div class="o new" style="left:24px;top:253px;width:346px;display:flex;justify-content:space-between;align-items:center">'
        '<span class="lbl" style="color:#8D8D8D">Filter slope</span><span class="dd" style="width:170px;height:24px">12 dB/oct</span></div>'
        '<div class="o new" style="left:24px;top:293px;width:346px;display:flex;justify-content:space-between;align-items:center">'
        '<span class="lbl" style="color:#8D8D8D">Ceiling</span><span class="dd" style="width:170px;height:24px">Off</span></div>'
    )
    return page("Fancy DS: Phase 4 Wave 1 mockup", "docs/ds.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Footer: Learn",
         "Learn sits in the footer's free space after Link channels. While it listens (2 s) it shows a progress ring; then Threshold moves, and the history's threshold line with it.",
         shot("ds.png", W, H, main, "An action, one undo step. Decision 1 A: Threshold at the 90th percentile of the band level.")),
        ("2 · EDIT drawer: Filter slope and Ceiling",
         "Two rows under Split filter. 12 dB/oct and Off are today's sound.",
         shot("ds-edit.png", W, H, drawer, "")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("dsmini", 34)
def dsmini():
    W, H = 392, 492
    arc = ('<svg class="o" style="left:68px;top:270px" width="140" height="140" viewBox="0 0 140 140">'
           '<path d="M 26.8 107.3 A 50 50 0 1 1 120 70" stroke="#9fd0f5" stroke-width="2" fill="none" stroke-dasharray="3 2"/></svg>')
    main = (
        arc +
        '<div class="o new pill" style="left:118px;top:432px;width:40px;height:18px;background:#3598DB;color:#141414;font-weight:700;font-size:11px">Auto</div>'
        '<div class="o new cap" style="left:166px;top:434px;color:#9fd0f5">now 7.4</div>'
    )
    off = '<div class="o new pill" style="left:118px;top:432px;width:40px;height:18px;background:#262626;border:1px solid #434343;color:#8D8D8D;font-weight:700;font-size:11px">Auto</div>'
    return page("Fancy DS-Mini: Phase 4 Wave 1 mockup", "docs/ds-mini.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Auto Amount on",
         "A small Auto pill under the Amount value. While it's on, a thin dashed arc inside the ring shows the working value (here 7.4 against the knob's 6.0), "
         "and 'now 7.4' beside the pill says it in numbers. The knob still sets the centre; Auto moves the working value within ±30 % of the travel (Decision 1 A).",
         shot("dsmini.png", W, H, main, "Off (the default) is today's DS-Mini, bit for bit.") +
         '<div style="height:14px"></div>' +
         shot("dsmini.png", W, H, off, "Auto off: the pill is grey and nothing else shows.", crop=(0, 400, 392, 92))),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("multicomp", 35)
def multicomp():
    W, H = 760, 609
    main = (
        '<div class="o new btn" style="left:556px;top:104px;width:56px;height:20px;font-size:11px;background:#262626;border:1px solid #434343">Learn</div>'
        '<div class="o new btn" style="left:458px;top:282px;width:30px;height:24px;font-size:13px">M</div>'
    )
    return page("Fancy MultiComp: Phase 4 Wave 1 mockup", "docs/multicomp.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Learn crossovers and band mute",
         "Learn at the graph's top right: listens for 5 s and places the crossovers (Decision 1 A, equal pink-weighted energy per band). "
         "M (mute) in the band row, before the channel menu: a muted band is out of the sum entirely, where the power button keeps it in the mix unprocessed.",
         shot("multicomp.png", W, H, main, "Learn is an action, one undo step, and moves only the crossovers. M lights in the band colour when on.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("delay", 38)
def delay():
    W, H = 760, 448
    main = '<div class="o new btn" style="left:120px;top:89px;width:76px;height:22px;font-size:12px;background:#262626;border:1px solid #434343">&#10052; Freeze</div>'
    frozen = ('<div class="o new btn" style="left:120px;top:89px;width:76px;height:22px;font-size:12px;background:#F1C315;color:#141414;font-weight:600">&#10052; Frozen</div>'
              '<div class="o" style="left:14px;top:84px;width:608px;height:150px;background:rgba(150,200,255,.06);border-radius:6px;pointer-events:none"></div>')
    drawer = ('<div class="o new" style="left:560px;top:300px;display:flex;align-items:center;gap:10px"><span class="lbl">Ceiling</span>'
              '<span class="dd" style="width:72px;height:24px">Off</span></div>')
    return page("Fancy Delay: Phase 4 Wave 1 mockup", "docs/delay.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Freeze on the tap graph",
         "The footer is full, and Freeze is something you play, so it sits on the tap graph, top left, after 'dry'. "
         "Frozen, it lights in the accent and the graph is tinted; the loops hold, and new input isn't heard.",
         shot("delay.png", W, H, main, "Freeze is a parameter, so it can be automated for build-ups.") +
         '<div style="height:14px"></div>' + shot("delay.png", W, H, frozen, "Frozen.", crop=(0, 70, 760, 180))),
        ("2 · EDIT drawer: Ceiling",
         "After the Ducking group. Off is today's sound.",
         shot("delay-edit.png", W, H, drawer, "")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("exciter", 39)
def exciter():
    W, H = 392, 452
    main = ('<div class="o new" style="left:24px;top:384px;--a:#E74D3D"><div class="cb"><i></i>Listen to added harmonics</div></div>'
            '<div class="o new cap" style="left:300px;top:432px;width:72px;text-align:center;color:#ddd">THD 2.4 %</div>')
    drawer = ('<div class="o new" style="left:32px;top:300px;width:200px;display:flex;justify-content:space-between;align-items:center">'
              '<span class="lbl">Ceiling</span><span class="dd" style="width:92px;height:24px">Off</span></div>'
              '<div class="o new" style="left:32px;top:342px"><div class="cb"><i></i>Auto gain</div></div>')
    return page("Fancy Exciter: Phase 4 Wave 1 mockup", "docs/exciter.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Main window: the listen switch and the THD read-out",
         "Listen to added harmonics sits above the footer, in the accent when on (temporary, never saved). "
         "THD under the Out meter: the harmonics this setting adds to a tone an octave above Freq (Decision 1 A), updated as the knobs move.",
         shot("exciter.png", W, H, main, "")),
        ("2 · EDIT drawer: Ceiling and Auto gain",
         "Under Oversampling. Off and off are today's sound.",
         shot("exciter-edit.png", W, H, drawer, "")),
    ])


# ---------------------------------------------------------------------------------------------
def tight(x, y, label, value, colour, v=0.5, bi=False, size=30):
    return (f'<div class="o" style="left:{x}px;top:{y}px;width:58px" data-k data-v="{v}" data-c="{colour}" data-s="{size}" '
            f'data-fs="11" data-l="{label}" data-t="{value}"{" data-bi" if bi else ""}></div>')


@plugin("verb", 36)
def verb():
    W, H = 866, 697
    blue = "#3598DB"
    card = (
        '<div class="o" style="left:452px;top:273px;width:202px;height:352px;background:#2c2c2c"></div>'
        '<div class="o new" style="left:452px;top:273px;width:202px;height:352px"></div>'
        f'<div class="o" style="left:466px;top:287px;font-size:16px;font-weight:600;display:flex;gap:8px;align-items:center"><span style="color:{blue}">&#9211;</span>EQ</div>'
        '<div class="o" style="left:630px;top:284px;font-size:16px;color:#8D8D8D">&times;</div>'
        '<svg class="o" style="left:464px;top:318px" width="178" height="52"><rect width="178" height="52" rx="4" fill="#141414"/>'
        '<line x1="0" y1="26" x2="178" y2="26" stroke="#303030"/>'
        f'<path d="M0,34 C30,34 40,26 60,26 S100,12 110,12 S125,26 140,26 S160,30 178,32" stroke="{blue}" stroke-width="2" fill="none"/></svg>'
        '<div class="o cap" style="left:466px;top:378px">LOW SHELF</div>'
        + tight(462, 390, "Freq", "120 Hz", blue) + tight(522, 390, "Gain", "&minus;2.0", blue, 0.42, True) +
        '<div class="o cap" style="left:466px;top:462px">BELL</div>'
        + tight(462, 474, "Freq", "2.0 kHz", blue, 0.6) + tight(522, 474, "Gain", "+3.0", blue, 0.62, True) + tight(582, 474, "Q", "1.0", blue, 0.4) +
        '<div class="o cap" style="left:466px;top:546px">HIGH SHELF</div>'
        + tight(462, 554, "Freq", "8.0 kHz", blue, 0.7) + tight(522, 554, "Gain", "&minus;1.5", blue, 0.45, True)
    )
    library = (
        '<div class="o new menu" style="left:24px;top:412px;width:248px;padding:6px 0">'
        '<div style="padding:4px 8px"><div style="background:#141414;border-radius:3px;padding:3px 8px;color:#8D8D8D;font-size:12px">&#128269; hall</div></div>'
        '<div style="color:#8D8D8D;font-size:11px;padding:4px 12px 2px">FAVOURITES</div>'
        '<div class="on" style="display:flex;justify-content:space-between">&#10003; Concert Hall<span style="color:#F1C315">&#9733;</span></div>'
        '<div style="color:#8D8D8D;font-size:11px;padding:4px 12px 2px">HALLS</div>'
        '<div style="display:flex;justify-content:space-between">Chamber Hall<span style="color:#5a5a5a">&#9734;</span></div>'
        '<div style="display:flex;justify-content:space-between">Large Hall<span style="color:#5a5a5a">&#9734;</span></div>'
        '<div style="display:flex;justify-content:space-between">Stone Hall<span style="color:#5a5a5a">&#9734;</span></div>'
        '</div>'
        '<div class="o new menu" style="left:520px;top:120px;width:150px"><div>Export IR&hellip;</div><div style="color:#5a5a5a">Reset trim</div></div>'
    )
    drop = ('<div class="o" style="left:0px;top:273px;width:733px;height:352px;background:rgba(154,89,181,.18);border:2px dashed #9A59B5;'
            'display:flex;align-items:center;justify-content:center"><span style="background:#1e1e1e;border:1px solid #9A59B5;border-radius:6px;padding:8px 14px;font-size:15px;color:#fff">Drop to add a File module: Bright Room.wav</span></div>')
    return page("Fancy Verb: Phase 4 Wave 1 mockup", "docs/verb.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · The EQ module",
         "A new card from the add panel, in the rack like LP/HP: a small response curve and three bands (low shelf, bell, high shelf) as tight knobs in the module blue. "
         "It shapes the IR, offline, so it costs nothing while playing.",
         shot("verb-library.png", W, H, card, "A band at 0 dB does nothing. Older Verbs drop the card and play the IR without it.")),
        ("2 · Library search and favourites, and Export IR",
         "The library menu gets a search field at its top (by name and category, as you type) and a star on each row; starred IRs are listed first under Favourites, kept in your settings, not in projects. "
         "Right-click the IR display (or the File card's menu) for Export IR&hellip;: the finished IR, after trim, as a 24-bit WAV.",
         shot("verb-library.png", W, H, library, "")),
        ("3 · Drag and drop",
         "Dragging an audio file over the rack tints it; dropping adds a File module with that file at the end of the chain (or replaces the selected File module's file).",
         shot("verb-library.png", W, H, drop, "Equal loudness (Decision 1 A) is one checkbox in the SETUP drawer, with the applied trim beside it, so it isn't drawn here.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("room", 37)
def room():
    W, H = 760, 465
    purple, blue = "#9A59B5", "#3598DB"
    drawer = (
        '<div class="o new" style="left:396px;top:124px;width:252px;height:190px"></div>'
        '<div class="o cap" style="left:400px;top:126px;font-weight:600;letter-spacing:.06em">REVERB EQ</div>'
        + tight(394, 138, "Low", "200 Hz", blue, 0.35) + tight(444, 138, "Gain", "0.0", blue, 0.5, True)
        + tight(494, 138, "Mid", "1.0 kHz", blue, 0.5) + tight(544, 138, "Gain", "0.0", blue, 0.5, True) + tight(594, 138, "Q", "1.00", blue, 0.4)
        + tight(394, 224, "High", "6.0 kHz", blue, 0.6) + tight(444, 224, "Gain", "0.0", blue, 0.5, True)
        + tight(530, 224, "Mod rate", "1.00&times;", purple, 0.5) +
        '<div class="o new" style="left:22px;top:326px;width:626px;height:92px"></div>'
        '<div class="o cap" style="left:26px;top:330px;font-weight:600;letter-spacing:.06em">GATE</div>'
        '<div class="o" style="left:26px;top:358px"><div class="cb" style="--a:#9A59B5"><i></i>Gate the tail</div></div>'
        + tight(170, 340, "Threshold", "&minus;30 dB", purple, 0.5) + tight(240, 340, "Hold", "80 ms", purple, 0.3) + tight(310, 340, "Decay", "40 ms", purple, 0.25) +
        '<div class="o" style="left:420px;top:358px"><div class="cb" style="--a:#9A59B5"><i></i>Auto gain</div></div>'
        '<div class="o cap" style="left:446px;top:380px;width:190px;white-space:normal;line-height:1.4">Keeps the level steady as Size and Decay change</div>'
    )
    return page("Fancy Room: Phase 4 Wave 1 mockup", "docs/room.md, Phase 4 Wave 1, Decision 1 A: everything in the EDIT drawer; the main window doesn't change.", [
        ("EDIT drawer: Reverb EQ, Mod rate, Gate and Auto gain",
         "The drawer's free right half takes the Reverb EQ (after Low cut and High cut on the main window) and Mod rate; a Gate row and Auto gain go along the bottom. "
         "Flat bands, 1.00&times;, gate off and Auto gain off are today's sound.",
         shot("room-edit.png", W, H, drawer, "Mod keeps its meaning (depth); Mod rate scales every line's own rate together.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("kicker", 40)
def kicker():
    W, H = 392, 492
    red = "#E74D3D"
    main = '<div class="o new" style="left:331px;top:122px;width:6px;height:315px;background:#141414"><div style="position:absolute;bottom:0;width:6px;height:120px;background:#E74D3D"></div></div>'
    drawer = (
        '<div class="o new" style="left:14px;top:330px;width:262px;height:150px"></div>'
        '<div class="o cap" style="left:20px;top:334px;font-weight:600;letter-spacing:.06em">SUB</div>'
        + tight(14, 346, "Length", "Off", red, 1.0) + tight(84, 346, "Make-room", "60 Hz", red, 0.3) +
        '<div class="o" style="left:160px;top:356px"><div class="cb" style="font-size:12px"><i></i>Make-room</div></div>'
        '<div class="o" style="left:160px;top:382px"><div class="cb" style="font-size:12px"><i></i>Invert sub</div></div>'
        '<div class="o" style="left:20px;top:440px"><div class="cb" style="font-size:12px"><i></i>Auto gain</div></div>'
        '<div class="o" style="left:130px;top:436px;display:flex;align-items:center;gap:6px"><span class="lbl" style="font-size:12px">Ceiling</span>'
        '<span class="dd" style="width:64px;height:22px;font-size:12px">Off</span></div>'
    )
    return page("Fancy Kicker: Phase 4 Wave 1 mockup", "docs/kicker.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · EDIT drawer (Sub mode): the Sub group, Auto gain and Ceiling",
         "Under Soft clip. Sub length (Off at the top of its knob), Make-room with its frequency, Invert sub; then Auto gain and Ceiling. "
         "In Boost mode the Sub group dims. Off, off, off and Off are today's sound.",
         shot("kicker-sub-edit.png", W, H, drawer, "")),
        ("2 · The sub meter (Decision 1 A)",
         "A 6 px bar in Kicker's red between In and Out: the generated sub's level alone. Sub mode only.",
         shot("kicker-sub-edit.png", W, H, main, "Nothing else moves.", crop=(270, 90, 122, 402))),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("fir", 41)
def fir():
    W, H = 760, 465
    green = "#2DCC70"
    main = (
        '<div class="cover" style="left:582px;top:330px;width:62px;height:56px;--bg:#262626"></div>'
        + tight(584, 328, "Tilt", "0 %", green, 0.5, True) +
        '<div class="o new" style="left:584px;top:328px;width:58px;height:62px"></div>'
        '<div class="o new btn" style="left:455px;top:84px;width:46px;height:24px;font-size:12px;background:#2DCC70;color:#141414;font-weight:600">Learn</div>'
    )
    return page("Fancy FIR: Phase 4 Wave 1 mockup", "docs/fir.md, Phase 4 Wave 1. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("Resonance tilt, and Learn",
         "Tilt sits in the Resonance card beside FFT size, a small bipolar knob (0 % is today). "
         "Learn appears in the top row between the Mode switch and the Edit menu, in Gate and Compressor modes only (drawn here on the Resonance screenshot for its place): "
         "it listens for 3 s and places the threshold curve's points (Decision 1 A: smoothed to a third of an octave, so it stays editable).",
         shot("fir-resonance.png", W, H, main, "Learn is an action, one undo step.")),
    ])


# ---------------------------------------------------------------------------------------------
def render(key):
    number, fn = PLUGINS[key]
    html = SRC / f"w1-{key}.html"
    html.write_text(fn(), encoding="utf-8")
    png = OUT / f"{number}__{key}_w1_mockup.png"
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=2",
                    "--window-size=1548,2400", f"--screenshot={png}", html.as_uri()], capture_output=True)
    # Trim the empty page below the content.
    from PIL import Image
    image = Image.open(png).convert("RGB")
    pixels = image.load()
    bottom = image.height - 1
    while bottom > 0 and all(pixels[x, bottom] == (18, 18, 18) for x in range(0, image.width, 7)):
        bottom -= 1
    image.crop((0, 0, image.width, min(image.height, bottom + 40))).save(png)
    print("wrote", png.relative_to(ROOT))


if __name__ == "__main__":
    for key in sys.argv[1:] or PLUGINS:
        render(key)
