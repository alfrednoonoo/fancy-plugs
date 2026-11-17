"""Fancy DS's carousel. Facts from docs/ds-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="De-esser",
         sentence="Turns down “s” sounds only while they’re too loud.",
         window=dict(image="ds")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Soften “s” sounds, with full control.",
         text="Threshold, frequency, range and two ways of cutting.",
         next="You can see every cut."),
    dict(kind="feature", heading="History", layout="below",
         image="ds", crop=(238, 234, 1300, 554), scale=0.866, ring=(430, 262),
         title="Cut each “s” only while it’s too loud.",
         text="The history shows the sibilance in grey and each cut in green, so you can see "
              "that the vowels are left alone.",
         next="Where it starts follows the singer."),
    dict(kind="feature", heading="Threshold", layout="side",
         image="ds", crop=(10, 234, 230, 830), scale=1.3, ring=(57, 503),
         title="Follow the voice.",
         text="A relative threshold tracks the voice’s level, so loud and soft phrases are "
              "treated the same.",
         next="Then choose how it cuts."),
    dict(kind="feature", heading="Mode", layout="above",
         image="ds", crop=(600, 160, 1310, 420), scale=1.29, ring=(1171, 192),
         title="Split or Wide.",
         text="Split turns down only the harsh band. Wide turns the whole signal down, for a "
              "gentler result.",
         next="And check what it takes out."),
    dict(kind="feature", heading="Listen", layout="below",
         image="ds", crop=(10, 640, 790, 926), scale=1.18, ring=(363, 890),
         title="Hear what’s removed.",
         text="Listen plays only the sibilance being taken out. It turns off when you close "
              "the window.",
         next="DS works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="ds-mini"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy DS: turns down "s" sounds only while they're too loud.
A relative threshold follows the voice, and Split or Wide sets how it cuts.
See every cut on the history, and hear only what's removed.
Free to download and use: {link}."""
