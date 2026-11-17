"""Fancy DS-Mini's carousel. Facts from docs/ds-mini-manual.md and site-src/data/plugins.json.
The Amount knob and the Save button fill their crops, so those slides carry no ring."""

SLIDES = [
    dict(kind="cover", heading="One-knob de-esser",
         sentence="The same engine as Fancy DS, behind one knob.",
         window=dict(image="dsmini")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Soften “s” sounds, quickly.",
         text="One Amount knob on the same engine.",
         next="First, say what you’re de-essing."),
    dict(kind="feature", heading="Profiles", layout="above",
         image="dsmini", crop=(20, 160, 540, 480), scale=1.5, ring=(493, 200),
         title="Six profiles.",
         text="Female voice, Male voice, Sibilance, Harshness, Acoustic guitar and Bowed "
              "strings. Each listens to a different band at a different speed.",
         next="Then there is one thing to turn."),
    dict(kind="feature", heading="Amount", layout="below",
         image="dsmini", crop=(20, 490, 540, 880), scale=1.3,
         title="One knob.",
         text="Turn Amount until the “s” sounds are soft but clear.",
         next="And if one knob isn’t enough, take it further."),
    dict(kind="feature", heading="Fancy DS", layout="below",
         image="dsmini-edit", crop=(20, 160, 540, 660), scale=1.2,
         title="Carry it over to Fancy DS.",
         text="Any setting here sounds exactly like Fancy DS set the same way. Save as Fancy "
              "DS preset opens it there, with every control.",
         next="DS-Mini works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="eq"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy DS-Mini: the same engine as Fancy DS, behind one knob.
Pick one of six profiles, then turn Amount until the "s" sounds are soft but clear.
Save the setting as a Fancy DS preset when you want every control.
Free to download and use: {link}."""
