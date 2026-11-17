"""Fancy Room's carousel, the last of the series. Facts from docs/room-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Algorithmic reverb",
         sentence="From a small room to a long hall, with Freeze.",
         window=dict(image="room")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Add a room or hall you can shape.",
         text="Size, decay and damping move freely, and the tail can freeze.",
         next="You see the shape before you hear it."),
    dict(kind="feature", heading="Display", layout="below",
         image="room", crop=(26, 166, 1246, 562), scale=0.754, ring=(420, 359),
         title="See the decay you’re setting.",
         text="The display draws the early reflections, the tail and how fast the highs fade, "
              "so Decay and Damping read as a shape.",
         next="And the length is a plain number."),
    dict(kind="feature", heading="Decay", layout="above",
         image="room", crop=(26, 578, 856, 832), scale=1.1, ring=(370, 735), ring_size=160,
         title="Length in seconds.",
         text="Decay sets how long the tail lasts, in seconds, whatever the Size.",
         next="The tail can also stop fading."),
    dict(kind="feature", heading="Freeze", layout="below",
         image="room", crop=(26, 640, 856, 926), scale=1.1, ring=(282, 890), ring_size=84,
         title="Freeze the tail.",
         text="Freeze holds the tail for as long as you like. Wet only (send) takes out the dry "
              "signal, and Sync pre-delay locks the pre-delay to the tempo.",
         next="Or it can step back while you play."),
    dict(kind="feature", heading="Ducking", layout="above",
         image="room-edit", crop=(30, 430, 560, 650), scale=1.6, ring=(138, 535), ring_size=200,
         title="Duck under the dry signal.",
         text="Ducking turns the reverb down while the dry signal plays, so it fills the gaps "
              "and not the words.",
         next="Room works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin=None),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Room: an algorithmic reverb, from a small room to a long hall.
See the decay on the display, and set its length in seconds.
Freeze the tail, use it as a send, and duck it under the dry signal.
Free to download and use: {link}."""
