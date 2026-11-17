"""Fancy Comp's carousel: the template for the other thirteen (docs/content-plan.md, part B).

Crop boxes and ring points are in pixels of the 2x screenshot in docs/images. Every fact is
from docs/comp-manual.md or Comp's entry in site-src/data/plugins.json.
"""

SLUG = "comp"

SLIDES = [
    dict(kind="cover", heading="Compressor",
         sentence="A compressor with its threshold on the input meter.",
         # The window starts on slide 1 and finishes on slide 2.
         window=dict(image="comp")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Even out a vocal, a bass or a drum bus.",
         text="One compressor over the whole signal, with every control on one page.",
         next="The first thing to set is the threshold."),
    dict(kind="feature", heading="Threshold", layout="side",
         image="comp", crop=(0, 250, 416, 990), scale=1.2, ring=(107, 383),
         title="Set it where you can see the signal.",
         text="The Threshold fader sits on top of the input meter. Pull it down until the peaks "
              "cross it, and the GR meter beside it shows what that costs.",
         next="That turns the loud parts down. Quiet parts need the other direction."),
    dict(kind="feature", heading="Upward", layout="below",
         image="comp-upward", crop=(440, 360, 1440, 950), scale=0.92, ring=(1013, 693),
         title="Lift what’s too quiet.",
         text="Below Up threshold, Upward raises the level by Up ratio, up to Max lift. "
              "Below Floor nothing is raised, so the noise between phrases stays down.",
         next="What the compressor listens to matters too."),
    dict(kind="feature", heading="Detector", layout="above",
         image="comp-detector", crop=(470, 325, 1425, 940), scale=0.92, ring=(1153, 395),
         title="Shape what the detector hears.",
         text="A low shelf, a bell and a high shelf change what the compressor reacts to, "
              "never the sound you hear. Auto threshold keeps the compression steady from a "
              "quiet verse to a loud chorus.",
         next="It can also colour the sound on the way."),
    dict(kind="feature", heading="Character", layout="below",
         image="comp-character", crop=(460, 370, 1425, 900), scale=0.92, ring=(793, 398),
         title="Add colour, before or after.",
         text="Warm adds even harmonics and a thicker low end. Bright adds odd harmonics and "
              "edge. Drive sets how hard it works.",
         next="Comp works well with three others."),
    dict(kind="pairs", heading="Works well with",
         title="Works well with",
         next_plugin="multicomp"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Comp: a compressor with its threshold on the input meter.
Lift quiet passages with upward compression, and shape what the detector hears.
Keep it steady from verse to chorus with Auto threshold, and add Warm or Bright colour.
Free to download and use: {link}."""
