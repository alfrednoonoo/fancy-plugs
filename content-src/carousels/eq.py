"""Fancy EQ's carousel. Facts from docs/eq-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Parametric equalizer",
         sentence="Up to 24 bands on a live analyzer, with dynamic bands, mid/side and Match.",
         window=dict(image="eq", y=540)),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Shape the frequencies of a track.",
         text="Up to 24 bands on a live analyzer, any of which can act only when needed.",
         next="You work on the graph."),
    dict(kind="feature", heading="Bands", layout="below",
         image="eq", crop=(0, 146, 1468, 880), scale=0.627, ring=(810, 330),
         title="Drag the curve itself.",
         text="Drag a handle: left and right is frequency, up and down is gain. Scroll on it "
              "for Q. The analyzer shows what’s playing behind the curve.",
         next="A band doesn’t have to be on all the time."),
    dict(kind="feature", heading="Dynamic", layout="above",
         image="eq-dynamic", crop=(0, 440, 1468, 1160), scale=0.627, ring=(182, 1070),
         title="Cut a band only while it’s too loud.",
         text="A dynamic band acts only while the level crosses its threshold. The dashed "
              "outline shows the most it can cut, and the filled curve what it’s cutting now.",
         next="Or on both channels."),
    dict(kind="feature", heading="Channel", layout="below",
         image="eq-channel", crop=(0, 146, 1468, 880), scale=0.627, ring=(977, 588),
         title="Put a band on Mid or Side.",
         text="Any band can work on Mid, Side, Left or Right. Its letter shows on its handle.",
         next="And the EQ can find the curve for you."),
    dict(kind="feature", heading="Match", layout="above",
         image="eq-match", crop=(188, 484, 1282, 784), scale=0.84, ring=(1174, 638),
         title="Match a reference.",
         text="Learn a reference and your material, and Match writes ordinary bands that move "
              "one towards the other.",
         next="EQ works well with three others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="fir"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy EQ: up to 24 bands on a live analyzer.
Drag the curve, make a band dynamic, and put a band on Mid or Side.
Match learns a reference and writes the bands for you.
Free to download and use: {link}."""
