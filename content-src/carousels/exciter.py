"""Fancy Exciter's carousel. Facts from docs/exciter-manual.md and site-src/data/plugins.json.
ring_size is the ring's diameter on the slide, for knobs drawn larger than the default ring."""

SLIDES = [
    dict(kind="cover", heading="Harmonic exciter",
         sentence="Adds brightness above a frequency you choose.",
         window=dict(image="exciter")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Add brightness and presence.",
         text="It makes new harmonics above a frequency you choose, instead of boosting what’s there.",
         next="Start by choosing where."),
    dict(kind="feature", heading="Freq", layout="below",
         image="exciter", crop=(20, 200, 520, 480), scale=1.6, ring=(155, 348), ring_size=250,
         title="Brighten only above Freq.",
         text="Around 2 kHz adds presence, and 6 to 10 kHz adds air. The new harmonics are made "
              "from the part of the signal above it.",
         next="Then decide how dense they are."),
    dict(kind="feature", heading="Clip Boost", layout="above",
         image="exciter", crop=(20, 200, 520, 770), scale=1.15, ring=(368, 348), ring_size=180,
         title="Make them denser.",
         text="Clip Boost drives the clipper harder for denser harmonics. Harmonics sets how "
              "much of them to add.",
         next="And how much of it you hear."),
    dict(kind="feature", heading="Mix Back", layout="below",
         image="exciter", crop=(20, 480, 520, 770), scale=1.6, ring=(368, 628), ring_size=260,
         title="Blend in parallel.",
         text="Mix Back sets how much of the effect you hear, under the untouched signal.",
         next="Exciter works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="sature"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Exciter: adds brightness above a frequency you choose.
It makes new harmonics instead of boosting the highs that are already there.
Clip Boost makes them denser, and Mix Back blends them in.
Free to download and use: {link}."""
