"""Fancy Sature's carousel. Facts from docs/sature-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Saturator",
         sentence="Tape, Tube, Transistor and Clip colour at a steady level.",
         window=dict(image="sature")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Add warmth or grit.",
         text="Four kinds of saturation, at a steady level.",
         next="Pick the kind first."),
    dict(kind="feature", heading="Type", layout="below",
         image="sature", crop=(26, 160, 1246, 554), scale=0.754, ring=(265, 192),
         title="Four characters.",
         text="Tape, Tube, Transistor and Clip. The display shows the harmonics the current "
              "settings add to a 1 kHz tone.",
         next="Then lean it the way you want."),
    dict(kind="feature", heading="Character", layout="above",
         image="sature", crop=(26, 570, 760, 824), scale=1.25, ring=(392, 727), ring_size=170,
         title="Even or odd.",
         text="Character leans the harmonics towards even or odd, and Tone tilts the saturated "
              "sound darker or brighter.",
         next="Driving harder doesn’t have to mean louder."),
    dict(kind="feature", heading="Auto gain", layout="below",
         image="sature", crop=(10, 640, 790, 926), scale=1.18, ring=(40, 890), ring_size=84,
         title="Stay at the same level.",
         text="Auto gain keeps the level steady as you drive harder, so you hear the colour "
              "and not just more volume.",
         next="And not everything has to be saturated."),
    dict(kind="feature", heading="Band", layout="above",
         image="sature", crop=(776, 570, 1292, 824), scale=1.6, ring=(903, 727), ring_size=200,
         title="Keep lows or highs clean.",
         text="Saturate only between Low and High, and leave the rest untouched.",
         next="Sature works well with three others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="kicker"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Sature: Tape, Tube, Transistor and Clip colour at a steady level.
Lean the harmonics even or odd, and see what you're adding on the display.
Auto gain keeps the level steady, and Band keeps the lows or highs clean.
Free to download and use: {link}."""
