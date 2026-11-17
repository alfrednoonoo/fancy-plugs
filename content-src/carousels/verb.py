"""Fancy Verb's carousel. Facts from docs/verb-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Convolution reverb",
         sentence="Real spaces from 32 built-in impulse responses, or your own files.",
         window=dict(image="verb-library", y=540)),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Put a sound in a real space.",
         text="It plays the sound through a recording of a room, a hall or a plate.",
         next="The spaces come with it."),
    dict(kind="feature", heading="Library", layout="below",
         image="verb-library", crop=(0, 546, 596, 1250), scale=0.92, ring=(516, 786),
         title="32 impulse responses built in.",
         text="Rooms, Halls, Plates, Chambers and Special. Or load any WAV of your own.",
         next="Many of them are more than plain stereo."),
    dict(kind="feature", heading="True stereo", layout="above",
         image="verb-library", crop=(0, 146, 1468, 640), scale=0.627, ring=(189, 598), ring_size=84,
         title="True stereo.",
         text="Library impulses marked TS, four-channel WAVs and file pairs keep each input "
              "side’s own reflections.",
         next="An impulse is only the start."),
    dict(kind="feature", heading="Modules", layout="below",
         image="verb-generators", crop=(0, 546, 1468, 1394), scale=0.627, ring=(1425, 870), ring_size=104,
         title="Build a reverb as a chain of modules.",
         text="Start from a file or a generator, filter it, add echoes and normalize it. "
              "Each module is a card you can switch off, reorder or remove.",
         next="One of them keeps a long tail alive."),
    dict(kind="feature", heading="Modulate", layout="above",
         image="verb-generators", crop=(720, 546, 1214, 1120), scale=1.15, ring=(905, 862), ring_size=160,
         title="Move a long tail.",
         text="Modulate moves the tail gently, without pitch wobble.",
         next="Verb works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="room"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Verb: real spaces from 32 built-in impulse responses, or your own files.
True stereo, and a chain of modules you can switch off, reorder or remove.
Modulate moves a long tail without pitch wobble.
Free to download and use: {link}."""
