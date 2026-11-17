"""Fancy Kicker's carousel. Facts from docs/kicker-manual.md and site-src/data/plugins.json.
The large Freq knob fills its crop, so that slide carries no ring."""

SLIDES = [
    dict(kind="cover", heading="Low-end enhancer",
         sentence="Swells a kick’s own note, or adds a sub below it.",
         window=dict(image="kicker")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Add low end to a kick.",
         text="It swells the kick’s own note, or adds a sub an octave or two below it.",
         next="Boost comes first."),
    dict(kind="feature", heading="Boost", layout="above",
         image="kicker", crop=(20, 160, 520, 570), scale=1.4,
         title="Find the kick’s note, and make it bigger.",
         text="Sweep the one large Freq knob until the low end swells: that’s where the drum’s "
              "own note is.",
         next="Or put something new under it."),
    dict(kind="feature", heading="Sub", layout="below",
         image="kicker-sub", crop=(20, 160, 520, 615), scale=1.3, ring=(311, 204), ring_size=150,
         title="Add a clean sub under it.",
         text="Sub follows the kick’s pitch and adds a sub one or two octaves below it, in tune.",
         next="Then set how much."),
    dict(kind="feature", heading="Blend", layout="above",
         image="kicker-sub", crop=(20, 615, 520, 870), scale=1.6, ring=(158, 740), ring_size=260,
         title="Blend it with the original.",
         text="Sub sets how much to add, and Dry the level of the original. There is no latency "
              "at any setting, so it lines up with everything else.",
         next="Kicker works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="delay"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Kicker: swells a kick's own note, or adds a sub below it.
Boost finds the drum's note with one large knob, and Sub follows its pitch an octave or two down.
No latency at any setting.
Free to download and use: {link}."""
