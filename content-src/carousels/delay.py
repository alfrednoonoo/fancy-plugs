"""Fancy Delay's carousel. Facts from docs/delay-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Multi-tap delay",
         sentence="Up to 16 taps, in time with the song.",
         window=dict(image="delay")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Add echoes in time with the song.",
         text="Up to 16 taps, each with its own time, feedback and colour.",
         next="You see them all at once."),
    dict(kind="feature", heading="Tap map", layout="below",
         image="delay", crop=(26, 166, 1246, 466), scale=0.754, ring=(298, 236),
         title="See every tap and its repeats on one line.",
         text="The tap map shows each tap’s time and level, with its feedback repeats faded "
              "behind it, so a pattern of echoes reads at a glance.",
         next="Each tap keeps time by itself."),
    dict(kind="feature", heading="Time", layout="above",
         image="delay", crop=(26, 542, 400, 832), scale=1.6, ring=(331, 587), ring_size=160,
         title="In time with the song.",
         text="Note values follow the host’s tempo, with dotted and triplet values. "
              "Or set a tap in milliseconds.",
         next="And each tap has its own colour."),
    dict(kind="feature", heading="Motion", layout="below",
         image="delay-motion", crop=(414, 542, 1292, 832), scale=1.045, ring=(562, 620), ring_size=104,
         title="Tape or BBD, with motion.",
         text="Each tap can be Clean, Tape or BBD. Rate, Depth and Random add wow and flutter "
              "to its repeats.",
         next="The echoes can also stay out of the way."),
    dict(kind="feature", heading="Ducking", layout="above",
         image="delay-edit", crop=(480, 556, 1110, 800), scale=1.4, ring=(752, 690), ring_size=180,
         title="Duck under the voice.",
         text="Ducking turns the echoes down while the dry signal plays, and lets them come "
              "back in the gaps.",
         next="Delay works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="verb"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Delay: up to 16 taps, in time with the song.
See every tap and its repeats on the tap map, and set each one as a note value.
Tape or BBD character per tap, with motion, ducking and swing.
Free to download and use: {link}."""
