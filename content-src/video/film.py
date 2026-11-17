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

# The 9:16 cut is laid out for the tall frame, not cut down from the wide one: one centred column of
# group line, caption, picture. TikTok, Reels and Shorts draw over the frame, and `hyperframes check` knows
# where: words are clear of all three between y 288 and 1248, from x 120 to 888 above y 840 and from x 120
# to 780 below it. Centred on x 540 that is a column 696 px wide above y 840 and 480 px wide below, so the
# words sit in the upper column and the picture, which needs no reading at its edges, runs below them.
TALL = dict(
    centred=True, strip_h=44, strip_y=296, strip_px=28, safe_x=192, safe_r=192,
    stage=(40, 600, 1000, 1020), stage_r=28, pull=(1.0, 1.0), slack=120, marker=180, step=40,
    chap_px=128, chap_gap=30, chap_ring=88, chap_ring_sw=11,
    cap_x=192, cap_r=192, cap_y=356, cap_h=210, cap_px=56, cap_w=600,
    logo_w=480, logo_y=940, head_y=560, head_h=270, head_px=80,
    roster_x=300, roster_box=(600, 640), roster_gap=34, roster_lab_gap=10, roster_lab=28, roster_row_gap="8px 30px",
    roster_px=44, roster_ring=30, roster_ring_sw=5, roster_ring_gap=14,
    end_logo_w=480, end_y=620, end_gap=64, end_gap2=40, end_gap3=44,
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
    s2 = [shot(d, "comp", "comp", (0.1, 9.3),
               cams=[(2.25, 40.0, 8.2, 1.3), (4.3, 32.4, 47.4, 1.5), (6.5, 50.0, 50.0, 1.0)],
               marks=[(2.9, 3.35, 14.9, 8.2), (3.35, 3.95, 65.3, 8.2), (3.95, 4.65, 59.3, 8.2), (4.95, 6.5, 32.4, 47.4)])]
    s3 = [
        shot(d, "comp", "comp", (1.45, 4.8), (2.2, 5.8, 35.0, 1.55), (5.8, 35.0)),
        shot(d, "multicomp", "multicomp", (4.8, 8.45), (5.95, 30.0, 24.0, 1.5), (30.0, 24.0)),
        shot(d, "gate", "gate", (8.45, 11.35), (9.35, 47.0, 34.0, 1.3), (47.0, 34.0)),
        shot(d, "limit", "limit", (11.35, 14.65), (12.45, 24.5, 78.0, 1.25), (24.5, 78.0)),
        shot(d, "ds", "ds", (14.65, 16.8), (15.5, 46.0, 32.0, 1.25), (46.0, 32.0)),
        shot(d, "ds-mini", "dsmini", (16.8, 19.5), (17.9, 36.0, 66.5, 1.4), (36.0, 66.5)),
    ]
    s4 = [
        shot(d, "eq", "eq", (1.45, 6.25), img2="eq-match", xfade=3.8,
             cams=[(2.4, 50.6, 25.9, 1.35), (3.9, 60.0, 47.0, 1.7)],
             marks=[(2.85, 3.8, 50.6, 25.9), (4.35, 5.95, 72.0, 49.0)]),
        shot(d, "fir", "fir-resonance", (6.25, 10.35), (7.45, 52.0, 33.0, 1.25), (52.0, 33.0)),
        shot(d, "exciter", "exciter", (10.35, 12.5), (11.1, 34.0, 33.0, 1.3)),
        shot(d, "sature", "sature", (12.5, 14.9), (13.25, 19.5, 20.8, 1.25), (19.5, 20.8)),
        shot(d, "kicker", "kicker", (14.9, 17.6), (16.1, 34.5, 59.0, 1.3), (34.5, 59.0), img2="kicker-sub", xfade=15.8),
    ]
    s5 = [
        shot(d, "delay", "delay", (1.1, 4.8), (2.05, 30.0, 30.0, 1.2)),
        shot(d, "verb", "verb", (4.8, 9.8), (6.25, 17.1, 56.2, 1.8), (17.1, 56.2), img2="verb-library", xfade=5.75),
        shot(d, "room", "room", (9.8, 13.0), (10.75, 32.0, 38.0, 1.25), (32.0, 38.0)),
    ]
    return [
        dict(id="s1", kind="intro", dur=6.0, voice="s1", vo=1.75, caps=[
            (0.0, 0.95, "Fancy Plugs."),
            (1.2, 3.45, "Fourteen free effects for the whole mix.")]),
        dict(id="s2", kind="shots", dur=9.3, voice="s2", vo=0.4, shots=s2, hold_last=True,
             strip=[(0.0, 9.3, "Plugs", TEAL, "One window")],
             caps=[(0.0, 2.0, "Every one of them works the same way."),
                   (2.45, 3.45, "Presets, A and B,"),
                   (3.48, 5.85, "undo, and any value you can type."),
                   (6.3, 8.4, "Learn one window and you know them all.")]),
        dict(id="s3", kind="shots", dur=19.5, voice="s3", vo=0.5, shots=s3, hold_last=True,
             chapter="Dynamics", chapter_at=(0.1, 1.45), strip=strips("Dynamics", s3, 19.5),
             caps=[(1.0, 2.08, "Comp puts its threshold"),
                   (2.1, 3.9, "right on the input meter."),
                   (4.35, 5.78, "MultiComp splits a mix"),
                   (5.8, 7.45, "into as many as eight bands."),
                   (8.0, 10.4, "Gate closes a drum mic between hits."),
                   (10.9, 11.96, "Limit holds a master"),
                   (11.98, 13.7, "under a true-peak ceiling."),
                   (14.25, 15.98, "DS softens harsh esses,"),
                   (16.0, 18.5, "and DS-Mini does it with one knob.")]),
        dict(id="s4", kind="shots", dur=17.6, voice="s4", vo=0.5, shots=s4, hold_last=True,
             chapter="Tone", chapter_at=(0.1, 1.45), strip=strips("Tone", s4, 17.6),
             caps=[(0.95, 3.0, "EQ gives you up to 24 bands,"),
                   (3.02, 5.3, "and Match moves a track toward a reference."),
                   (5.95, 7.84, "FIR turns down a ringing resonance"),
                   (7.86, 9.4, "only while it rings."),
                   (10.0, 11.4, "Exciter adds brightness."),
                   (12.15, 14.1, "Sature adds tape or tube colour."),
                   (14.55, 16.7, "Kicker puts a clean sub under a kick.")]),
        dict(id="s5", kind="shots", dur=13.0, voice="s5", vo=0.5, shots=s5, hold_last=True,
             chapter="Space", chapter_at=(0.1, 1.1), strip=strips("Space", s5, 13.0),
             caps=[(0.6, 2.32, "Delay builds up to 16 taps"),
                   (2.34, 3.85, "in time with the song."),
                   (4.45, 7.06, "Verb comes with 32 impulse responses,"),
                   (7.08, 8.75, "in true stereo."),
                   (9.45, 12.2, "Room goes from a small room to a long hall.")]),
        dict(id="s6", kind="roster", dur=3.9, voice="s6", vo=0.6,
             strip=[(0.0, 3.9, "Plugs", TEAL, "")],
             caps=[(0.0, 1.0, "Fourteen plugins."), (1.2, 2.05, "One window.")]),
        dict(id="s7", kind="end", dur=6.6, voice="s7", vo=0.9,
             caps=[(0.0, 3.05, "Free to download for Windows, with Mac in beta.")]),
    ]


def tall(d):
    t2 = [
        shot(d, "comp", "comp", (0.05, 3.5), (0.8, 5.8, 35.0, 1.5), (5.8, 35.0), crop="fill", start=(5.8, 35.0, 1.0)),
        shot(d, "ds-mini", "dsmini", (3.5, 7.1), (4.7, 36.0, 66.5, 1.3), (36.0, 66.5)),
    ]
    t3 = [
        shot(d, "eq", "eq", (0.05, 3.0), (1.0, 72.0, 49.0, 1.2), (72.0, 49.0), img2="eq-match", xfade=0.7,
             crop="fill", start=(60.0, 45.0, 1.0)),
        shot(d, "kicker", "kicker", (3.0, 5.6), (3.9, 34.5, 59.0, 1.3), (34.5, 59.0), img2="kicker-sub", xfade=3.7),
    ]
    t4 = [
        shot(d, "verb", "verb-library", (0.05, 3.6), (0.9, 17.1, 56.2, 1.3), (17.1, 56.2), crop="fill", start=(17.0, 50.0, 1.0)),
        shot(d, "room", "room", (3.6, 6.9), (4.5, 32.0, 38.0, 1.1), (32.0, 38.0), crop="fill", start=(32.0, 38.0, 1.0)),
    ]
    return [
        dict(id="t1", kind="intro", dur=3.2, voice="t1", vo=0.35, headline="Fourteen free effects for your mix.",
             subs=[(0.0, 2.0, "Fourteen free effects for your mix.")]),
        dict(id="t2", kind="shots", dur=7.1, voice="t2", vo=0.35, shots=t2, hold_last=True,
             strip=[(0.0, 3.5, "Dynamics", TEAL, t2[0]["kind"]), (3.5, 7.1, "Dynamics", TEAL, t2[1]["kind"])],
             caps=[(0.0, 2.9, "Comp puts its threshold right on the input meter."),
                   (3.25, 6.5, "DS-Mini softens harsh esses with one knob.")]),
        dict(id="t3", kind="shots", dur=5.6, voice="t3", vo=0.35, shots=t3, hold_last=True,
             strip=[(0.0, 3.0, "Tone", TEAL, t3[0]["kind"]), (3.0, 5.6, "Tone", TEAL, t3[1]["kind"])],
             caps=[(0.0, 2.4, "EQ matches a track to a reference."),
                   (2.75, 5.0, "Kicker puts a clean sub under a kick.")]),
        dict(id="t4", kind="shots", dur=6.9, voice="t4", vo=0.35, shots=t4, hold_last=True,
             strip=[(0.0, 3.6, "Space", TEAL, t4[0]["kind"]), (3.6, 6.9, "Space", TEAL, t4[1]["kind"])],
             caps=[(0.0, 3.0, "Verb comes with 32 impulse responses."),
                   (3.35, 6.3, "Room goes from a small room to a long hall.")]),
        dict(id="t5", kind="roster", dur=3.0, voice="t5", vo=0.4,
             strip=[(0.0, 3.0, "Plugs", TEAL, "")],
             caps=[(0.0, 2.55, "Fourteen plugins, one window.")]),
        dict(id="t6", kind="end", dur=5.0, voice="t6", vo=0.9,
             caps=[(0.0, 2.6, "Free for Windows, with Mac in beta.")]),
    ]
