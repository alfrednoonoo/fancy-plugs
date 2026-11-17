"""Fancy Gate's carousel. Facts from docs/gate-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="Gate, ducker and trigger",
         sentence="Closes between hits, ducks from a sidechain and sends MIDI.",
         window=dict(image="gate")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Silence a drum mic between hits.",
         text="It opens on the hit and closes after it, with a filter on what it listens to.",
         next="You can watch it do that."),
    dict(kind="feature", heading="History", layout="below",
         image="gate", crop=(10, 222, 1160, 580), scale=0.8, ring=(390, 262),
         title="Watch the gate open and close.",
         text="The history shows the level and where the gate opens and closes, so you set "
              "Threshold, Hold and Release by eye before you fine-tune by ear.",
         next="It can also work the other way round."),
    dict(kind="feature", heading="Duck", layout="above",
         image="gate", crop=(410, 160, 1160, 372), scale=1.22, ring=(1103, 188),
         title="Duck from a sidechain.",
         text="Switch Mode to Duck, and the track turns down while the sidechain plays, "
              "such as music under a voice.",
         next="What it listens to is up to you."),
    dict(kind="feature", heading="Detector filters", layout="below",
         image="gate", crop=(240, 590, 1160, 790), scale=1.0, ring=(820, 690),
         title="Open only on what matters.",
         text="Lowpass and Highpass filter what the gate listens to, so only the snare opens "
              "a snare mic. Preview filter lets you set them by ear.",
         next="And it never has to clip a transient."),
    dict(kind="feature", heading="Lookahead", layout="above",
         image="gate-edit", crop=(240, 382, 1160, 578), scale=1.0, ring=(332, 482),
         title="Keep every attack.",
         text="With Lookahead on, the gate opens 50 ms early, so not even the first transient "
              "is cut. Pre-open sets how early.",
         next="Gate works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="limit"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Gate: closes between hits, ducks from a sidechain and sends MIDI.
Watch it open and close on the history, and filter what it listens to.
Open 50 ms early with Lookahead, and trigger a sample with a MIDI note on every opening.
Free to download and use: {link}."""
