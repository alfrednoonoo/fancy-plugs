# Hand-written content for the Fancy Plugs manual: plugin metadata, quick starts, the shared
# chapters, and descriptions for controls the per-plugin manuals don't table.

VERSION = "1.7"

PARTS = [
    ("Dynamics", "Plugins that react to level: they turn things down, or up, when the sound gets louder or quieter.",
     ["Comp", "MultiComp", "Gate", "Limit", "DS", "DS-Mini", "TS"]),
    ("Tone", "Plugins that change the colour of a sound: which frequencies are loud, and what the sound is made of.",
     ["EQ", "FIR", "Exciter", "Sature", "Kicker"]),
    ("Space and time", "Plugins that add echoes, rooms and movement around a sound.",
     ["Delay", "Verb", "Room", "Chorus"]),
]

# Accent: the plugin's own accent (docs/DESIGN_SYSTEM.md section 2). Plugins whose accent follows a
# selection (EQ and MultiComp bands, FIR modes) show their whole sequence.
PLUGINS = {
    "Kicker": dict(file="kicker", accent="#E74D3D", kind="Low-end enhancer",
                   images=[("kicker", "Boost mode"), ("kicker-sub", "Sub mode")], narrow=True,
                   quick=["Put Fancy Kicker on a kick drum or bass track.",
                          "Sweep **Freq** between 40 and 80 Hz until the low end swells: that's where the drum's own note is.",
                          "Raise **Wet** from -12 dB until the low end feels full, then lower it a little. For a deeper sub-octave instead, switch the **Boost | Sub** switch to Sub."]),
    "Exciter": dict(file="exciter", accent="#E74D3D", kind="Harmonic exciter",
                    images=[("exciter", "Fancy Exciter")], narrow=True,
                    quick=["Put Fancy Exciter on a dull vocal, guitar or bus.",
                           "Raise **Harmonics** from 0 %: nothing changes until you do. Set **Freq** to where the brightness should start, around 2 kHz for presence and 6 to 10 kHz for air.",
                           "Add **Clip Boost** for denser harmonics, and use **Mix Back** to set how much of the effect you hear."]),
    "Comp": dict(file="comp", accent="#00A985", kind="Compressor",
                 images=[("comp", "Fancy Comp"), ("comp-upward", "The EDIT drawer's Upward page")],
                 quick=["Put Fancy Comp on a vocal, bass or drum track and play the loudest part.",
                        "Pull the **Threshold** fader down until the **GR** meter shows 3 to 6 dB of gain reduction on the peaks. **Ratio** sets how firmly: 2:1 is gentle, 8:1 is strong.",
                        "Set **Attack** (longer lets the punch through) and **Release** (or tick **Auto**), then tick **Auto make-up**, or raise the header's output knob, to win back the level."]),
    "EQ": dict(file="eq", accent="#E74D3D", palette=["#E74D3D", "#9A59B5", "#3598DB", "#00A985", "#2DCC70", "#F1C315", "#E77E23"],
               kind="Parametric equalizer", images=[("eq-dynamic", "Band 2 as a dynamic cut"), ("eq-match", "The Match card")],
               quick=["Put Fancy EQ on any track. Four bands are ready on the graph as coloured handles.",
                      "Drag a handle: left and right is frequency, up and down is gain. Scroll on it, or use the **Q** knob, to make it narrower or wider.",
                      "Click **+** in the band tabs for more bands, and pick each band's shape from the **Type** dropdown. Cut narrow, boost wide."]),
    "Verb": dict(file="verb", accent="#9A59B5", kind="Convolution reverb",
                 images=[("verb", "Fancy Verb"), ("verb-library", "A true-stereo hall from the library")],
                 quick=["Put Fancy Verb on a send (an FX or aux track), or on the track itself.",
                        "A new instance starts with an empty chain. Click **File**, one of the module buttons in the middle of the window, then open the new module's dropdown and pick an IR from the **Library**: Rooms, Halls, Plates, Chambers or Special. Or pick a factory preset from the preset pill.",
                        "Balance **Wet** against **Dry** (on a send, pull Dry all the way down), and set **Pre-delay** to keep the voice clear of the reverb."]),
    "Delay": dict(file="delay", accent="#F1C315", kind="Multi-tap delay",
                  images=[("delay", "Fancy Delay"), ("delay-motion", "The Motion page")],
                  quick=["Put Fancy Delay on a vocal, guitar or synth, or on a send.",
                         "Tap 1 is already on at a dotted 1/8 note, in time with your project. Pick another note value from the dropdown, or switch to **ms** for a free time.",
                         "Raise **Feedback** for more repeats, darken them with **Lowpass**, and set the balance with **Wet** and **Dry**. Click **+** for another tap."]),
    "MultiComp": dict(file="multicomp", accent="#E74D3D", palette=["#E74D3D", "#9A59B5", "#24AEC6", "#2DCC70", "#E77E23", "#F1C315", "#1BBC9C", "#3598DB"],
                      kind="Multiband compressor", images=[("multicomp", "Upward compression on a band"), ("multicomp-channel", "Band 2 on the Mid channel")],
                      quick=["Put Fancy MultiComp on a bus or a full mix. It starts with four bands that sum back flat.",
                             "Drag the blue lines on the graph to move the crossovers between bands.",
                             "Drag a band's handle down to lower its **Threshold** until it starts to compress, use the mouse wheel on the handle for **Ratio**, and set **Attack** and **Release** below. Repeat for each band that needs it."]),
    "FIR": dict(file="fir", accent="#9A59B5", palette=["#9A59B5", "#F1C315", "#00A985", "#E74D3D", "#3598DB", "#2DCC70"],
                kind="Spectral processor", images=[("fir-adaptive", "Subtract mode, Adaptive"), ("fir-resonance", "Resonance mode")],
                quick=["Put Fancy FIR on the track and choose what it should do with the **Mode** switch: EQ, Gate, Compressor, Subtract, Convolve L/R or Resonance.",
                       "To clean up noise: choose **Subtract**, play a part with only the noise, click **Learn**, then raise **Reduction** until the noise goes.",
                       "To tame ringing: choose **Resonance** and raise **Depth**. Tick **Listen** to hear only what's being removed, then untick it."]),
    "Gate": dict(file="gate", accent="#F1C315", kind="Gate, ducker and trigger",
                 images=[("gate", "Fancy Gate")],
                 quick=["Put Fancy Gate on a drum mic or a noisy track.",
                        "Play it and raise **Threshold** until the gate opens on every hit and stays closed between them.",
                        "Set **Release** so the tail sounds natural, and raise **Range** from -inf if a closed gate sounds too abrupt."]),
    "Limit": dict(file="limit", accent="#E74D3D", kind="True-peak limiter",
                  images=[("limit", "Fancy Limit")],
                  quick=["Put Fancy Limit last on your master bus.",
                         "Set **Ceiling** to -1 dB, so peaks stay under -1 dBTP after encoding.",
                         "Pull **Threshold** down until the loudness meter reaches your **Target** (Streaming, -14 LUFS, by default). The history display shows the gain reduction in red: a few dB on the peaks is usually transparent."]),
    "DS": dict(file="ds", accent="#2DCC70", kind="De-esser",
               images=[("ds", "Fancy DS")],
               quick=["Put Fancy DS on a vocal, after any EQ or compressor that brightens it.",
                      "Play a phrase with strong \"s\" sounds and lower **Threshold** until the GR meter shows a few dB on each \"s\" and nothing on the vowels.",
                      "Move **Freq** along the ruler to where the \"s\" is harshest, and set **Range** to limit how far it can cut."]),
    "DS-Mini": dict(file="ds-mini", accent="#3598DB", kind="One-knob de-esser",
                    images=[("dsmini", "Fancy DS-Mini")], narrow=True,
                    quick=["Put Fancy DS-Mini on a vocal.",
                           "Pick the **Profile** that matches the voice: Female voice, Male voice, and so on.",
                           "Turn **Amount** up until the \"s\" sounds are soft but still clear."]),
    "Sature": dict(file="sature", accent="#E77E23", kind="Saturator",
                   images=[("sature", "Fancy Sature")],
                   quick=["Put Fancy Sature on a track that needs warmth or grit.",
                          "Choose a **Type**: Tape, Tube, Transistor or Clip.",
                          "Turn **Drive** up. Auto gain keeps the level steady, so you hear the colour, not just more volume. Use **Dry** for parallel saturation."]),
    "Room": dict(file="room", accent="#9A59B5", kind="Algorithmic reverb",
                 images=[("room", "Fancy Room")],
                 quick=["Put Fancy Room on a send, or on the track itself.",
                        "Set **Decay** for the length of the tail in seconds, and **Size** for how big the space sounds.",
                        "Balance **Wet** against **Dry**. On a send, tick **Wet only (send)**. Use **Damping** and **High cut** to make it darker."]),
    "TS": dict(file="ts", accent="#E77E23", kind="Transient shaper",
               images=[("ts", "Fancy TS")],
               quick=["Put Fancy TS on a drum track, a loop or a plucked instrument.",
                      "Turn **Attack** up for more snap at the start of each hit, or down to soften it. **Speed** sets how long the start counts.",
                      "Turn **Sustain** down to shorten the ring after each hit (a room, a boomy kick), or up to bring it out; **Length** sets how long it works. Tick **Clip output** if raised attacks get too loud."]),
    "Chorus": dict(file="chorus", accent="#E0559A", kind="Chorus",
                   images=[("chorus", "Fancy Chorus")],
                   quick=["Put Fancy Chorus on a guitar, pad, synth or vocal.",
                          "Pick a **Mode**: Ensemble (the default) for shimmer, Classic for the familiar two-voice chorus, Dimension for steady width, Doubler for a second take, Flange for jet sweeps.",
                          "Set **Rate** and **Depth** for how fast and how far the voices move, **Voices** and **Width** for how many and how wide, and balance **Wet** against **Dry**."]),
}

# Screens: from the release build's screens-<sha> artifact, @2x. MORE goes under "More views".
# Each view: (screen, caption, heading). The screenshot goes right under that heading of the
# plugin's manual; a view without a heading goes under "More views".
MORE = {
    "Kicker": [("kicker-edit", "Boost mode's EDIT drawer", "EDIT drawer"), ("kicker-sub-edit", "Sub mode's EDIT drawer", "Sub mode")],
    "Exciter": [("exciter-edit", "The EDIT drawer", "EDIT drawer")],
    "Comp": [("comp-history", "EDIT drawer: History page", "History"), ("comp-sync", "Host sync as the detector input, with its Rate and Shape", "Host sync (Phase 4)"),
             ("comp-options", "EDIT drawer: Options page", "Options"), ("comp-detector", "EDIT drawer: Detector page (detector EQ)", "Detector"),
             ("comp-character", "EDIT drawer: Character page", "Character"),
             ("comp-listening", "Preview filter on: the title strip says LISTENING", "Checkboxes")],
    "EQ": [("eq", "Default bands", "The graph"), ("eq-channel", "A band on one channel", "Channel"),
           ("eq-compact", "Compact mode", "Checkboxes"), ("eq-edit", "The EDIT drawer", "EDIT drawer"),
           ("eq-dynamic-compact", "A dynamic band's Reach, and Freeze on the graph", "Phase 4 additions"),
           ("eq-brickwall", "A Brickwall high-pass on band 1, and Character set to Warm in the EDIT drawer", "Phase 4 additions")],
    "Verb": [("verb-empty", "A new instance: an empty chain", "Getting started"), ("verb-add", "Add module", "The rack"),
             ("verb-generators", "The generator modules", "Modules"), ("verb-eq", "The EQ module", "Modules"),
             ("verb-picker", "Searching the library from the File module's dropdown", "Modules"),
             ("verb-drop", "Dropping an audio file on the rack", "Modules"), ("verb-edit", "SETUP", "SETUP")],
    "Delay": [("delay-edit", "The EDIT drawer", "EDIT")],
    "MultiComp": [("multicomp-edit", "The EDIT drawer: Set all bands, band 2's detector EQ and the crossover slopes", "EDIT"),
                  ("multicomp-history", "GR history: each band's gain reduction over the last 3 s", "GR history")],
    "FIR": [("fir", "Subtract mode while Learned is learning a noise profile", "Learned, Adaptive or Both"), ("fir-eq", "EQ mode", "Modes"), ("fir-resonance-range", "Resonance with Range set, listening", "Resonance"),
            ("fir-edit", "The EDIT drawer, with Mid/Side on", "EDIT"),
            ("fir-compressor", "Compressor mode: the threshold curve, with the ratio curve dimmed", "Ratio curve"),
            ("fir-ratio", "Editing the side's ratio curve in Mid/Side", "Mid/Side")],
    "Gate": [("gate-edit", "Lookahead, a sidechain detector and MIDI out", "Controls"), ("gate-drawer", "The EDIT drawer, with Host sync", "Controls"), ("gate-listening", "Listening to the gain-reduced part", "Controls"),
             ("gate-transient", "Transient chosen: Rise in RMS size's place", "Transient detector"), ("gate-upward", "Upward mode: Boost in Range's place", "Upward mode"),
             ("gate-drawer-upward", "The EDIT drawer in Upward, with Up ratio and Ceiling", "Upward mode")],
    "Limit": [("limit-edit", "The EDIT drawer", "EDIT drawer")],
    "DS": [("ds-edit", "The EDIT drawer", "EDIT drawer"), ("ds-listening", "Listening to what's removed", "Controls")],
    "DS-Mini": [("dsmini-edit", "The EDIT drawer", "EDIT drawer"), ("dsmini-listening", "Listening to what's removed", "Controls")],
    "Sature": [("sature-edit", "The EDIT drawer", "EDIT"), ("sature-listening", "Listening to the added harmonics", "Controls")],
    "Room": [("room-edit", "The EDIT drawer", "EDIT")],
    "TS": [("ts-edit", "The EDIT drawer", "EDIT drawer"), ("ts-listening", "Listen to change on", "Listen to change")],
    "Chorus": [("chorus-edit", "The EDIT drawer", "EDIT drawer"), ("chorus-listening", "Listen to wet on", "Listen to wet")],
}

# The cover knob's lit arc: the suite's band colours, as on EQ's bands.
COVER_ARC = ["#E74D3D", "#E77E23", "#F1C315", "#2DCC70", "#00A985", "#3598DB", "#9A59B5"]

GENERIC = {
    "Output": "The header's small teal knob: the level leaving the plugin, after everything else. The same control appears in the EDIT drawer where there is one.",
    "Bypass": "The header's ON pill. Off passes the input through unchanged, with a short crossfade and the plugin's latency kept, so nothing jumps.",
    "Wet": "Level of the processed signal, on the right-column fader.",
    "Dry": "Level of the unprocessed input, on the right-column fader. It's delayed to stay in time with the wet signal.",
}

DESC = {
    "Comp": {
        "Lowpass": "A low-pass filter on what the detector hears (never on the audio you hear). Lower it so bright sounds stop pumping the compressor.",
        "Highpass": "A high-pass filter on the detector. Raise it so the bass stops pumping the compressor.",
        "Detector input": "What the detector listens to: the main input, or the sidechain input (for ducking under a kick or a voice), in stereo or one side.",
        "Character": "The EDIT drawer's Character page: Off, Warm or Bright saturation inside the compressor.",
        "Character position": "Pre saturates before the compressor, so it hears the rounded peaks. Post saturates after make-up gain.",
        "Upward": "Turns on upward compression: quiet passages below Up threshold are raised.",
        "Max lift": "The most upward compression will ever raise a quiet passage.",
        "Floor": "Below this level nothing is raised, so noise and silence stay where they are.",
        "Auto threshold": "Moves Threshold with the input's long-term level, so the amount of compression stays steady between quiet and loud parts.",
        "SC low freq": "Detector EQ, low shelf frequency. The detector EQ shapes what the compressor reacts to, never the audio.",
        "SC low gain": "Detector EQ, low shelf gain. 0 dB is off; boost to make the compressor react more to the lows.",
        "SC mid freq": "Detector EQ, bell frequency.",
        "SC mid gain": "Detector EQ, bell gain. Boosting a bell at 6 to 8 kHz makes the compressor work like a de-esser.",
        "SC mid Q": "Detector EQ, bell width. Higher is narrower.",
        "SC high freq": "Detector EQ, high shelf frequency.",
        "SC high gain": "Detector EQ, high shelf gain.",
    },
    "DS": {"Detection": "Band listens to a bell around Freq; Shelf listens to everything above Freq."},
    "Delay": {
        "Used": "Whether the tap exists. Clicking + in the tabs adds one; × removes it. Not shown as a control.",
        "On": "The power icon on the tab row: a tap that's off takes no input and makes no sound.",
        "Solo": "S on the tab row: hear only the soloed taps. The others keep running silently, so their tails are there when you unsolo.",
        "Time mode": "ms for a free time, Note to follow the host's tempo.",
        "Length": "The tap's time in ms mode, up to 10 seconds. Drag its handle on the map, or type a value.",
        "Note value": "The tap's time in Note mode, from a whole note to 1/64.",
        "Feel": "Straight, Dotted (1.5 times as long) or Triplet (two thirds as long).",
        "Multiplier": "In EDIT: multiplies a Note-mode time, from x1 to x16.",
        "Ping-pong": "Repeats alternate between left and right. Pan picks the starting side.",
        "Filters inside feedback loop": "On: each repeat is filtered again, so repeats get darker as they fade. Off: every repeat keeps the same tone.",
        "Saturate feedback": "A soft clipper on the tap's output and loop, so high feedback saturates instead of growing, and Feedback can go to 110 %.",
        "Smooth time changes": "On: a time change crossfades over 30 ms, with no pitch change. Off: the delay glides to the new time and the pitch bends like tape.",
        "Duck threshold": "In EDIT: the repeats are turned down while the input is above this level.",
        "Duck release": "In EDIT: how quickly the repeats come back once the input drops below the threshold.",
        "Swing": "In EDIT: moves Note-mode taps that land on an odd eighth note later. 50 % is straight.",
        "Saturate curve": "Not on screen: Exact or Fast. New instances use Fast; projects saved before 1.5 keep Exact so they sound as they did.",
    },
    "EQ": {
        "Used": "Whether the band exists. + in the band tabs adds one; × removes it.",
        "On": "The band's power button: off leaves the band set up but silent.",
        "Analyzer size": "In the EDIT drawer: the analyzer's FFT size. Larger shows more detail in the lows, but moves more slowly.",
        "Analyzer window": "In the EDIT drawer: the analysis window. Blackman-Harris suits most material.",
        "Analyzer overlap": "In the EDIT drawer: how often the analyzer updates. More overlap moves more smoothly.",
        "Dynamic": "Makes the band dynamic: it only cuts or boosts while the level in its range crosses Threshold.",
        "Dynamic mode": "Above acts while the band's level is over Threshold (taming harshness); Below acts while it's under (lifting what's too quiet).",
        "Analyzer channel": "In the EDIT drawer: what the analyzer shows: Mid (the sum), Side, Left or Right.",
        "Channel": "Which part of the signal the band works on: Stereo, Left, Right, Mid or Side.",
        "Sidechain": "A dynamic band listens to the sidechain input instead of the track itself.",
    },
    "FIR": {
        "Edit": "How the EQ curve is drawn: Points (smooth), Points (flat) or Freehand.",
        "Low latency": "Halves the latency at the same frequency detail, with a little more smearing.",
        "Reduce artifacts": "Smooths the gain across neighbouring bands, which removes the warbly \"musical noise\" spectral processing can make.",
        "Gain": "The level after processing, before Output.",
        "Gate floor": "Gate mode: how far a band below the curve is turned down. -inf removes it completely.",
        "Compressor ratio": "Compressor mode: how firmly bands above the curve are turned down.",
        "Learn length": "Subtract mode: how many seconds of noise Learn listens to.",
        "Sidechain detector": "Gate and Compressor modes listen to the sidechain input instead of the track.",
        "Show analysis": "Shows the live spectrum on the graph.",
        "Graph top": "The top of the graph's scale.",
        "Analysis floor": "The bottom of the analyzer's scale.",
        "Profile source": "Subtract mode: Learned (once, from a noise-only section), Adaptive (tracked all the time) or Both.",
        "Adapt speed": "Subtract mode, Adaptive: how quickly the tracked noise profile follows changes.",
        "Resonance depth": "Resonance mode: the most a resonance is turned down.",
        "Resonance sharpness": "Resonance mode: how narrow a peak must be to count. Higher catches only sharp ringing.",
        "Resonance attack": "Resonance mode: how quickly the cut arrives when a resonance appears.",
        "Resonance release": "Resonance mode: how quickly the cut lets go.",
        "Resonance low": "Resonance mode, Range: the lowest frequency it works on.",
        "Resonance high": "Resonance mode, Range: the highest frequency it works on.",
    },
    "Gate": {
        "Lowpass": "A low-pass filter on what the detector hears, never on the audio. Use it to stop cymbals opening a kick gate.",
        "Highpass": "A high-pass filter on the detector. Use it to stop the kick opening a snare gate.",
        "MIDI on open/close": "Sends a MIDI note when the gate opens, and its note-off when it closes, to trigger a sample or synth.",
        "MIDI note": "The note it sends.",
        "MIDI channel": "The MIDI channel it sends on.",
        "Velocity from level": "Off: every note has velocity 100. On: the velocity follows how loud the hit is.",
        "Velocity window": "With Velocity from level: how long after opening the gate measures the hit's peak.",
    },
    "Kicker": {
        "Mode": "The Boost | Sub switch: Boost lifts the kick's own low end; Sub adds a clean sine an octave or two below it.",
        "Octave": "Sub mode: how far below the kick the sub plays, -1 or -2 octaves.",
    },
    "Limit": {"Custom target": "The loudness target when Target is set to Custom."},
    "MultiComp": {
        "Active": "The band's power button. An inactive band passes through without compression.",
        "Solo": "Hear only this band (or the soloed bands).",
        "Top frequency": "The crossover at the band's top edge. Drag the band edge on the graph.",
        "Bands": "How many bands, up to 8. + in the band tabs splits the selected band in half; × deletes it.",
        "Detector": "What every band's detector listens to: the main input or the sidechain.",
        "Link bands": "Moving one band's control moves the same control on every band by the same amount. Hold Alt to move only one.",
        "Limit output": "A true-peak limiter after the bands, with a fixed ceiling at 0 dBFS. It adds 2.2 ms of latency.",
        "Attack = 90 % rise": "Attack is the time to reach 90 % of the gain change, rather than the usual 63 %.",
        "Log-scale automated freq.": "Automation of band frequencies moves on a musical (log) scale. Leave it on.",
        "Show spectrum": "Shows the live spectrum behind the bands.",
        "Show grid": "Shows the frequency and level grid.",
        "Upward": "Turns on upward compression for the band: quiet passages are raised.",
        "Depth": "Scales every band's gain change. 0 % changes nothing.",
        "Time": "Multiplies every band's Attack and Release: 200 % is twice as slow.",
        "Channel": "Which part of the signal the band compresses: Stereo, Left, Right, Mid or Side.",
        "Lookahead": "Delays the audio so the compressor can react before a peak arrives, up to 10 ms. Adds that much latency.",
    },
    "Room": {"Pre-delay note": "The pre-delay as a note value, used when Sync pre-delay is on."},
    "Verb": {"Latency mode": "Zero latency has none. Low CPU adds 4096 samples of latency and does nearly all the work in the background."},
}

RANGE = {
    "Room": {"Pre-delay": "0–250 ms", "Pre-delay note": "note values, as Fancy Delay", "Sync pre-delay": "off/on", "Size": "0–100 %",
             "Decay": "0.1–20 s", "Diffusion": "0–100 %", "Mod": "0–100 %", "Width": "-1.00 to +2.00", "Damping": "0–100 %",
             "Low cut": "20–1000 Hz", "High cut": "1–20 kHz", "Freeze": "off/on", "Wet only (send)": "off/on", "Ducking": "0–100 %",
             "Quality": "Eco, Normal, High", "Early reflections": "-inf to +6 dB", "Stereo input": "Stereo, Mono", "Wet": "-inf to +12 dB",
             "Dry": "-inf to +12 dB", "Output": "-24 to +12 dB", "Bypass": "off/on"},
    "Delay": {"Saturate curve": "Exact, Fast"},
    "EQ": {"Channel": "Stereo, Left, Right, Mid, Side", "Analyzer channel": "Mid, Side, Left, Right"},
    "Sature": {"Bypass": "off/on"},
}

# Manual sections that the shared chapter covers, dropped from each plugin chapter.
DROP_SECTIONS = {"Menus and dialogs", "Undo, A/B and Compare", "Menus, undo, A/B and Compare", "Presets and window size",
                 "Using the knobs", "Using the controls", "Credits"}

FIRST_STEPS = r"""
## What a plugin is

A plugin is an effect that runs inside your recording software, which is called a **host** or **DAW** (digital audio workstation): REAPER, Logic Pro, Ableton Live, Cubase and so on. You put a plugin on a track, the track's sound goes through it, and you hear the result. Nothing is changed permanently: switch the plugin off and the track sounds as it did.

Fancy Plugs is a set of sixteen plugins that share one design, so once you know one, the others feel familiar.

## Installing

**Windows.** Download `FancyPlugs-<version>-win64.zip` from the release page, close your DAW and unzip it. Drag the `.vst3` folders from its `VST3` folder into `C:\Program Files\Common Files\VST3`, the folder every Windows DAW looks in; Windows asks once for administrator approval. Then rescan your plugins (in REAPER, *Options > Preferences > Plug-ins > VST > Re-scan*).

**Mac** (Intel and Apple silicon, macOS 11 or later). Unzip `FancyPlugs-<version>-macos.zip` and double-click `Install Fancy Plugs.command`. The first time, macOS blocks it because the plugins aren't signed with an Apple Developer ID: click *Open Anyway* in *System Settings > Privacy & Security* and confirm. It installs the VST3s to `~/Library/Audio/Plug-Ins/VST3` and the Audio Units to `~/Library/Audio/Plug-Ins/Components`, and clears the flag macOS puts on downloaded files, which would otherwise stop them loading. Use the VST3s in REAPER, Live, Cubase, Bitwig and Studio One, and the Audio Units in Logic Pro and GarageBand.

## Putting a plugin on a track

In REAPER, click a track's **FX** button and pick a plugin from the *Fancy Plugs* group. In Logic Pro, click an empty Audio FX slot and choose *Audio Units > Fancy Plugs*. Other hosts work the same way: look for an insert or effect slot on the track.

Two common ways to use an effect:

- **On the track (insert).** All of the track's sound goes through the plugin. Use this for EQ, compression, de-essing, saturation and gating. Wet and Dry together set the mix.
- **On a send (aux or FX track).** Several tracks send some of their sound to one shared track with the plugin on it. Use this for reverb and delay. Set the plugin to wet only (Dry all the way down, or Room's *Wet only (send)*), and set how much each track gets with its send level.

## Reading levels

Levels are in **decibels (dB)**. 0 dBFS is the loudest a digital signal can be; everything below it is negative. A change of 6 dB is about double or half the signal level; 1 dB is a small but audible step. **-inf** means silence.

Meters in Fancy Plugs fill from yellow to green, with a red cap when a signal clips. A **gain-reduction (GR)** meter hangs down from the top: it shows how far a compressor, gate, limiter or de-esser is turning the sound down right now.

**LUFS** measures loudness the way ears hear it, averaged over time. Streaming services play music at about -14 LUFS.
"""

WINDOW = r"""
## The window

Every Fancy Plugs window has the same three parts, top to bottom:

1. The **title strip**: *FANCY* and the plugin's name in its colour. It also says **LISTENING** while a Listen switch is on, so you can't forget one.
2. The **header**: presets, history and output, the same in every plugin.
3. The **body**: the plugin's own controls, described in its chapter. On the right, faders for Wet, Dry or Gain and the meters.

## The header, left to right

**Preset pill.** Shows the loaded preset. Click it to open the preset browser; the ‹ and › arrows inside it step to the previous or next preset without opening it. A **•** after the name means something has changed since the preset loaded.

**+ New.** Saves the current settings as your own preset, under a name you choose. *Ctrl+S* (*Cmd+S* on a Mac) does the same while the plugin window has the keyboard focus.

**Parameter.** A menu with three things: the list of every parameter with its current value (choose one to open your host's own menu for it: automation, MIDI learn and so on), **UI size** (100, 125, 150 or 200 %), and **About**.

**I/O pill.** Shows the channels going in and out, such as *2 in 2 out*. It's information only.

**↶ Undo and ↷ Redo.** Step back and forward through your changes, up to 100. Each drag, click, typed value or preset load is one step, and the buttons' tooltips name it. Automation and changes made by the host aren't steps.

**A | B.** Two complete sets of settings. Click the other letter to switch: the plugin glides to them. **→** copies the active set into the other one. Both sets are saved with the project.

**≈ Compare.** Plays the input instead of the output, at the loudness the output has, so you hear what the plugin does without being fooled by a change in volume. The button shows the matching gain, such as *+2.4 dB*. It turns off when you close the window and is never saved.

**Output knob.** The small teal knob: the level leaving the plugin, from -24 to +12 dB.

**⏻ ON.** Bypass. Off, the input passes through unchanged, with a short crossfade and the plugin's latency kept, so nothing jumps and nothing drifts out of time. The controls dim while it's off.

Fancy Kicker, Exciter and DS-Mini have a **compact header**, because their windows are narrow: the preset pill, **+**, **Prm** and **A | B**. Undo, redo, copy and Compare are inside the **Prm** menu. In narrow windows, *+ New*, *Parameter* and the I/O pill are shortened to **+**, **Prm** and **2/2**.

## The preset browser

Click the preset pill to open it. Presets are grouped by category, with **Factory** presets first and your own **User** presets after them. The search box and the category filter at the top narrow the list.

- **One click** loads a preset and keeps the browser open, so you can try several in a row.
- **Double-click**, or **Enter**, loads it and closes the browser.
- The **star** marks a favourite without loading it.
- **Esc**, or a click outside, closes the browser.
- Its menu has **Save as new preset…**, **Reload** (discard your changes), **Rename…**, **Delete…** and **Open preset folder**.

## Using the controls

Every knob and fader works the same way:

- **Drag** up or down. Hold **Shift** for fine control (eight times finer).
- **Scroll** the mouse wheel to nudge, with **Shift** for smaller steps.
- **Double-click** to go back to the default.
- **Click the value** under a control to type a new one; or **Ctrl-click** anywhere on the control (**Cmd-click** on a Mac, where Ctrl-click is a right-click). Press **Enter**, or click away, to apply it, and **Esc** to cancel.
- **Right-click** for your host's menu for that control: automation, MIDI learn and so on.

Typed values don't need their unit: `120`, `120 ms` and `0.12 s` all mean 120 ms. Frequencies take `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.

**Dropdowns** open a menu; the current choice is ticked. **Checkboxes** fill with the plugin's colour when they're on. A **segment switch**, such as *Boost | Sub*, lights the active choice.

**EDIT** (or **⚙ EDIT**) opens a drawer of less-used settings over the body. **Done** or **EDIT** again closes it.

## Listen switches

Several plugins have a **Listen** checkbox that lets you hear only what they're taking away: the removed noise, the de-essed sibilance, the clipped peaks. It's for setting up: while it's on, the title strip says **LISTENING**. It turns itself off when you close the window, and it's never saved in a project or preset, so it can't end up in a mix by mistake.

## Window size

Drag the window's edge or corner to make it bigger, from 100 to 200 % (the shape stays the same), or pick a size from **Parameter > UI size**. Fancy EQ is the exception: its layout stretches to fill a window of any shape. The size is saved with the project.

## Latency

Some settings make a plugin look ahead or analyse before it plays: lookahead, linear phase, oversampling and spectral processing. The plugin then delays its output a little and tells the host, which delays the other tracks to match, so everything stays in time. Some hosts only notice the change when playback stops and starts, so change these settings while stopped. Each chapter's control reference says which settings add latency.
"""

GLOSSARY = [
    ("Attack", "How quickly a dynamics plugin reacts when the level rises past its threshold."),
    ("Bypass", "Turning a plugin off so the sound passes through unchanged."),
    ("Ceiling", "The highest level a limiter lets through."),
    ("Crossover", "The frequency where one band of a multiband plugin hands over to the next."),
    ("dB, dBFS", "Decibels, the unit of level. dBFS is measured from full scale: 0 dBFS is the loudest possible digital level."),
    ("Detector", "The part of a dynamics plugin that listens to the level and decides how much to act. Filtering the detector changes what it reacts to, not what you hear."),
    ("Dry", "The unprocessed signal."),
    ("FFT", "The method a spectral plugin uses to split sound into narrow frequency bands. A larger FFT size means finer frequency detail and more latency."),
    ("Gain reduction (GR)", "How far a plugin is turning the sound down at a moment, in dB."),
    ("Harmonics", "Frequencies at whole-number multiples of a note's pitch. Saturation and excitation add them; even harmonics sound warm, odd ones edgy."),
    ("Hz, kHz", "Frequency: how high or low a sound is. Human hearing runs from about 20 Hz to 20 kHz."),
    ("IR (impulse response)", "A recording of how a space answers a single click. A convolution reverb plays your sound through it."),
    ("Knee", "How gently a compressor starts working near its threshold. A soft knee eases in; a hard knee starts all at once."),
    ("Latency", "A short delay a plugin adds so it can look ahead or analyse. The host compensates for it."),
    ("Lookahead", "Delaying the audio slightly so a plugin can react before a peak arrives."),
    ("LUFS", "Loudness Units relative to Full Scale: loudness as ears hear it."),
    ("Mid and Side", "The parts of a stereo signal that are the same in both channels (Mid, the centre) and different (Side, the width)."),
    ("Oversampling", "Running a plugin internally at a higher sample rate, to keep distortion clean. It costs CPU and adds a little latency."),
    ("Q", "The width of an EQ band. A higher Q is narrower."),
    ("Ratio", "How firmly a compressor turns down signal above its threshold. At 4:1, 4 dB over the threshold comes out 1 dB over."),
    ("Release", "How quickly a dynamics plugin lets go once the level falls back."),
    ("Sidechain", "A second input that controls a plugin's detector, so one sound can duck or open another."),
    ("Sibilance", "The hissy \"s\", \"sh\" and \"t\" sounds in a voice."),
    ("Threshold", "The level at which a dynamics plugin starts to act."),
    ("True peak", "The real peak level between samples, which can be higher than the samples themselves and clip after conversion."),
    ("Wet", "The processed signal."),
]


# Phase 4 (v1.7.0): ranges and descriptions for the controls Phase 4 added, and for Fancy TS and
# Fancy Chorus, whose manuals don't table their controls. Each comes from the plugin's manual,
# tooltips or design note (docs/<plugin>.md, "Phase 4").
_SYNC_RATES = "1/1 to 1/16, plain, dotted or triplet"

RANGE_P4 = {
    "Comp": {"Sync rate": _SYNC_RATES, "Sync shape": "5–100 %", "Optical": "off/on"},
    "DS-Mini": {"Auto amount": "off/on"},
    "DS": {"Filter slope": "12, 24, 48 dB/oct", "Ceiling": "Off, -12 to 0 dBTP"},
    "Delay": {"Freeze": "off/on", "Ceiling": "Off, -12 to 0 dBTP"},
    "EQ": {"Auto gain": "off/on", "Reach": "1–48 dB"},
    "Exciter": {"Ceiling": "Off, -12 to 0 dBTP", "Auto gain": "off/on"},
    "FIR": {"Resonance tilt": "-100 to +100 %"},
    "Gate": {"MIDI trigger": "off/on", "Ratio": "1.1:1 to ∞", "De-bleed": "0–100 %", "Sync rate": _SYNC_RATES, "Sync shape": "5–100 %"},
    "Kicker": {"Sub length": "20–2000 ms, Off at the top", "Make-room": "off/on", "Make-room freq": "20–200 Hz", "Invert sub": "off/on",
               "Auto gain": "off/on", "Ceiling": "Off, -12 to 0 dBTP"},
    "Limit": {"Gain lock": "off/on", "Gain lock reference": "-30 to 0 dB", "DC filter": "off/on", "Release curve": "Exponential, Linear",
              "Clip before limiting": "off/on", "Clip": "0–12 dB"},
    "MultiComp": {"Mute": "off/on"},
    "Room": {"Mod rate": "0.25–4 ×", "EQ low freq": "20–1000 Hz", "EQ low gain": "-12 to +12 dB", "EQ mid freq": "200–8000 Hz",
             "EQ mid gain": "-12 to +12 dB", "EQ mid Q": "0.3–8", "EQ high freq": "1–20 kHz", "EQ high gain": "-12 to +12 dB",
             "Gate": "off/on", "Gate threshold": "-60 to 0 dB", "Gate hold": "10–500 ms", "Gate decay": "5–500 ms", "Auto gain": "off/on"},
    "Sature": {"Low drive": "0–36 dB", "Low mix": "0–100 %", "High drive": "0–36 dB", "High mix": "0–100 %"},
    "Verb": {"Ducking": "0–100 %", "Ducking source": "Main, Sidechain", "Gate": "off/on", "Gate threshold": "-60 to 0 dB",
             "Gate hold": "10–500 ms", "Gate decay": "5–500 ms"},
    "TS": {"Wet": "-inf to +12 dB", "Dry": "-inf to +12 dB", "Bypass": "off/on"},
    "Chorus": {"Wet": "-inf to +12 dB", "Dry": "-inf to +12 dB", "Bypass": "off/on"},
}

DESC_P4 = {
    "Comp": {
        "Sync rate": "With Host sync as the detector input, the note length of the virtual kick that drives the compressor.",
        "Sync shape": "With Host sync, how long each virtual kick lasts, as a share of its note.",
        "Optical": "Optical ballistics: the attack follows the signal, the release has two stages, and long reduction recovers more slowly.",
    },
    "DS-Mini": {"Auto amount": "Rides Amount from the measured balance of sibilance to voice, so quiet and loud passages are treated alike."},
    "DS": {
        "Filter slope": "How sharply the detector picks out the band. Steeper ignores more of the voice around it. 12 dB/oct is the old sound.",
        "Ceiling": "Holds the output's true peak under this level. Off at the bottom; it adds a little latency while on.",
        "Dual band": "Turns on the second, Low band, for \"sh\" and \"ch\" sounds below the High band's \"s\".",
        "Low freq": "The centre of the Low band.",
        "Low width": "How wide the Low band is, in octaves.",
        "Low threshold": "How loud the Low band must get before it's turned down.",
        "Low range": "The most the Low band turns down. 0 dB does nothing.",
    },
    "Delay": {
        "Freeze": "Stops the input and holds whatever the taps are repeating, for as long as it's on.",
        "Ceiling": "Holds the output's true peak under this level, so high feedback can't clip the bus. Off at the bottom.",
    },
    "EQ": {
        "Auto gain": "Keeps the level steady as you boost and cut, from the bands' response.",
        "Reach": "How far past its threshold a dynamic band reaches its full Range. 12 dB is the old curve; less is harder, more gentler.",
    },
    "Exciter": {
        "Ceiling": "Holds the output's true peak under this level. Off at the bottom.",
        "Auto gain": "Keeps the level steady as you add harmonics, so you hear the colour rather than more volume.",
    },
    "FIR": {"Resonance tilt": "Leans Resonance's depth towards the lows (negative) or the highs (positive)."},
    "Gate": {
        "MIDI trigger": "Plays the gate from MIDI: note-on opens it, note-off closes it, and the detector is ignored.",
        "Ratio": "Below ∞ the gate becomes an expander: under Threshold the level falls by (Ratio − 1) dB per dB, never more than Range.",
        "De-bleed": "Raises the threshold while loud sound sits outside the detector's band, so another drum's bleed can't open the gate.",
        "Sync rate": "With Host sync as the detector input, the note length of each opening.",
        "Sync shape": "With Host sync, how long each opening lasts, as a share of its note.",
    },
    "Kicker": {
        "Sub length": "Ends the sub this long after each hit, with a short fade, so a long tail doesn't run into the next kick. Off at the top.",
        "Make-room": "Cuts the source kick's lowest end below Make-room freq, so the sub has it to itself.",
        "Make-room freq": "Where Make-room's cut starts.",
        "Invert sub": "Flips the sub's polarity, for when it cancels the kick's own low end.",
        "Auto gain": "Keeps the level steady as you change the sub.",
        "Ceiling": "Holds the output's true peak under this level. Off at the bottom.",
    },
    "Limit": {
        "Gain lock": "Holds the output's loudness while Threshold moves, so you hear the limiting, not the level.",
        "Gain lock reference": "The loudness Gain lock holds, set when it's turned on.",
        "DC filter": "Removes DC offset before limiting.",
        "Release curve": "How the release moves: Exponential (the old sound) or Linear.",
        "Clip before limiting": "A soft clipper ahead of the limiter, which shaves the fastest peaks so the limiter has less to do.",
        "Clip": "How hard the clipper works: at 12 dB it clips at the ceiling, at 0 it almost never acts.",
    },
    "MultiComp": {"Mute": "Silences the band, to hear the others without it."},
    "Room": {
        "EQ low freq": "The reverb EQ's low shelf frequency.",
        "EQ low gain": "The reverb EQ's low shelf gain, on the tail only.",
        "EQ mid freq": "The reverb EQ's bell frequency.",
        "EQ mid gain": "The reverb EQ's bell gain, on the tail only.",
        "EQ mid Q": "The reverb EQ's bell width.",
        "EQ high freq": "The reverb EQ's high shelf frequency.",
        "EQ high gain": "The reverb EQ's high shelf gain, on the tail only.",
        "Gate": "Cuts the tail off when the dry signal drops, for a gated drum room.",
        "Gate threshold": "How quiet the dry signal must get before the gate closes.",
        "Gate hold": "How long the gate stays open after the dry signal drops.",
    },
    "Sature": {
        "Low drive": "Drives the band below Low as well, which otherwise passes clean. 0 dB leaves it clean.",
        "Low mix": "How much of the low band's added harmonics you hear.",
        "High drive": "Drives the band above High as well, which otherwise passes clean. 0 dB leaves it clean.",
        "High mix": "How much of the high band's added harmonics you hear.",
    },
    "Verb": {
        "Ducking": "Turns the reverb down while the input, or the sidechain, is loud.",
        "Ducking source": "What Ducking and the Gate listen to: the main input or the sidechain.",
        "Gate": "Cuts the reverb off when its source drops, for a gated sound.",
        "Gate threshold": "How quiet the source must get before the gate closes.",
        "Gate hold": "How long the gate stays open after the source drops.",
    },
    "TS": {
        "Attack": "Turns the start of each hit up (right) or down (left), the same at any level.",
        "Speed": "How long the start of a hit counts as the attack: short for clicks, longer for plucks.",
        "Sustain": "Turns the ring after each hit up or down.",
        "Length": "How long after each hit Sustain works. It never lifts the noise between hits.",
        "Pump": "Dips the level just after each attack, for a pumping, breathing feel.",
        "Sensitivity": "How small a jump in level counts as a hit. Higher catches softer hits.",
        "Highpass": "Keeps low notes from triggering the detector. Off at the bottom.",
        "Contour": "The shape of Sustain's work over Length: Sharp, Medium or Smooth.",
        "Detector": "What finds the hits: this track, the sidechain, or this track's mid or side.",
        "Clip output": "A soft clipper on the output, so a raised attack can't clip harshly.",
        "Link channels": "Both channels shaped together, from the louder one.",
        "Lookahead": "Delays the audio so Attack acts on a hit's very first sample. Adds that much latency.",
        "Oversampling": "Runs Clip output at a higher rate so it doesn't alias, adding latency only while Clip output is on.",
        "Channels": "Shape both channels, or only the mid or the side.",
        "Lowpass": "Keeps highs from triggering the detector. Off at the top.",
        "Dead band": "Hits that would change the level by less than this are left alone.",
    },
    "Chorus": {
        "Mode": "Classic, Ensemble, Dimension, Doubler or Flange: how the voices move.",
        "Rate": "How fast the voices move. Slow for width, fast for vibrato.",
        "Rate (synced)": "With Tempo sync on, one LFO cycle as a note length.",
        "Depth": "How far each voice's delay moves, so how much its pitch wobbles.",
        "Delay": "Each voice's average delay. Flange mode maps it to 0.3–10 ms.",
        "Feedback": "Sends the voices back into themselves; positive and negative give different combs.",
        "Voices": "How many copies of the sound.",
        "Spread": "How far apart the voices' LFOs are. 0 moves them together.",
        "Width": "How far apart the voices sit between left and right. 0 is mono.",
        "Low cut": "Takes lows out of the chorus. Off at the bottom. Keep bass mono uses this frequency.",
        "High cut": "Takes highs out of the chorus. Off at the top.",
        "Age": "An old bucket-brigade chorus's character: darker, a little saturated, with faint hiss.",
        "Tempo sync": "Locks the LFO to the host's tempo and, while it plays, its position.",
        "Keep bass mono": "Keeps the chorus's lows below the Low cut frequency in the centre.",
        "LFO shape": "Sine, Triangle or Smooth random. Doubler always drifts at random.",
        "Input": "Stereo keeps the input's stereo; Mono sums it first, for a mono source made wide.",
        "LFO phase": "How far the right channel's motion is behind the left's. 0° moves them together.",
        "Ducking": "Turns the chorus down by up to 24 dB while the input is loud.",
    },
}

for _plugin, _values in RANGE_P4.items():
    RANGE.setdefault(_plugin, {}).update(_values)

for _plugin, _values in DESC_P4.items():
    DESC.setdefault(_plugin, {}).update(_values)

# Fancy TS and Fancy Chorus: every range written out, as their design notes' tables put units in a
# column of their own.
RANGE.setdefault("TS", {}).update({
    "Attack": "-24 to +24 dB", "Speed": "1–50 ms", "Sustain": "-24 to +24 dB", "Length": "20–1000 ms", "Pump": "0–100 %",
    "Sensitivity": "0–100 %", "Highpass": "Off, 20–2000 Hz", "Contour": "Sharp, Medium, Smooth",
    "Detector": "Main input L+R, Sidechain L+R, Mid, Side", "Clip output": "off/on", "Link channels": "off/on",
    "Lookahead": "Off, 1, 2, 5 ms", "Oversampling": "Off, 2x, 4x", "Channels": "Stereo, Mid only, Side only",
    "Lowpass": "200–20000 Hz, Off", "Dead band": "0–6 dB", "Output": "-24 to +12 dB"})
RANGE.setdefault("Chorus", {}).update({
    "Mode": "Classic, Ensemble, Dimension, Doubler, Flange", "Rate": "0.05–10 Hz", "Rate (synced)": "4/1 to 1/32, plain, dotted or triplet",
    "Depth": "0–100 %", "Delay": "1–30 ms (Flange: 0.3–10 ms)", "Feedback": "-90 to +90 %", "Voices": "1–6", "Spread": "0–100 %",
    "Width": "0–100 %", "Low cut": "Off, 20–1000 Hz", "High cut": "1–20 kHz, Off", "Age": "0–100 %", "Tempo sync": "off/on",
    "Keep bass mono": "off/on", "LFO shape": "Sine, Triangle, Smooth random", "Input": "Stereo, Mono", "LFO phase": "0–180°",
    "Ducking": "0–100 %", "Output": "-24 to +12 dB"})
