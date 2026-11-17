"""Fancy FIR's carousel. Facts from docs/fir-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Spectral processor",
         sentence="Removes steady noise and ringing, or draws an EQ freehand.",
         window=dict(image="fir-resonance")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Remove steady noise or ringing.",
         text="It works on the spectrum itself, so it can take out what an EQ can’t.",
         next="Start with what rings."),
    dict(kind="feature", heading="Resonance", layout="below",
         image="fir-resonance", crop=(26, 232, 1246, 624), scale=0.754, ring=(916, 328),
         title="Turn down ringing only while it rings.",
         text="Resonance finds narrow peaks that stand above the spectrum around them, such as "
              "a ringing snare or a boxy vocal, and turns each one down only while it’s there.",
         next="Noise that never stops needs another mode."),
    dict(kind="feature", heading="Subtract", layout="above",
         image="fir-adaptive", crop=(26, 400, 1298, 866), scale=0.723, ring=(204, 670),
         title="Remove steady noise.",
         text="Learn the noise from a part with only noise in it, or let Adaptive track it as "
              "it changes.",
         next="The same spectrum can be an EQ."),
    dict(kind="feature", heading="EQ", layout="below",
         image="fir-eq", crop=(26, 232, 1246, 624), scale=0.754, ring=(508, 332),
         title="Draw an EQ freehand.",
         text="The curve is the EQ. Click it to add a point and drag points to shape it, or "
              "draw it with the mouse.",
         next="Two more modes work on every frequency."),
    dict(kind="feature", heading="Modes", layout="above",
         image="fir-resonance", crop=(26, 160, 906, 420), scale=1.045, ring=(213, 192),
         title="Gate or compress each frequency.",
         text="Gate and Compressor act on every part of the spectrum on its own.",
         next="FIR works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="exciter"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy FIR: removes steady noise and ringing, or draws an EQ freehand.
Resonance turns down a ringing peak only while it rings, and Subtract learns or tracks noise.
Gate and Compressor modes work on every frequency on its own.
Free to download and use: {link}."""
