# Fancy Plugs content: intro video and plugin carousels

Status: **approved by the user on 16 November 2026. Part A (the video) is built and approved by the user on 17 November 2026, section 6. Part B (the fourteen carousels) is built, section 7, and waits for the user's review.**

Decided by the user on 16 November 2026: the video is made in two cuts (16:9 and 9:16), the script is read by a generated voice, antislop is applied during the work, and no skill is excluded.

Design read: product introduction for people who mix music in a DAW, in the plugins' own visual language (`docs/DESIGN_SYSTEM.md`, and the site's rules in `docs/website-plan.md` section 3), dial ENERGY 2 / RHYTHM 2 / MOTION 2.

## 1. Rules both parts follow

These come from the design system and the website plan. The content adds nothing to the palette.

- **Colours:** background `#1E1E1E`, panels `#262626` radius 6, wells `#141414`, text `#FFFFFF`, secondary `#8D8D8D`. Dark only, because every screenshot is the dark theme.
- **One accent per piece.** A carousel uses its plugin's accent. In the video the accent changes with the plugin on screen, as the title strip does. Teal `#00A985` means the suite: the Download button and shared scenes. Text on teal is `#141414`.
- **Accent as text** only at large sizes and only where the contrast checker passes; purple text uses `#A970C2`. Otherwise accents are shapes: rings, underlines, markers.
- **Type:** Inter Regular, Medium and SemiBold, from `assets/fonts/`. Uppercase with tracking only in the title strip.
- **Two motifs:** the title strip (`FANCY` in grey, the name in the accent) and the knob ring (a 300 degree arc from 7 o'clock). The logo is used from `design/website/` as it is; no new mark is drawn.
- **Two gradients, each with one job:** the meter gradient where something shows a level or progress, and the seven-accent gradient on the logo's ring only.
- **Images:** only CI screenshots from `docs/images/` (55 views). Never the Phase 1 reference images. No stock art, no illustrations, no icons beyond the ones the plugins draw.
- **Nothing is faked.** A control never moves in a way the screenshot doesn't show. A change of state is a crossfade between two real screenshots (Kicker's Boost to Sub, Comp's main view to its Upward page).
- **Facts:** every number is one the README or a plugin's manual states. No user counts, ratings, quotes or comparisons with other products. The plugins are "free to download and use"; Mac is "beta".
- **Copy:** plain and specific, says what a control lets you do. No em dashes, no buzzwords, sentence case.
- **Sound:** no plugin audio is demonstrated, because no clips exist yet. The video's sound is the voice only, with no music.

## 2. Part A: the intro video

### What it is for

One film that tells a stranger what Fancy Plugs is in 90 seconds and sends them to the download. It goes on the site's home page and YouTube (16:9), and a shorter cut goes to Reels, Shorts and TikTok (9:16).

### Motion

The same reference as the site: the plugins' own behaviour.

| Motion | Where | Why |
|---|---|---|
| The ring draws from 7 o'clock to 5 o'clock, then the prongs come out | Opening, once | It is the logo's idea: a knob that is a plug |
| Title strip text and accent crossfade | Every change of plugin or group | Reads as moving between windows |
| A window slides up 12 px and fades in | Each plugin's first appearance | Brings the eye to the product |
| Slow push toward one control, with a ring marker closing round it | The control the voice is naming | Connects the words to the control |
| Crossfade between two real views | Kicker, Comp, EQ, Verb, Delay | Shows a real change of state |
| The roster's underline slides from name to name | Closing roster | Shows all fourteen in one move |

Eases are the site's (`cubic-bezier(0.2, 0, 0, 1)` in, `0.4, 0, 1, 1` out). No bounce, spring, float, parallax, glow or particle effects. One thing moves at a time. Captions are on screen for every spoken line, white on `#141414`, so the film works muted.

### Cut 1: 16:9, 1920 x 1080, about 90 seconds

| # | Time | On screen | Voice |
|---|---|---|---|
| 1 | 0:00 to 0:07 | `#1E1E1E`. The logo's ring draws, the prongs come out, the wordmark sets beside it. Title strip: `FANCY PLUGS` | Fancy Plugs. Fourteen free effects for the whole mix. |
| 2 | 0:07 to 0:18 | Fancy Comp's window at real size. Ring markers close round the preset name, the A/B switch, then a typed value | Every one of them works the same way. Presets, A and B, undo, and any value you can type. Learn one window and you know them all. |
| 3 | 0:18 to 0:40 | Title strip: `FANCY DYNAMICS`. Comp (push to the Threshold fader over the input meter), MultiComp (band graph), Gate (history display), Limit (ceiling and loudness meter), DS and DS-Mini side by side | Dynamics. Comp puts its threshold right on the input meter. MultiComp splits a mix into as many as eight bands. Gate closes a drum mic between hits. Limit holds a master under a true-peak ceiling. DS softens harsh esses, and DS-Mini does it with one knob. |
| 4 | 0:40 to 1:00 | Title strip: `FANCY TONE`. EQ (crossfade to the Match card), FIR in Resonance mode, Exciter, Sature (curve display), Kicker (crossfade Boost to Sub) | Tone. EQ gives you up to twenty-four bands, and Match moves a track toward a reference. FIR turns down a ringing resonance only while it rings. Exciter adds brightness. Sature adds tape or tube colour. Kicker puts a clean sub under a kick. |
| 5 | 1:00 to 1:15 | Title strip: `FANCY SPACE`. Delay (tap map, crossfade to Motion), Verb (crossfade to the library hall), Room | Space. Delay builds up to sixteen taps in time with the song. Verb comes with thirty-two impulse responses, in true stereo. Room goes from a small room to a long hall. |
| 6 | 1:15 to 1:22 | The roster: fourteen names, a ring in each one's accent, the underline sliding across | Fourteen plugins. One window. |
| 7 | 1:22 to 1:30 | The teal Download button, `Windows` and `Mac (beta)` under it, the site's address, the logo | Free to download for Windows, with Mac in beta. |

About 150 spoken words, a little under 110 words a minute with the pauses between scenes. Timings are fitted to the generated voice once it exists, scene by scene.

Screenshots used: `comp`, `multicomp`, `gate`, `limit`, `ds`, `dsmini`, `eq`, `eq-match`, `fir-resonance`, `exciter`, `sature`, `kicker`, `kicker-sub`, `delay`, `delay-motion`, `verb`, `verb-library`, `room`.

### Cut 2: 9:16, 1080 x 1920, about 45 seconds

A wide window doesn't fit a tall frame, so this cut shows the three compact plugins whole (Kicker, Exciter, DS-Mini, 392 px wide) and shows the wide ones as crops of the control being named. The top 220 px and bottom 420 px stay clear of text, where the apps draw their own buttons.

| # | Time | On screen | Voice |
|---|---|---|---|
| 1 | 0:00 to 0:05 | The ring draws; `FANCY PLUGS`; "Fourteen free effects" | Fourteen free effects for your mix. |
| 2 | 0:05 to 0:16 | `FANCY DYNAMICS`: Comp's Threshold fader crop, then DS-Mini whole | Comp puts its threshold right on the input meter. DS-Mini softens harsh esses with one knob. |
| 3 | 0:16 to 0:27 | `FANCY TONE`: EQ's graph crop with the Match card, then Kicker whole, Boost to Sub | EQ matches a track to a reference. Kicker puts a clean sub under a kick. |
| 4 | 0:27 to 0:36 | `FANCY SPACE`: Verb's library dropdown crop, Room's display crop | Verb comes with thirty-two impulse responses. Room goes from a small room to a long hall. |
| 5 | 0:36 to 0:45 | The roster stacked in three groups, then the teal Download button and the address | Fourteen plugins, one window. Free for Windows, with Mac in beta. |

About 70 spoken words.

### How it is built

- **HyperFrames, local skills.** The hosted HyperFrames tools refuse compose and render from Claude Code and point to the local skills, which write plain HTML compositions that live in this repo. Source goes in `content-src/video/`, renders in `content/` (git-ignored, like `site/`).
- **Voice:** HyperFrames' text-to-speech, one calm voice, one take per scene so a line can be redone alone. I'll play you two voices on scene 1 before generating the rest.
- **Checks before you see it:** the contrast checker on every caption and text colour, every frame of text inside the safe area, each fact against the README, a watch-through of both cuts, and antislop's Delivery Gate.

### Needed before the build (your approval)

1. **Install the HyperFrames skills:** `npx skills add heygen-com/hyperframes`. A new dependency for the content only; nothing in the plugin build changes.
2. **Install ffmpeg** on this PC, which the local renderer needs and which isn't installed.
3. **`build.yml` paths-ignore** gains `content-src/**`, so content work never starts the 10 to 20 minute plugin build.

## 3. Part B: one carousel per plugin

### Format

1080 x 1350 px slides, exported as PNG, six to eight slides each. Built as HTML at the real pixel size and rendered with headless Edge, the way the site's renders were made. Source in `content-src/carousels/<slug>/`.

### Continuous flow

Each carousel is drawn as **one wide canvas** (1080 x the number of slides) and then sliced, so anything that crosses a seam lines up exactly when swiped.

1. **The title strip runs across the whole carousel** along the top: `FANCY COMP` on slide 1, then each slide's own heading in the same strip, with the slide number at the right where the site shows the group.
2. **The plugin's window crosses the seams.** A screenshot is placed so one slide ends partway through it and the next slide shows the rest, with the control that slide is about. The eye is pulled to the next slide by the product itself.
3. **A meter along the bottom** in a `#141414` well, filled with the meter gradient up to the current slide. It is one bar across the canvas, so it reads as progress and continues through every seam.
4. **Each slide's last line sets up the next slide's first.** Slide 2 names the problem; slide 3 opens with the control that solves it.

One focal point per slide: either a crop of the window or one sentence, never both at full weight. Slides differ in composition where their content differs (a full window, a tight crop, a two-view comparison), so no two neighbours have the same shape.

### Slide sequence

| Slide | Job |
|---|---|
| 1 | Cover: the name in the title strip, the kind ("Compressor"), one sentence, the window leaving through the right edge |
| 2 | The job it does, as the task from the site's "Which one do I need?" table |
| 3 to 5, or 6 | One capability each, on a real crop with a ring marker on the control |
| Next to last | Works well with: two or three plugins and why. It names the next carousel in the series |
| Last | Free to download and use, Windows, Mac (beta), the address, the teal Download button |

Each carousel also gets a caption: the plugin's one sentence, its capabilities as two lines, and the address.

### The fourteen, in publishing order

The order is the site's: Dynamics, then Tone, then Space, so each carousel's "works well with" slide hands to a neighbour. Capability slides are drafts from `docs/website-plan.md` section 6; each is checked against the plugin's manual when its carousel is built.

| # | Plugin | Accent | Slide 2: the job | Capability slides | Views used |
|---|---|---|---|---|---|
| 1 | Comp | teal `#00A985` | Even out a vocal | Threshold on the input meter; upward compression; Auto threshold; Character (Warm, Bright); detector EQ | `comp`, `comp-upward`, `comp-character`, `comp-detector` |
| 2 | MultiComp | red `#E74D3D` | Control only the lows or highs of a bus | Up to 8 bands that sum back flat; drag crossovers and thresholds; upward per band; a band on Mid or Side; lookahead to 10 ms | `multicomp`, `multicomp-channel`, `multicomp-edit` |
| 3 | Gate | yellow `#F1C315` | Silence a drum mic between hits | Threshold over the input meter; Gate or Duck from the sidechain; detector filters; 50 ms lookahead; a MIDI note on every opening | `gate`, `gate-listening`, `gate-edit` |
| 4 | Limit | red `#E74D3D` | Stop peaks at the end of a master | True-peak ceiling; loudness meter and Target; three styles; Auto release; hear only what's removed | `limit`, `limit-edit` |
| 5 | DS | green `#2DCC70` | Soften "s" sounds with full control | Relative threshold; Split or Wide cut; minimum- or linear-phase filters; history display; Listen | `ds`, `ds-listening`, `ds-edit` |
| 6 | DS-Mini | blue `#3598DB` | Soften "s" sounds with one knob | One Amount knob; voice and instrument profiles; the same engine as DS; move the setting to DS | `dsmini`, `dsmini-listening`, `dsmini-edit` |
| 7 | EQ | red `#E74D3D` | Shape frequencies | Up to 24 bands, dragged on the graph; a dynamic band; a band on Mid or Side; Match; the analyzer | `eq`, `eq-dynamic`, `eq-channel`, `eq-match` |
| 8 | FIR | purple `#9A59B5` | Remove steady noise or ringing | Draw a curve freehand; learn or track noise; Resonance; gate or compress per part of the spectrum; Listen | `fir-eq`, `fir-adaptive`, `fir-resonance`, `fir-resonance-range` |
| 9 | Exciter | red `#E74D3D` | Add brightness | Brightness above a chosen Freq; Clip Boost; Mix Back in parallel | `exciter`, `exciter-edit` |
| 10 | Sature | orange `#E77E23` | Add warmth or grit | Tape, Tube, Transistor, Clip; even or odd Character; auto gain; oversampling; Listen | `sature`, `sature-listening`, `sature-edit` |
| 11 | Kicker | red `#E74D3D` | Add low end to a kick | Boost with one large Freq knob; Sub, an octave or two below; Wet and Dry | `kicker`, `kicker-sub`, `kicker-sub-edit` |
| 12 | Delay | yellow `#F1C315` | Echoes in time with the song | Up to 16 taps; tempo or milliseconds; Tape and BBD; the Motion page; Ducking and Swing | `delay`, `delay-motion`, `delay-edit` |
| 13 | Verb | purple `#9A59B5` | A real space from an impulse response | 32 built-in IRs; true stereo; a chain of modules; trim on the waveform; Modulate | `verb`, `verb-library`, `verb-generators`, `verb-add` |
| 14 | Room | purple `#9A59B5` | An adjustable room or hall | Size and Decay; Freeze; tempo-synced pre-delay; send mode; ducking | `room`, `room-edit` |

Exciter and Kicker have three capabilities, so their carousels are six slides; the others are seven or eight.

### Working order

Comp first, as the template: its canvas, slices and caption are shown to you before any other plugin is drawn, as the site's Comp page was. Then each plugin in the table's order, one at a time, each approved before the next.

## 4. Which skill does what

The skills this work uses (user, 16 November 2026: the ones with no job here are left out):

| Skill | Use |
|---|---|
| antislop (core), during mode | The purpose test on every visual choice, and the Delivery Gate on each deliverable |
| antislop-ui | Colour, layout and motion of the compositions and slides |
| antislop-copywriting, humanizer | The script, the slide copy and the captions |
| antislop-human | The contrast checker on every text colour pair |
| antislop-layoutmobile | The 9:16 cut and the 1080 x 1350 slides: safe areas, text size on a phone |
| antislop-code | Comments in the composition and build code |
| content-strategy | What each piece is for, the publishing order, and cutting one film into two |
| web-design-guidelines | A review of each HTML composition before it's rendered |
| logo-design | Correct use of the existing mark: clear space, minimum size, no redrawing |
| dataviz | The progress meter and any level shown as a bar |
| HyperFrames (compose, CLI, media) | The video compositions, the renders and the voice |
| agent-browser | Rendering the slides and looking at every one |

## 5. Open questions

1. **The voice.** Two samples on scene 1 for you to pick from, once HyperFrames is installed.
2. **Where the renders go.** Planned: sources committed, MP4 and PNG output git-ignored and handed to you in a folder. Say if the finished files should be committed, or put on the site's home page.
3. **Captions on the 16:9 cut:** burned in (planned), or a separate subtitle file for YouTube as well?

## 6. Part A as built

Built on 16 November 2026 and revised on 17 November after the user's first viewing: the voice changed from George to Michael, the 9:16 cut was laid out again for the tall frame, and each cut gained a subtitle file. The end cards say "Link in description" (16:9) and "Link in bio" (9:16) in place of the address. Both cuts are in `content/` (git-ignored, by the user's choice): `fancy-plugs-intro-16x9.mp4` (1920 x 1080, 75.9 s) and `fancy-plugs-intro-9x16.mp4` (1080 x 1920, 30.8 s), 30 fps, H.264 with AAC, each with a `.srt` of the same name.

**Sources.** `content-src/video/film.py` holds the script, the shots, the camera stops and every timing for both cuts. `content-src/video/build.py wide|tall` turns it into a HyperFrames project in `wide/` or `tall/`: `index.html`, one scene file per beat, the title strip and the captions, and writes the cut's subtitle file to `content/`. The screenshots, the logo and the fonts are copied from `docs/images`, `design/website` and `assets/fonts` at build time, so those generated files are git-ignored. The voice lines (`assets/voice/*.wav`), their text (`voice.json`) and their word timings (`timing/*.json`) are committed.

**To rebuild** (needs Node 22 or later, ffmpeg on the PATH and Pillow):

```
python content-src/video/build.py wide
cd content-src/video/wide
node ~/.claude/skills/hyperframes/scripts/plugin-cli.mjs check
node ~/.claude/skills/hyperframes/scripts/plugin-cli.mjs render --quality looks -o ../../../content/fancy-plugs-intro-16x9.mp4
```

**Voice.** HyperFrames' local text-to-speech (`hyperframes tts`, the Kokoro model), voice `am_michael` (the user's choice on 17 November 2026; the first build used `bm_george`), one file per scene. It needs `kokoro-onnx` and `soundfile` in a Python the CLI can find (`HYPERFRAMES_PYTHON`; here a virtual environment in the git-ignored `.deps/tts-venv`). The spoken text spells out what the model would misread ("D S Mini", "E Q", "F I R"); the captions use the real names. Scene 1 in all three voices that were compared is in `content/voice-samples/`. HeyGen's voices need a sign-in (`hyperframes auth login`) and were not used.

**Word timings** come from `faster-whisper` in the same environment (`content-src/video/timings.py`, which writes `timing/*.json`), because the CLI's own `transcribe` crashed here (whisper-cli exit code 3221225785). The caption and shot times in `film.py` are set by hand from them, so a new voice means running it and re-timing `film.py`.

**The 9:16 layout** (17 November 2026). One centred column: the group line (`FANCY DYNAMICS`, then the kind of plugin), the whole spoken sentence as the caption under it, then the picture. A wide window covers a 1000 x 1020 stage and the camera picks the part being named; Kicker and DS-Mini are shown whole. `hyperframes check` knows where TikTok, Reels and Shorts draw their own buttons: words are clear of all three from y 288 to 1248, in a centred column 696 px wide above y 840 and 480 px wide below it. So every word sits in the upper column, the roster is a narrow list, and the end card puts the address above the button. The picture runs down to y 1620, where only the apps' own caption covers it.

**Checks.** `hyperframes check` passes on both cuts with no errors: contrast 17 of 17 text pairs, no layout or audio findings, and no text outside the TikTok, Reels or Shorts safe areas on the 9:16 cut. The only warnings left say a file holds many timed elements. Every camera stop was looked at as a frame after the re-timing. The user watched and approved both cuts on 17 November 2026.

**The review** (user, 17 November 2026): all fourteen approved.

**The intro carousel** (user, 17 November 2026: "one carousel introducing the plugins"). `suite.py`, built by the same `build.py`, published before Comp's. Its spec gives `PLUGIN` (the strip reads `FANCY PLUGS` in teal, the suite's colour) instead of a plugin from `plugins.json`. Eight slides: cover (the Comp window crosses into slide 2, as in the series); the idea (fourteen effects that work the same way); one slide per group, Dynamics and Tone as a grid of window, name and kind, Space and time as three larger rows so neighbours differ in shape; the shared window (the header from Comp's screenshot, numbered as on the site's home page); the series (all fourteen in publishing order, handing to "First in the series: Fancy Comp"); Download. Group text is the manual's `PARTS`; names, kinds and one-liners are `plugins.json`. New slide kinds in `build.py`: `group`, `shared`, `series`.

**Differences from the plan:**

- The 16:9 cut is 76 s, not about 90, and the 9:16 cut 31 s, not about 45: the voice reads the approved script faster than estimated. The words are the plan's.
- The film's own title strip names the group (`FANCY DYNAMICS`) with the kind of plugin at the right ("Compressor"), because each screenshot already carries its plugin's strip.
- Each group opens on its name as a large word with the ring bullet, for under a second.
- Delay stays on its main view; the planned crossfade to the Motion page didn't fit its four seconds.
- The 9:16 cut has no title-strip bar and no group title cards; see its layout above. Comp's shot starts on the Threshold fader, not on the whole window.
- Captions are burned in, and each cut also has a subtitle file (user, 17 November 2026).
- The site's hotspot coordinates mark a point beside each control, so the film's ring positions are set by eye in `film.py`.

**Not done:** not on the website or YouTube; the renders stay git-ignored (user, 17 November 2026). No music or sound effects, by the plan.

## 7. Part B as built

All fourteen were built on 17 November 2026. The user asked for the other thirteen to be made after Comp without stopping for approval between them, so all fourteen wait for one review. `content-src/carousels/build.py <slug>` reads the plugin's spec (`content-src/carousels/<slug>.py`: slides, crop boxes, ring points, copy, caption) and `site-src/data/plugins.json` (name, accent, pairs), writes the canvas as HTML, prints it with headless Edge and slices it. Output, git-ignored, in `content/carousels/<slug>/`: `<slug>-01.png` to `-08.png` (1080 x 1350), the whole canvas, a contact sheet and `<slug>-caption.txt`.

**Slide counts:** DS-Mini, Exciter and Kicker have seven slides (three capabilities each); the other eleven have eight. Room is the last in the series, so its hand-off line reads "All fourteen are free".

**The link** (user, 17 November 2026): no piece spells the address. The carousels' last slide and caption say "Link in bio"; the film's end card says "Link in description" in the 16:9 cut (YouTube) and "Link in bio" in the 9:16 cut.

**The specs:** a slide's crop box and ring point are pixels of the 2x screenshot. A ring is left off where the control fills its crop (DS-Mini's Amount, Kicker's Freq), and `ring_size` fits it to a large knob or a small checkbox. A crop sits centred in the space its text leaves, text first or crop first, alternating.

Comp has eight slides: cover, the job, Threshold, Upward, Detector (the detector EQ and Auto threshold share a view, so they share a slide), Character, Works well with (the site's pairs, then "Next in the series: Fancy MultiComp"), Download.

**Differences from the plan:**

- The meter is a cell per slide in one well along the bottom, lit with the meter gradient up to the current slide. A single fill can't show progress in a still image, so the cells do.
- The window crosses one seam, from the cover into the job slide. The capability slides use crops of different views, so they don't share a window.
- The strip reads `FANCY COMP` on every slide, with the slide's heading beside it in grey and the number at the right.

