"""The suite's carousel: it introduces all fourteen and hands to the series, which starts with
Fancy Comp (docs/content-plan.md, section 7, "The intro carousel").

Every fact is from site-src/data/plugins.json, the site's home page and the manual's shared
chapters. Teal is the suite's colour.
"""

SLUG = "suite"

# Not a plugin: the strip and the cover read "Fancy Plugs", in the suite's teal.
PLUGIN = dict(name="Plugs", accent="#00A985")

SLIDES = [
    dict(kind="cover", heading="Fourteen free effects",
         sentence="Free effects for the whole mix: dynamics, tone and space.",
         window=dict(image="comp")),
    dict(kind="job", heading="The idea",
         label="One set of plugins",
         title="Fourteen effects that all work the same way.",
         text="Every one opens with the same strip and header. Learn one window and you know them all.",
         next="They come in three groups."),
    dict(kind="group", heading="Dynamics", group="dynamics", layout="grid",
         title="Dynamics",
         text="Plugins that react to level: they turn things down, or up, when the sound gets louder or quieter.",
         next="Then the ones that change the tone."),
    dict(kind="group", heading="Tone", group="tone", layout="grid",
         title="Tone",
         text="Plugins that change the colour of a sound: which frequencies are loud, and what the sound is made of.",
         next="And the ones that add space."),
    dict(kind="group", heading="Space and time", group="space", layout="rows",
         title="Space and time",
         text="Plugins that add echoes and rooms around a sound.",
         next="Next: a carousel for each, from Fancy Comp."),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Plugs: fourteen free effects for the whole mix, for dynamics, tone and space.
Compressors, a gate, a limiter, de-essers, an EQ, a spectral processor, an exciter, saturation, a kick enhancer, a delay and two reverbs.
They all work the same way: learn one window and you know them all. Windows, and Mac as a beta.
A carousel for each plugin follows, starting with Fancy Comp.
Free to download and use: {link}."""
