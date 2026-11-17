"""Fancy Limit's carousel. Facts from docs/limit-manual.md and site-src/data/plugins.json."""

SLIDES = [
    dict(kind="cover", heading="True-peak limiter",
         sentence="Holds a master under a ceiling, with a loudness meter beside it.",
         window=dict(image="limit")),
    dict(kind="job", heading="The job",
         label="Use it to",
         title="Stop peaks at the end of a master.",
         text="A brickwall ceiling measured in true peak, with a loudness meter beside it.",
         next="It starts with the ceiling."),
    dict(kind="feature", heading="Ceiling", layout="below",
         image="limit", crop=(238, 570, 1260, 824), scale=0.9, ring=(366, 727),
         title="Hold a true-peak ceiling.",
         text="Ceiling is the highest level the output can reach. With True peak on, peaks "
              "between samples are caught too, so the ceiling holds after encoding.",
         next="Then push the level up to it."),
    dict(kind="feature", heading="Loudness", layout="above",
         image="limit", crop=(238, 160, 1260, 554), scale=0.9, ring=(608, 262),
         title="Reach a loudness target without guessing.",
         text="Pick a target, such as Streaming at -14 LUFS. The meter shows the loudness as "
              "you pull Threshold down, and the red marks show what each peak costs.",
         next="How it catches those peaks is a choice."),
    dict(kind="feature", heading="Style", layout="below",
         image="limit", crop=(10, 160, 930, 420), scale=1.0, ring=(165, 192),
         title="Choose a style.",
         text="Clean, Punchy or Dense: how the limiter shapes the peaks it catches.",
         next="And how it lets go can follow the music."),
    dict(kind="feature", heading="Auto release", layout="above",
         image="limit", crop=(10, 640, 790, 926), scale=1.18, ring=(264, 890),
         title="Let the release follow the music.",
         text="Auto release speeds up for short peaks and slows down when the limiting is "
              "sustained, which avoids pumping. Listen to removed plays only what the limiter "
              "takes away.",
         next="Limit works well with two others."),
    dict(kind="pairs", heading="Works well with", title="Works well with", next_plugin="ds"),
    dict(kind="download", heading="Download"),
]

CAPTION = """Fancy Limit: holds a master under a true-peak ceiling, with a loudness meter beside it.
Pick a loudness target and a style: Clean, Punchy or Dense.
Let Auto release follow the music, and hear only what the limiter removes.
Free to download and use: {link}."""
