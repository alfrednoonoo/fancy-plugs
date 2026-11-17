"""Fancy MultiComp's carousel. Facts from docs/multicomp-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Multiband compressor",
         sentence="Up to 8 bands of compression that sum back flat.",
         window=dict(image="multicomp")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Control only the lows or highs of a bus.",
         text="Each band has its own compressor, and untouched bands pass through flat.",
         next="It starts on the graph."),
    dict(kind="feature", heading="Bands", layout="below",
         image="multicomp", crop=(26, 166, 1246, 548), scale=0.75, ring=(765, 495),
         title="Drag the bands where you need them.",
         text="Drag a blue line to move a crossover, and a band’s handle to set its threshold. "
              "Add bands with +, up to 8.",
         next="Each band can also work the other way."),
    dict(kind="feature", heading="Upward", layout="above",
         image="multicomp", crop=(26, 914, 926, 1138), scale=1.0, ring=(746, 1027),
         title="Lift the quiet parts of one band.",
         text="Each band has its own upward compression, with a limit on the lift and a floor "
              "under it, so noise stays where it is.",
         next="A band doesn’t have to be stereo."),
    dict(kind="feature", heading="Channel", layout="below",
         image="multicomp-channel", crop=(26, 166, 1246, 612), scale=0.75, ring=(1028, 586),
         title="Put a band on Mid or Side.",
         text="A band can work on Stereo, Left, Right, Mid or Side. Its letter shows on the "
              "graph: M for Mid.",
         next="It can also see a transient coming."),
    dict(kind="feature", heading="Lookahead", layout="above",
         image="multicomp-edit", crop=(560, 640, 1246, 930), scale=1.3, ring=(902, 767),
         title="Start turning down before the hit.",
         text="Up to 10 ms of lookahead, so the bands react before a transient arrives. "
              "The latency it adds is reported to the host.",
         next="MultiComp works well with three others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="gate"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy MultiComp: up to 8 bands of compression that sum back flat.
Drag the crossovers and thresholds on the graph, and lift a band's quiet parts with upward compression.
Put a band on Mid or Side, and look ahead up to 10 ms.
Free to download and use: {link}."""
