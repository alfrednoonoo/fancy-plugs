"""The intro film's script, shots and timings, for both cuts.

Times inside a scene count from the scene's start. A scene's voice starts at
"vo"; caption times count from there, read from the word timings in
wide/timing/ (made from the generated voice). A camera step is
(time, x %, y %, zoom): the point of the screenshot to move toward. x and y
come from the hotspots in site-src/data/plugins.json where one exists.
"""

ADDRESS = "alfrednoonoo.github.io/fancy-plugs"
TEAL = "#00A985"

WIDE = dict(
    strip_h=68, strip_y=0, strip_px=28, safe_x=120, safe_r=120,
    stage=(96, 104, 1728, 770), pull=(0.8, 0.8), marker=120, step=48,
    chap_px=132, chap_gap=36, chap_ring=92, chap_ring_sw=11,
    cap_x=192, cap_r=192, cap_y=904, cap_h=96, cap_px=46,
    logo_w=720, logo_y=400,
    roster_gap=64, roster_lab_gap=20, roster_lab=34, roster_row_gap="20px 48px",
    roster_px=56, roster_ring=36, roster_ring_sw=6, roster_ring_gap=18,
    end_logo_w=560, end_y=300, end_gap=76, end_gap2=44, end_gap3=20,
    btn_px=40, btn_pad="22px 56px", os_px=30, url_px=34,
)

# The apps draw over the top of the frame, everything below 1330 px and a rail of buttons at the right,
# so text stays between x 130 and 780 and below y 300. Screenshots may run wider: they aren't read at the edge.
TALL = dict(
    strip_h=72, strip_y=300, strip_px=28, safe_x=130, safe_r=300,
    stage=(40, 400, 1000, 650), pull=(1.0, 0.9), marker=150, step=40,
    chap_px=128, chap_gap=30, chap_ring=88, chap_ring_sw=11,
    cap_x=130, cap_r=300, cap_y=1070, cap_h=180, cap_px=54,
    logo_w=600, logo_y=480, head_y=720, head_h=420, head_px=88,
    roster_gap=40, roster_lab_gap=12, roster_lab=30, roster_row_gap="12px 34px",
    roster_px=46, roster_ring=32, roster_ring_sw=5, roster_ring_gap=14,
    end_logo_w=580, end_y=480, end_gap=80, end_gap2=44, end_gap3=20,
    btn_px=46, btn_pad="26px 60px", os_px=34, url_px=34,
)


def shot(data, slug, img, t, cam=None, mark=None, img2=None, xfade=None, cams=None, marks=None, crop=None, start=None):
    """One window on the stage. cam is (time, x %, y %, zoom); mark is the point the ring closes round,
    shown from the camera's arrival to the end of the shot. cams and marks give several of each:
    a camera step as above, a mark as (on, off, x %, y %). crop shows the screenshot at that scale, larger than
    the stage, starting at the point in start (x %, y %, zoom): the 9:16 cut uses it for the wide windows."""
    p = data["plugins"][slug]
    alt = {v["image"]: v["alt"] for v in p["views"]}
    cams = list(cams or ([cam] if cam else []))
    marks = list(marks or [])
    if mark:
        marks.append((cam[0] + 0.45, t[1] - 0.25, mark[0], mark[1]))
    return dict(slug=slug, img=img, img2=img2, xfade=xfade, t=t, cams=cams, marks=marks, accent=p["accent"],
                kind=p["kind"], alt=alt[img], alt2=alt.get(img2, ""), crop=crop, start=start)


def strips(group, shots, dur, first=0.0):
    """The strip names the group; the note at the right is the kind of plugin on screen."""
    out = []
    edges = [first] + [s["t"][0] for s in shots] + [dur]
    notes = [""] + [s["kind"] for s in shots]
    for a, b, note in zip(edges, edges[1:], notes):
        if b - a > 0.3:
            out.append((a, b, group, TEAL, note))
    return out


def scenes(fmt, data):
    return wide(data) if fmt == "wide" else tall(data)


def wide(d):
    s2 = [shot(d, "comp", "comp", (0.1, 9.4),
               cams=[(2.2, 40.0, 8.2, 1.3), (4.6, 32.4, 47.4, 1.5), (6.5, 50.0, 50.0, 1.0)],
               marks=[(2.75, 3.4, 14.9, 8.2), (3.4, 4.25, 65.3, 8.2), (4.25, 4.9, 59.3, 8.2), (5.15, 6.5, 32.4, 47.4)])]
    s3 = [
        shot(d, "comp", "comp", (0.95, 4.6), (1.9, 5.8, 35.0, 1.55), (5.8, 35.0)),
        shot(d, "multicomp", "multicomp", (4.6, 8.05), (5.6, 30.0, 24.0, 1.5), (30.0, 24.0)),
        shot(d, "gate", "gate", (8.05, 10.8), (8.95, 47.0, 34.0, 1.3), (47.0, 34.0)),
        shot(d, "limit", "limit", (10.8, 13.75), (11.75, 24.5, 78.0, 1.25), (24.5, 78.0)),
        shot(d, "ds", "ds", (13.75, 15.9), (14.55, 46.0, 32.0, 1.25), (46.0, 32.0)),
        shot(d, "ds-mini", "dsmini", (15.9, 18.8), (17.05, 36.0, 66.5, 1.4), (36.0, 66.5)),
    ]
    s4 = [
        shot(d, "eq", "eq", (0.85, 6.15), img2="eq-match", xfade=3.35,
             cams=[(1.8, 50.6, 25.9, 1.35), (3.45, 60.0, 47.0, 1.7)],
             marks=[(2.25, 3.3, 50.6, 25.9), (4.0, 5.9, 72.0, 49.0)]),
        shot(d, "fir", "fir-resonance", (6.15, 9.95), (7.2, 52.0, 33.0, 1.25), (52.0, 33.0)),
        shot(d, "exciter", "exciter", (9.95, 12.0), (10.75, 34.0, 33.0, 1.3)),
        shot(d, "sature", "sature", (12.0, 14.4), (12.75, 19.5, 20.8, 1.25), (19.5, 20.8)),
        shot(d, "kicker", "kicker", (14.4, 17.2), (15.6, 34.5, 59.0, 1.3), (34.5, 59.0), img2="kicker-sub", xfade=15.25),
    ]
    s5 = [
        shot(d, "delay", "delay", (0.9, 5.0), (1.95, 30.0, 30.0, 1.2)),
        shot(d, "verb", "verb", (5.0, 9.55), (6.3, 17.1, 56.2, 1.8), (17.1, 56.2), img2="verb-library", xfade=5.85),
        shot(d, "room", "room", (9.55, 13.1), (10.5, 32.0, 38.0, 1.25), (32.0, 38.0)),
    ]
    return [
        dict(id="s1", kind="intro", dur=6.0, voice="s1", vo=1.75, caps=[
            (0.0, 1.25, "Fancy Plugs."),
            (1.3, 3.75, "Fourteen free effects for the whole mix.")]),
        dict(id="s2", kind="shots", dur=9.4, voice="s2", vo=0.4, shots=s2, hold_last=True,
             strip=[(0.0, 9.4, "Plugs", TEAL, "One window")],
             caps=[(0.0, 1.95, "Every one of them works the same way."),
                   (2.25, 3.75, "Presets, A and B,"),
                   (3.8, 5.95, "undo, and any value you can type."),
                   (6.2, 8.6, "Learn one window and you know them all.")]),
        dict(id="s3", kind="shots", dur=18.8, voice="s3", vo=0.5, shots=s3, hold_last=True,
             chapter="Dynamics", chapter_at=(0.1, 0.95), strip=strips("Dynamics", s3, 18.8),
             caps=[(0.48, 1.96, "Comp puts its threshold"),
                   (1.98, 3.75, "right on the input meter."),
                   (4.15, 5.38, "MultiComp splits a mix"),
                   (5.4, 7.2, "into as many as eight bands."),
                   (7.6, 9.95, "Gate closes a drum mic between hits."),
                   (10.35, 11.26, "Limit holds a master"),
                   (11.28, 12.95, "under a true-peak ceiling."),
                   (13.3, 15.02, "DS softens harsh esses,"),
                   (15.06, 17.75, "and DS-Mini does it with one knob.")]),
        dict(id="s4", kind="shots", dur=17.2, voice="s4", vo=0.5, shots=s4, hold_last=True,
             chapter="Tone", chapter_at=(0.1, 0.85), strip=strips("Tone", s4, 17.2),
             caps=[(0.34, 2.44, "EQ gives you up to 24 bands,"),
                   (2.48, 5.3, "and Match moves a track toward a reference."),
                   (5.8, 7.54, "FIR turns down a ringing resonance"),
                   (7.56, 9.15, "only while it rings."),
                   (9.55, 11.15, "Exciter adds brightness."),
                   (11.6, 13.65, "Sature adds tape or tube colour."),
                   (14.0, 16.2, "Kicker puts a clean sub under a kick.")]),
        dict(id="s5", kind="shots", dur=13.1, voice="s5", vo=0.5, shots=s5, hold_last=True,
             chapter="Space", chapter_at=(0.1, 0.9), strip=strips("Space", s5, 13.1),
             caps=[(0.38, 2.32, "Delay builds up to 16 taps"),
                   (2.34, 4.15, "in time with the song."),
                   (4.6, 7.05, "Verb comes with 32 impulse responses,"),
                   (7.1, 8.65, "in true stereo."),
                   (9.15, 12.0, "Room goes from a small room to a long hall.")]),
        dict(id="s6", kind="roster", dur=3.9, voice="s6", vo=0.6,
             strip=[(0.0, 3.9, "Plugs", TEAL, "")],
             caps=[(0.0, 1.25, "Fourteen plugins."), (1.34, 2.6, "One window.")]),
        dict(id="s7", kind="end", dur=6.6, voice="s7", vo=0.9,
             caps=[(0.0, 3.6, "Free to download for Windows, with Mac in beta.")]),
    ]


def tall(d):
    t2 = [
        shot(d, "comp", "comp", (0.05, 3.75), (0.9, 5.8, 35.0, 1.25), (5.8, 35.0), crop=0.95, start=(5.8, 35.0, 1.0)),
        shot(d, "ds-mini", "dsmini", (3.75, 7.9), (4.9, 36.0, 66.5, 1.3), (36.0, 66.5)),
    ]
    t3 = [
        shot(d, "eq", "eq", (0.05, 3.1), (1.0, 72.0, 49.0, 1.2), (72.0, 49.0), img2="eq-match", xfade=0.7,
             crop=0.85, start=(60.0, 40.0, 1.0)),
        shot(d, "kicker", "kicker", (3.1, 6.0), (4.1, 34.5, 59.0, 1.3), (34.5, 59.0), img2="kicker-sub", xfade=3.85),
    ]
    t4 = [
        shot(d, "verb", "verb-library", (0.05, 3.7), (1.0, 17.1, 56.2, 1.15), (17.1, 56.2), crop=0.9, start=(17.0, 50.0, 1.0)),
        shot(d, "room", "room", (3.7, 7.3), (4.6, 32.0, 38.0, 1.15), (32.0, 38.0), crop=0.82, start=(32.0, 38.0, 1.0)),
    ]
    return [
        dict(id="t1", kind="intro", dur=3.4, voice="t1", vo=0.35, headline="Fourteen free effects for your mix."),
        dict(id="t2", kind="shots", dur=7.9, voice="t2", vo=0.35, shots=t2, hold_last=True,
             strip=[(0.0, 3.75, "Dynamics", TEAL, t2[0]["kind"]), (3.75, 7.9, "Dynamics", TEAL, t2[1]["kind"])],
             caps=[(0.0, 1.1, "Comp puts its threshold"), (1.12, 3.15, "right on the input meter."),
                   (3.6, 5.66, "DS-Mini softens harsh esses"), (5.7, 7.0, "with one knob.")]),
        dict(id="t3", kind="shots", dur=6.0, voice="t3", vo=0.35, shots=t3, hold_last=True,
             strip=[(0.0, 3.1, "Tone", TEAL, t3[0]["kind"]), (3.1, 6.0, "Tone", TEAL, t3[1]["kind"])],
             caps=[(0.0, 1.24, "EQ matches a track"), (1.26, 2.75, "to a reference."),
                   (3.0, 4.04, "Kicker puts a clean sub"), (4.06, 5.1, "under a kick.")]),
        dict(id="t4", kind="shots", dur=7.3, voice="t4", vo=0.35, shots=t4, hold_last=True,
             strip=[(0.0, 3.7, "Space", TEAL, t4[0]["kind"]), (3.7, 7.3, "Space", TEAL, t4[1]["kind"])],
             caps=[(0.0, 0.96, "Verb comes with"), (0.98, 2.95, "32 impulse responses."),
                   (3.6, 4.96, "Room goes from a small room"), (4.98, 6.45, "to a long hall.")]),
        dict(id="t5", kind="roster", dur=3.1, voice="t5", vo=0.4,
             strip=[(0.0, 3.1, "Plugs", TEAL, "")],
             caps=[(0.0, 1.3, "Fourteen plugins,"), (1.36, 2.45, "one window.")]),
        dict(id="t6", kind="end", dur=5.4, voice="t6", vo=0.9,
             caps=[(0.0, 2.75, "Free for Windows, with Mac in beta.")]),
    ]
