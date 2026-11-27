# Fancy Plugs website: design plan

Status: **approved by the user on 16 November 2026, with the lighter purple; built (section 12).** This note is the design; the build follows it once it's approved, and anything it doesn't cover is asked about, not improvised.

Decided by the user on 16 November 2026: the site is a free download showcase; audio players are designed now and filled later; screenshots get hotspots; it's hosted on GitHub Pages from this repo; the plugins are described as "free to download and use", with no licence named; Mac is labelled beta; the logo's ring is a gradient of the seven accents.

## 1. What the site is

A public site that shows what each of the fourteen plugins can do and sends visitors to the GitHub release to download them free.

- **Audience:** people who mix music in a DAW (REAPER, Live, Logic, Cubase, Bitwig, Studio One) and are looking for effects.
- **Main action on every page:** download the current release.
- **Voice:** plain and specific. It says what a control or mode lets you do, with the plugin's real numbers. No superlatives, no invented statistics, no testimonials, no FAQ unless real questions arrive.
- **What it is not:** a web copy of the manual. The manual explains how to do things, step by step. The site says what can be done, and links to the manual for the how.

Design read: a product showcase for people who mix music, in the plugins' own visual language, at ENERGY 2 / RHYTHM 2 / MOTION 2 (confident but not loud; sections vary where the content does; motion on scroll and on interaction, nothing looping).

## 2. Structure: one page per plugin, plus a page per group

**Chosen: every plugin has its own page, and each of the manual's three groups has a hub page that links to its plugins.** The alternative, three long group pages with in-page links, was rejected:

| | A page per plugin (chosen) | Three group pages with anchors |
|---|---|---|
| Accent colour | The whole page takes the plugin's accent, as the plugin's window does | Six accents on one Dynamics page; the "one accent" rule of the plugins breaks |
| Length | One screenshot explorer, one player, one capability list | Dynamics would carry six explorers and about 60 hotspots on one page |
| Sharing and search | "Fancy Limit" has its own address, title and preview image | A link lands mid-page, and a search result is titled "Dynamics" |
| Phone | Short pages | Very long scrolls, heavy image weight |
| Cost | 14 pages from one template and one data file | 3 pages, but each one unique and hard to keep in step |

The groups still matter, because they answer "which one do I need?". So the hub pages keep the manual's grouping and its in-page links, and add a comparison the manual doesn't have.

### Site map (23 pages)

```
/                         Home
/dynamics/                Hub: Comp, MultiComp, Gate, Limit, DS, DS-Mini
/tone/                    Hub: EQ, FIR, Exciter, Sature, Kicker
/space/                   Hub: Delay, Verb, Room
/plugins/<slug>/          14 pages: comp, multicomp, gate, limit, ds, ds-mini,
                          eq, fir, exciter, sature, kicker, delay, verb, room
/download/                Windows and Mac downloads, requirements, install
/guide/                   Getting started and the shared window; links to the PDF manual
/whats-new/               Release history
/about/                   How it's built, credits, source
/404.html
```

Groups and their one-line descriptions come from `PARTS` in `tools/manual/content.py`, so the site and the manual never disagree.

**Navigation (every item has a real page):** logo (home) · Plugins (a menu listing the three groups and their fourteen plugins) · Guide · What's new · **Download** (button). On a phone the same items sit in a sheet opened by a menu button, closed by Esc or a tap outside, with focus returned to the button.

**Footer:** one row of links (GitHub, manual PDF, releases, About), the version, and the attribution line that the CC BY 4.0 licence requires: UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, modified, with the link; Inter under the SIL Open Font License.

## 3. Visual language

Everything comes from `docs/DESIGN_SYSTEM.md`. The site adds nothing to the palette.

### Colour

| Role on the site | Value | Same as in the plugins |
|---|---|---|
| Page background | `#1E1E1E` | header |
| Section panels, cards, the footer | `#262626`, radius 6 | cards |
| Wells: the screenshot stage, code, the audio player's track | `#141414` | wells |
| Buttons and menus (secondary) | `#303030`, radius 3; menus `#262626`, 1 px `#434343`, radius 6 | controls, pop-ups |
| Text | `#FFFFFF`; body copy `#E2E2E2` | text |
| Secondary text | `#8D8D8D` on `#1E1E1E`, `#262626` and `#141414` only; `#B5B5B5` on `#303030` | captions |
| Suite colour: Download button, links, focus ring on shared pages | teal `#00A985` | header ON pill and output knob |
| Page accent on a plugin page | that plugin's accent (section 6) | title strip, main knobs |
| Level and "before/after" fills | gradient `#FFCF07` to `#00FF96` | meters |

Rules:

- **Dark only, no theme toggle.** The plugins have one dark theme and the screenshots are that theme; a light page around them would misrepresent the product. This is the brand, not a "tech" default.
- **One accent per page.** Shared pages use teal. A plugin page uses its plugin's accent for the title strip, the ring bullets, the hotspot markers, the active tab and the focus ring. The Download button stays teal on every page, so it's always the same button.
- **Teal means "the suite", never one plugin**, as in the plugins. Comp's page is the one place where accent and suite colour coincide, as in Comp itself.
- **Contrast (WCAG AA).** Text on the teal button is `#141414` (white on teal is about 3:1 and fails). Red `#E74D3D` and purple `#9A59B5` fail 4.5:1 as small text on `#1E1E1E`, so an accent is used for text only at 24 px or larger, and otherwise for shapes: rings, underlines, markers. Small text is white or grey. The build runs the contrast checker on every pair.
- **Gradients:** two, each with one job. The meter gradient, only where it shows a level (the audio player's progress). The seven-accent gradient, only on the logo's ring, where it stands for the whole suite (the user's choice, 16 November 2026). Neither is used as a background, on text or on buttons. No glows, blur or patterns.
- **Shadow:** one, on the plugin screenshot, because it's a window sitting above the page. Nothing else floats.
- **Radius:** 3 for controls, 6 for panels, 8 for the screenshot frame and dialogs. Pills only where the plugins have pills (the Download button echoes the `ON` pill).

### Type

Inter, self-hosted from `assets/fonts/` (Regular, Medium, SemiBold) as WOFF2 with `font-display: swap`. Reason: it's the typeface in every plugin window and in the manual, so the site reads as the same product.

| Use | Size / weight |
|---|---|
| Home headline | 56 px SemiBold, -0.02em, line height 1.05 (36 px on a phone) |
| Page title (h1) | 44 px SemiBold (30 px on a phone) |
| Section heading (h2) | 26 px SemiBold, with the ring bullet |
| Sub-heading (h3) | 18 px SemiBold |
| Lede | 18 px Regular, `#8D8D8D` or `#B5B5B5` |
| Body | 16 px Regular, line height 1.6, measure 60 to 72 characters |
| Captions, table text | 14 px |
| Title strip | 11 px SemiBold, uppercase, tracking 0.08em |

Uppercase with tracking is used in exactly one place, the title strip, because that is the plugin's own title strip. Section labels and buttons are sentence case.

### Two motifs, both taken from the plugins

1. **The title strip.** Every page opens with a `#141414` strip, 28 px tall, reading `FANCY` in grey and the page's name in its accent: `FANCY COMP`, `FANCY DYNAMICS`, `FANCY PLUGS` on the home page. It doubles as the breadcrumb (`FANCY` links home; on a plugin page the group follows at the right). It is the first thing in every plugin window, so it's the first thing on every page.
2. **The knob ring.** A 300 degree arc from 7 o'clock. It appears in the logo, as the small ring bullet before each h2 (as in the manual), and as the hotspot marker. Nowhere else.

### Layout

- Content width 1120 px, 24 px gutters (16 px on a phone). Breakpoints at 720 and 1040 px.
- Spacing scale 4, 8, 13, 24, 40, 64, 104 px. 8 and 13 are the plugins' card gap and margin. Sections are 104 px apart on the home page and 64 px on inner pages; related items sit 13 or 24 px apart, so whitespace shows what belongs together.
- Screenshots are shown at their real size where the screen allows (Comp is 934 px wide at 100 %), never upscaled, with `width` and `height` set so nothing shifts. On a phone they scale to the width and can be opened full size.
- Tap targets are at least 44 px. No horizontal scrolling at any width from 320 px.

### Icons and images

- Icons: only the ones the plugins already draw (power, gear, plus, cross, the preset arrows, A/B), redrawn as inline SVG from the Figma icon set in `design/`. No icon library, no emoji.
- Images: only CI screenshots from `docs/images/` and the release's `screens-<sha>` artifact. **Never the Phase 1 reference images**, which stay outside the repo. No illustrations, no stock art.

## 4. Brand: logo and favicon

Made and in `design/website/`. The script that draws them is `design/website/make-brand.py` (it needs `fonttools`; the PNG and ICO files are rendered from the SVGs with headless Edge and Pillow).

**The idea: a knob that is a plug.** The knob's ring is the plugins' own 300 degree arc, which leaves a 60 degree gap at the bottom. A plug's two prongs come out through that gap. "Fancy" is the knob, "Plugs" is the prongs, and the gap that every Fancy knob already has is what makes the two fit. The ring runs through the suite's seven accents as one gradient, from red at 7 o'clock through orange, yellow, green, teal and blue to purple at 5 o'clock: every plugin's colour in one knob. The wordmark is the title strip itself: `FANCY` in `#8D8D8D`, `PLUGS` in white, Inter SemiBold, tracking 0.08em, outlined so it needs no font.

| File | Use |
|---|---|
| `logo-lockup.svg` | Site header and footer: mark plus wordmark, for dark backgrounds |
| `logo-lockup-on-light.svg` | Anywhere light (a README badge, a print) |
| `logo-mark.svg`, `logo-mark-on-light.svg` | The mark alone |
| `logo-mark-mono-white.svg`, `-mono-black.svg` | One colour |
| `favicon.svg`, `favicon.ico` (16, 32, 48), `favicon-16.png`, `favicon-32.png` | Browser tab: simplified, with a thicker ring and prongs and no pointer, on a `#1E1E1E` tile so it reads on light and dark tabs. Checked at 16 px |
| `apple-touch-icon.png` (180), `icon-192.png`, `icon-512.png` | Home-screen icons, square |
| `app-icon.svg` | The full mark on the tile, for larger uses |

Rules: the ring is always the seven-accent gradient in that order, or one flat colour in the mono versions; never a single plugin's accent, and never a different set of colours. SVG can't stroke with a conic gradient, so the ring is drawn as 100 short arcs; use the files as they are and don't rebuild it with a CSS gradient. Clear space around the mark is one prong's length. The mark is not smaller than 16 px; below 24 px use the favicon drawing. Don't rotate it, outline it, or put it on a busy image.

Still to make at build time: the social preview image (1200 × 630, the mark, the wordmark and one screenshot) for each page's `og:image`.

## 5. Page designs

Each page lists what it shows and **what a visitor can do on it**.

### 5.1 Home `/`

```
▓ FANCY PLUGS ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  title strip
 (logo) FANCY PLUGS      Plugins ▾   Guide   What's new      [ Download ]
────────────────────────────────────────────────────────────────────────
 Free effects for the whole mix:       ┌───────────────────────────────┐
 dynamics, tone and space.             │                               │
                                       │   plugin window (screenshot)  │
 Fourteen plugins that all work the    │                               │
 same way. Learn one window ...        └───────────────────────────────┘
 [ Download 1.6.0 ]  See all fourteen   Fancy Comp · Compressor
                                        See what it can do
 COMP MULTICOMP GATE LIMIT DS DS-MINI EQ FIR EXCITER SATURE KICKER DELAY VERB ROOM
────────────────────────────────────────────────────────────────────────
 ◔ Dynamics  ....  six tiles, two rows
 ◔ Tone      ....  five tiles, one wide (EQ) and four
 ◔ Space and time  three tiles, large
────────────────────────────────────────────────────────────────────────
 ◔ One window, fourteen times     annotated crop of the shared header
────────────────────────────────────────────────────────────────────────
 ◔ New in 1.6.0   (text left, FIR Resonance screenshot right)
────────────────────────────────────────────────────────────────────────
 Download band: Windows · Mac (beta) · what you need
 footer
```

- **Hero.** The headline ("Free effects for the whole mix: dynamics, tone and space.", chosen by the user), one sentence ("Fourteen plugins for Windows and Mac that all work the same way. Learn one window and you know them all."), the Download button and a text link to the plugin list. On the right, one plugin window at real size. Under the hero, **the roster**: the fourteen names set like the title strip, each in its own accent (the manual's cover has the same row). Choosing a name swaps the hero window, its caption and its link to that plugin. This is the one place on the site where all seven accents appear together, and it's the focal point: the visitor browses the whole suite without leaving the first screen. The roster is a row of real buttons (arrow keys move, Enter selects); on a phone it scrolls sideways inside its own strip. It doesn't rotate by itself.
- **The three groups.** One section per group with its description from the manual and a tile for each plugin: a cropped screenshot, the name, its kind ("True-peak limiter") and one line. The sections are deliberately not the same shape, because the groups aren't the same size: six, five and three. Each heading links to the hub.
- **One window, fourteen times.** What every plugin shares, shown on a real crop of the full header with six numbered points: the preset browser, A/B and Compare, undo and redo, typed values (click a value and type `2.5k`), resizing from 100 % to 200 %, and the temporary Listen switches. Text sits beside the image, not in cards.
- **New in the latest release.** The first item of the What's new page with its screenshot, and a link to the rest.
- **Download band.** Windows and Mac buttons, requirements in one line, a link to the install steps.

What a visitor can do: browse all fourteen plugins from the hero, jump to any plugin or group, see what's shared, read what's new, download.

Facts shown on this page, all checkable in the repo and updated per release: fourteen plugins, free to download and use; VST3 on Windows 10 and 11 (x64); VST3 and Audio Unit on macOS 11 or later (beta); projects saved with any earlier version load and sound exactly as before. No user counts, ratings or quotes.

### 5.2 Group hubs `/dynamics/`, `/tone/`, `/space/`

```
▓ FANCY DYNAMICS ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
 Dynamics
 Plugins that react to level: they turn things down, or up, ...
 On this page:  Comp · MultiComp · Gate · Limit · DS · DS-Mini · Which one?
────────────────────────────────────────────────────────────────────────
 ┌ screenshot ─────────────┐  ◔ Fancy Comp            Compressor
 │                         │  What it's for, in two sentences.
 └─────────────────────────┘  You can: · · ·  (three short points)
                               See Fancy Comp
 (next plugin, image on the other side) ...
────────────────────────────────────────────────────────────────────────
 ◔ Which one do I need?      table: I want to ... / use ... / because
```

- An in-page list at the top links to each plugin's row and to the comparison (these are the manual's in-page links).
- One row per plugin, image and text alternating sides, each with its accent on the ring bullet only.
- **Which one do I need?** A table from task to plugin. Dynamics: even out a vocal (Comp), control only the lows or highs of a bus (MultiComp), silence a drum mic between hits (Gate), stop peaks at the end of a master (Limit), soften "s" sounds with full control (DS) or with one knob (DS-Mini). Tone: shape frequencies (EQ), remove steady noise or ringing (FIR), add brightness (Exciter), add warmth or grit (Sature), add low end to a kick (Kicker). Space: echoes in time with the song (Delay), a real space from an impulse response (Verb), an adjustable room or hall (Room). On a phone the table becomes a list.

What a visitor can do: compare the plugins in a group, find the right one for a task, go to its page.

### 5.3 Plugin page `/plugins/<slug>/` (one template, fourteen pages)

```
▓ FANCY COMP ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ Dynamics ▓
 Fancy Comp
 Compressor
 One sentence on what it does.                 [ Download ]  Manual chapter
────────────────────────────────────────────────────────────────────────
 [ Main view ] [ Upward page ]                        views (tabs)
 ┌──────────────────────────────────────────────┐   1 Threshold fader
 │   screenshot at real size        ②           │     Set the level where
 │        ①                 ③                   │     compression starts, right
 │                    ④                         │     on the input meter.
 └──────────────────────────────────────────────┘   2 GR meter ...
────────────────────────────────────────────────────────────────────────
 ◔ What you can do with it
   lead capability, large, with a crop          then four to six more as a
                                                two-column list of statements
────────────────────────────────────────────────────────────────────────
 ◔ Hear it        [ Before | After ]  ▶ ▬▬▬▬▬▬▬▬▬▬   (only when a clip exists)
────────────────────────────────────────────────────────────────────────
 ◔ Details        table                 ◔ Works well with   two or three links
────────────────────────────────────────────────────────────────────────
 ‹ Fancy DS-Mini          Dynamics          Fancy MultiComp ›
```

1. **Top.** Title strip in the accent with the group at the right, the name, the kind, one sentence, the Download button and a link to the plugin's chapter in the manual.
2. **Screenshot explorer.** The plugin's views as tabs (the images listed in section 6). On each view, five to eight numbered markers in the accent. Hovering, focusing or tapping a marker highlights its entry in the list beside the image, and choosing an entry highlights its marker. Each entry is the control's name and one sentence that starts from what it lets you do. Keyboard: Tab reaches the tabs and then each marker in number order; arrow keys move between tabs. Below 1040 px the list moves under the image; on a phone the image can be opened full size. Without JavaScript the first view and the full list still show.
3. **What you can do with it.** Outcome statements, not steps: "Keep a vocal steady between quiet and loud passages with Auto threshold", not "Tick Auto threshold in the EDIT drawer". The lead capability gets a large treatment with a crop of the relevant view; the rest are a plain two-column list. No icons, no identical cards.
4. **Hear it.** Switched off for now (user, 17 November 2026): `SHOW_PLAYER = False` in `tools/site/build.py` keeps the code, CSS and script in place for later. A before/after player: one clip pair, a Before | After switch that changes source without losing position, play and pause, and a progress bar in the meter gradient. The section is left out of the page entirely until that plugin has clips, so there is never an empty player. States when present: loading (the bar shows "Loading clip"), failed ("This clip didn't load. Try again", with a retry button). Clips: two files per plugin, the same passage dry and processed, level-matched, 10 to 20 seconds, in `site-src/audio/<slug>/`, named in the data file with the settings used.
5. **Details.** A table: formats, sidechain or MIDI where the plugin has them, whether it adds latency and when, window size, number of factory presets. Every value is read from the plugin's design note and manual at build time; a value that can't be sourced is left out.
6. **Works well with.** Two or three real relations (section 6), each a link with one line of why.
7. **Previous and next** within the group, and the Download band.

What a visitor can do: switch between the plugin's views, explore its controls on the real screenshot, read what it makes possible, hear a before and after (when clips exist), check the details, move to a related plugin, download.

### 5.4 Download `/download/`

- Two panels of different weight: **Windows** (the main one: `FancyPlugs-<version>-win64.zip`, three install steps, where the plugins go, no admin rights) and **Mac** (marked beta until the first release with both zips: the pre-release zip, `install.sh`, the note that the plugins aren't signed with an Apple Developer ID and what the script does about it).
- Requirements and hosts in a short table: VST3 in REAPER, Live, Cubase, Bitwig and Studio One; Audio Units in Logic Pro and GarageBand.
- The version, date and file names come from one data file updated at each release, so the page never calls the GitHub API and has nothing to load or fail.
- A code block with a Copy button for the one command (`xattr`), which confirms with "Copied".

What a visitor can do: download for their system, copy the command, follow the install steps, open the release on GitHub.

### 5.5 Guide `/guide/`

The manual's two shared chapters, shortened: finding the plugins in a host, the three parts of every window, presets, typed values, A/B and undo, resizing, latency. This is the one page that explains how, because it applies to all fourteen. It ends with the manual: a large link to the PDF with its page count and version. A side list of the page's sections follows the scroll position on wide screens.

What a visitor can do: learn the shared window once, jump between sections, open or download the PDF manual.

### 5.6 What's new `/whats-new/`

Releases from newest to oldest, from the README's "What's new" sections and the releases table: version, date, the changes grouped by plugin with links to the plugin pages, and the line about old projects. The newest release is open; older ones are collapsed `details` elements, so the page works without JavaScript.

What a visitor can do: see what each version added, open older releases, go to the plugin that changed.

### 5.7 About `/about/`

Three short sections with different shapes: how a build is checked (the CI steps named in the README: DSP tests, Steinberg's validator, pluginval, the host harness, rendered screenshots; a test count only with the run it comes from), the design credit and licences in full, and the source link. No team section, no photos.

What a visitor can do: read how the plugins are tested, see the credits, go to the source.

### 5.8 404

The title strip reads `FANCY 404`, one sentence, and the roster of fourteen plugin links.

## 6. The fourteen plugin pages

Accent and views come from `tools/manual/content.py` and `docs/images/`. "Can do" lines are the page's capability statements in draft; the build checks each one against the plugin's manual and uses the manual's control names. Hotspots list the controls to mark on the main view.

| Plugin (group) | Accent | Views | Works well with |
|---|---|---|---|
| Comp (Dynamics) | teal `#00A985` | Main; EDIT drawer's Upward page | Sature (Character uses its curves), MultiComp (the same upward compression, per band), Limit |
| MultiComp (Dynamics) | follows the band; page uses red `#E74D3D` | Upward row; a Mid-only band | Comp, Limit, EQ |
| Gate (Dynamics) | yellow `#F1C315` | Main | Comp, Kicker |
| Limit (Dynamics) | red `#E74D3D` | Main | MultiComp, Sature |
| DS (Dynamics) | green `#2DCC70` | Main | DS-Mini (same engine), EQ |
| DS-Mini (Dynamics) | blue `#3598DB` | Main | DS (settings carry over), Comp |
| EQ (Tone) | follows the band; page uses red `#E74D3D` | A dynamic band; the Match card | FIR, Comp |
| FIR (Tone) | follows the mode; page uses purple `#9A59B5` | Subtract with Adaptive; Resonance | EQ, Gate |
| Exciter (Tone) | red `#E74D3D` | Main | EQ, DS |
| Sature (Tone) | orange `#E77E23` | Main | Comp, Exciter |
| Kicker (Tone) | red `#E74D3D` | Boost; Sub | Comp, Gate |
| Delay (Space) | yellow `#F1C315` | Main; Motion page | Verb, Room |
| Verb (Space) | purple `#9A59B5` | Main; a true-stereo library hall | Room, Delay |
| Room (Space) | purple `#9A59B5` | Main | Verb, Delay |

For EQ, MultiComp and FIR, whose accent follows a selection, the page uses the first colour of the sequence and the title strip changes colour with the selected view, as the plugin's own title strip does.

**Fancy Comp.** Compressor.
- Can do: set the threshold directly on the input meter; compress from a sidechain or a filtered detector; lift quiet passages with upward compression; keep gain reduction steady with Auto threshold; add Warm or Bright character before or after; shape what the detector hears with a three-band EQ; catch peaks with the output limiter.
- Hotspots: Threshold fader, GR meter, Ratio and Knee, Attack and Release, Detector input, Lowpass and Highpass, Wet and Dry, the checkbox row.

**Fancy MultiComp.** Multiband compressor.
- Can do: split a mix into up to 8 bands that sum back flat; drag crossovers and thresholds on the graph; compress upward as well as down, per band; put a band on Mid, Side, Left or Right only; look ahead up to 10 ms; solo a band.
- Hotspots: band graph and handles, crossover lines, band tabs, the Upward row, the channel chip, Depth and Time, the meters.

**Fancy Gate.** Gate, ducker and trigger.
- Can do: close a drum mic between hits; duck one track under another from the sidechain; open on only part of the spectrum with the detector filters; look ahead 50 ms so no attack is clipped; send a MIDI note on every opening.
- Hotspots: Threshold over the input meter, the Gate | Duck switch, the history display, Range, Attack, Hold and Release, detector Lowpass and Highpass, the footer row.

**Fancy Limit.** True-peak limiter.
- Can do: hold a master under a true-peak ceiling; work to a loudness target with the built-in meter; choose between three limiting styles; let release adapt with Auto; hear only what's removed; dither the output.
- Hotspots: Ceiling, Threshold, the history display, the loudness meter and Target, Style, Release and Auto.

**Fancy DS.** De-esser.
- Can do: soften "s", "sh" and "t" only while they're too loud; follow the voice's level with a relative threshold; cut only the harsh band or the whole signal; choose minimum- or linear-phase split filters; watch the cuts on a history display; hear only what's removed.
- Hotspots: Threshold, the Freq ruler, Range, the Split | Wide switch, the history display, the GR meter, Listen.

**Fancy DS-Mini.** One-knob de-esser.
- Can do: de-ess with one knob; match the source with voice and instrument profiles; get the same result as Fancy DS, on the same engine; move the setting to Fancy DS as a preset when more control is needed.
- Hotspots: Amount, Profile, the meter, Listen, EDIT.

**Fancy EQ.** Parametric equalizer.
- Can do: shape a track with up to 24 bands by dragging on the graph; watch the live analyzer behind the curve; make a band dynamic so it cuts only when needed; put a band on Mid, Side, Left or Right; match a track to a reference with Match, as one undo step; switch to linear phase.
- Hotspots: band handles, the analyzer, band tabs and `+`, the Type dropdown and channel chip, Freq, Gain and Q, the dynamic chip, the Gain fader, MATCH.

**Fancy FIR.** Spectral processor.
- Can do: draw an EQ curve freehand; remove steady noise by learning it, or track it as it changes; gate or compress each part of the spectrum on its own; turn down ringing resonances only while they ring; hear only what's removed; convolve with a file.
- Hotspots: the Mode switch, the spectrum and curve, Learn, Reduction, Depth and Sharpness, Range, Listen, the Gain fader.

**Fancy Exciter.** Harmonic exciter.
- Can do: add brightness above a chosen frequency without raising the whole top end; make the new harmonics denser with Clip Boost; blend the effect in parallel with Mix Back.
- Hotspots: Freq, Clip Boost, Harmonics, Mix Back, the In/Out meter, EDIT.

**Fancy Sature.** Saturator.
- Can do: add the colour of Tape, Tube, Transistor or Clip; lean the harmonics even or odd with Character; keep the level steady while driving harder, with auto gain; keep the lows or highs clean; saturate in parallel with Dry; oversample to avoid aliasing; hear only the added harmonics.
- Hotspots: Type, Drive, Character, Tone, the curve display, Wet and Dry, Listen.

**Fancy Kicker.** Low-end enhancer.
- Can do: swell a kick's own note with one large Freq knob; add a clean sub an octave or two below that follows the kick's pitch; balance it with Wet and Dry.
- Hotspots: the Boost | Sub switch, Freq, Wet, Dry, the In/Out meter, EDIT. Second view: Cutoff and the octave controls in Sub mode.

**Fancy Delay.** Multi-tap delay.
- Can do: build up to 16 taps, each with its own time, feedback, filters, pan and width; lock times to the project's tempo or set them in milliseconds; give a tap Tape or BBD character; move and diffuse the repeats on the Motion page; duck the echoes under the dry signal; swing the timing; bounce taps left and right with ping-pong.
- Hotspots: the tap map, tap tabs and `+`, the ms | Note switch and time, Feedback, Lowpass and Highpass, the tap's Volume fader, Wet and Dry.

**Fancy Verb.** Convolution reverb.
- Can do: load one of 32 built-in impulse responses or a file of your own; keep each input side's own reflections in true stereo; build the sound as a chain of modules (file, filters, echo and reverb generators, normalize, Modulate); trim the impulse on its waveform; move a long tail gently without pitch wobble.
- Hotspots: the chain strip, the File module and its Library dropdown, the waveform and trim handles, a module's power and drag handle, Pre-delay, Wet and Dry, Add module.

**Fancy Room.** Algorithmic reverb.
- Can do: go from a small room to a long hall with Size, and set the tail's length in seconds with Decay; freeze the tail; sync pre-delay to the tempo; use it as a send with Wet only; duck the reverb under the dry signal; darken it with Damping and High cut.
- Hotspots: Decay, Size, Pre-delay, Damping and High cut, Freeze, the display, Wet and Dry, the footer row.

## 7. Animation guidelines

Motion on the site behaves like the plugins: it answers something the visitor did, it's short, and it ends. MOTION dial 2, raised to 3 on 17 November 2026: the user found the first build's motion too faint to notice, asked for clearly visible reveals and hovers, and for each plugin to be shown working (section 7.1). The demo clips are the one thing that moves by itself and loops.

### Principles

1. **Every movement has a cause the visitor can name:** they pressed, hovered, focused or scrolled something into view. The only exception is the demo clips (section 7.1), which loop while they're on screen.
2. **The plugins' own behaviour is the reference.** A knob turns; a value changes; a drawer slides over the body; a bypass is a short crossfade. The site uses those: crossfades, slides over, and the ring drawing along its arc. No bounce, no spring overshoot, no float, no parallax.
3. **One thing moves at a time.** If the view changes, the markers don't also animate in one by one.
4. **Only `transform` and `opacity`** are animated (plus `stroke-dashoffset` for the ring), so nothing reflows.
5. **Reduced motion:** under `prefers-reduced-motion: reduce`, every transition becomes an instant change or a 100 ms opacity fade, and scroll reveals are off. Nothing is hidden behind an animation: content is in the page before it animates.

### Timing

| Token | Value | Use |
|---|---|---|
| `--t-press` | 90 ms | A button's pressed state |
| `--t-quick` | 140 ms | Hover and focus changes, menu open, tooltip |
| `--t-swap` | 200 ms | Crossfades: explorer views, the hero window, Before and After |
| `--t-move` | 280 ms | Things that travel: the phone menu sheet, a tile lifting |
| `--t-draw` | 500 ms | The ring drawing, once |
| `--t-hero` | 360 ms | The hero window sliding out and in |
| `--t-reveal` | 600 ms | The scroll reveal |
| `--ease-out` | `cubic-bezier(0.2, 0, 0, 1)` | Anything entering or responding |
| `--ease-in` | `cubic-bezier(0.4, 0, 1, 1)` | Anything leaving, at 0.7 of the entering time |

The hotspot tooltip appears after 150 ms of hover and at once on focus or tap. (The plugins wait 600 ms; on a page whose job is exploring, that's too slow.)

### The catalogue: every animation on the site

| Where | What happens | Why |
|---|---|---|
| Logo, on the first page of a visit | The ring draws from 7 o'clock round to 5 o'clock, once, `--t-draw` | It is the knob being turned up; it introduces the motif |
| Home roster | The chosen name's underline slides to it; the hero window slides 28 px out to the left and fades, and the next slides in from the right (`--t-hero`); the caption follows | Shows that the names control the window |
| Screenshot explorer, changing view | The image crossfades; the markers of the new view appear with it, together | Keeps the frame still so the eye stays on the plugin |
| Hotspot marker, on hover or focus | The ring closes from 300 to 360 degrees and the linked list entry gets its accent underline, `--t-quick` | Connects the marker to its text |
| Before and After switch | The lit segment slides, as `ParamSegments` does; audio crossfades over 30 ms | Matches the plugin's own switch; avoids a click in the sound |
| Audio progress | The bar fills with playback | It is a meter |
| Buttons and tiles | Hover: background one step lighter; a tile's border to `#434343` and its picture lifts 4 px (`--t-move`). Press: 1 px down, `--t-press` | Shows it can be pressed and that it was |
| Plugins menu, phone sheet | Menu: fade and 4 px drop, `--t-quick`. Sheet: slides over from the right like the EDIT drawer, `--t-move` | Same behaviour as the plugins' menus and drawer |
| Scroll reveal | Once per element, `--t-reveal`. Screenshots: opacity 0 to 1, 32 px up and scale 0.97 to 1. Headings, group intros, tile names and captions, list entries and the lead text: opacity 0 to 1 and 16 px up. The hero never animates in (user, 17 November 2026) | Makes the page visibly come together as it's read; the first screen is there at once |
| Demo clips | Each plugin's clip loops, silent, while its tile is on screen, and pauses off screen (section 7.1) | Shows the plugin working, which a screenshot can't |
| Moving between a hub and a plugin page | The title strip's name and accent crossfade (View Transitions, where the browser supports it; otherwise a normal page load) | Makes the accent change read as moving between windows |
| `details` on What's new | The chevron turns 90 degrees, `--t-quick` | Shows open and closed |

Anything not in this table doesn't animate. A new animation is added to the table, with its reason, before it's built.

### 7.1 Demo clips (approved by the user on 17 November 2026)

Each plugin's tile on the home page, and the hero window, shows a short clip of the real plugin being used: a pointer moves one or two controls and the plugin's own displays (curve, analyzer, meters, gain reduction, waveform) react to music playing through it.

**Real footage, not a lookalike.** `fancy-screens --demos <dir>` (`tools/screens/Demos.cpp`) opens each plugin's real editor offscreen, plays a synthesized music loop through the real processor in 128-sample blocks at 48 kHz, and moves the scripted parameters along their path. Every display in the plugins updates from the shared window's one 30 Hz tick, so each frame processes exactly 1/30 s of audio and runs exactly one tick (`ShellEditor::tickForCapture`): the clips are the same on every run, with no message loop. Frames are saved as PNG at 2x. The pointer is drawn onto each frame where a hand would hold the control (`ParamControl::getGripPoint`: a knob's centre, moving up as a drag would; a fader's handle; the EQ graph's band handle), with a faint ring while it drags. The screenshot path is unchanged, so no existing screenshot moves.

**The signal.** Synthesized in the tool so the clips need no audio file: a 120 BPM, two-bar loop of kick, snare, hats, a bass line and a sustained chord, deterministic, peaking at about -6 dBFS. Each plugin gets a loop that suits it (Gate, Kicker and the compressors the drums; DS and DS-Mini the hats pushed up; the reverbs and Delay with the snare and chord prominent).

**Length and loop.** 6 seconds, starting and ending on the same setting so the loop has no jump. The first second is played before recording so meters and analyzers are already moving.

**What each clip does:**

| Plugin | Gesture |
|---|---|
| Comp | Threshold fader pulled down 18 dB and back; the GR meter follows |
| MultiComp | The low band's threshold pulled down and back |
| Gate | Threshold raised until the gate chops the hats out, then lowered |
| Limit | Gain pushed up 8 dB and back; gain reduction grows |
| DS | Threshold lowered on bright hats; the history display shows the cuts |
| DS-Mini | Amount turned up and back |
| EQ | Band 2's handle dragged across the graph, up and down; the curve and analyzer follow |
| FIR | Resonance mode: Depth turned up; the peaks it finds are drawn turned down |
| Exciter | Harmonics turned up and back |
| Sature | Drive turned up and back |
| Kicker | The large Freq knob swept |
| Delay | Feedback turned up and back with the taps visible |
| Verb | Length shortened and lengthened; the IR waveform redraws |
| Room | Size and Decay turned up together and back |

The exact parameter IDs and values are in `scripts()` in `tools/screens/Demos.cpp`.

**Files.** CI encodes the frames with ffmpeg, scaled to 1.5x (sharp in the hero on a 2x screen), into `<slug>.webm` (VP9) and `<slug>.mp4` (H.264), silent, and uploads them as `demos-<sha>`. They're copied into `site-src/video/` and committed, as screenshots are into `docs/images/`, and remade when a plugin's look changes. A plugin without a clip shows its screenshot alone. As built (17 November 2026): run 44061446742, 14 clips, 80 to 430 KB each as WebM (EQ, FIR and Gate the largest), 4.8 MB in `site-src/video/` for both formats. Refreshed for v1.7.2 (27 November 2026) from run 45963731259: 16 clips, 7.3 MB for both formats; Comp's is now the largest, about 1 MB as WebM, because its main page shows the history graph. Fancy FIR's loop adds a snare ringing at 740 Hz and 2.9 kHz, so Resonance mode has something to find; EQ's clip selects band 2 first, as clicking its handle does.

**On the page.** A `<video muted loop playsinline preload="none">` with the screenshot as its poster, so the page looks as it does now until a clip loads. A clip starts loading only when its tile is about to scroll into view, plays while it's at least half on screen, and pauses when it isn't. The hero plays the chosen plugin's clip. The 300 KB first-load target stays: clips aren't counted, as audio wasn't. Under reduced motion, or with Save-Data on, only the screenshot shows. The video has no controls and is hidden from screen readers; the image's `alt` text stays.

## 8. Accessibility and performance

- One `h1` per page, landmarks, `lang="en"`, a unique title and description per page, a skip link.
- Everything works by keyboard, with a visible 2 px focus ring in the page accent (teal on shared pages), offset 2 px. Menus, the sheet and the full-size image close with Esc and return focus.
- Screenshots have descriptive `alt` text; the hotspot list is real text beside the image, so a screen reader gets every point without the markers.
- Controls are native elements (`a`, `button`, `details`, `audio`); tabs follow the ARIA tabs pattern.
- Images: WebP with PNG fallback, 1x and 2x, `width` and `height` set; the hero image is in the HTML with `fetchpriority="high"`; everything below the fold is lazy. Fonts are preloaded. No third-party scripts, no analytics, no cookies, so no consent banner.
- Target: each page under 300 KB before audio on first load. Lighthouse and Web Vitals figures are reported only once measured on the deployed site.
- Checks before it ships: the contrast checker on every colour pair, a keyboard pass on every page, widths 320, 390, 768, 1040 and 1440, reduced motion on, JavaScript off, and a click-through of every control with the results recorded.

## 9. How it's built

- **Plain HTML, CSS and JavaScript.** No framework and no runtime dependency. One stylesheet with the tokens above as custom properties; one small script per interactive part (roster, explorer, player, menu).
- **Source and output.** `site-src/` holds the templates, CSS, JS, the data files and audio; `tools/site/build.py` (Python standard library plus Pillow for images, like `tools/manual/`) writes the 23 pages to a build folder. One data file, `site-src/data/plugins.json`, holds each plugin's accent, kind, sentence, views, hotspots (as percentages of the image), capabilities, details and relations; `release.json` holds the version, date and file names. Group and accent data is read from `tools/manual/content.py` so there is one source.
- **Screenshots** are copied from `docs/images/`, with extra views from the release's `screens-<sha>` artifact, at release time.
- **Deploy.** A second workflow, `pages.yml`, runs the build on `ubuntu-latest` and publishes with GitHub's Pages actions when `site-src/`, `tools/site/`, `docs/images/` or the manual PDF change. `site-src/**` and `tools/site/**` are added to `build.yml`'s `paths-ignore`, so site work never starts the plugin build. Address: `https://alfrednoonoo.github.io/fancy-plugs/`, so every link and asset path is relative.
- **Release recipe gains one step:** update `release.json` and the What's new entry, and copy the new screenshots.
- **Build order:** tokens, title strip, header and footer; the plugin template with Comp as the first page, reviewed as a screenshot before the other thirteen; hubs; home; download, guide, what's new, about; then the checks in section 8.

## 10. Decisions and open questions

Decided on 16 November 2026:

- **Licence:** the site says "free to download and use" and names no licence.
- **Mac** is labelled beta and links the pre-release until the first release with both zips.
- **Logo:** the ring is the seven-accent gradient (section 4).
- **Headline:** "Free effects for the whole mix: dynamics, tone and space."

Open:

1. **Purple as small text** (decided, 16 November 2026): `#A970C2` for text on Verb's, Room's and FIR's pages (the title strip, the selected tab, a lit hotspot); shapes keep `#9A59B5`.
2. **Audio clips.** Whenever you record them: one dry and one processed file per plugin, same passage, level-matched, 10 to 20 seconds, music you have the right to publish.
3. **A custom domain** later, or the `github.io` address only? (The plan works with either.)

## 11. Mockup

`design/website/mockup/`: the home page (`index.html`), the Dynamics hub (`dynamics.html`) and Fancy Comp's page (`comp.html`), built by `build.py` from `site.css` and `site.js`, with renders at 1440 px and 390 px wide in `png/`. Open `index.html` in a browser to use it.

What differs from the plan, as drawn:

- **Explorer:** the list of points sits under the screenshot in columns, not beside it. Beside it, eight points ran well past the bottom of the image.
- **Roster:** the names are grey and white with a small ring in each plugin's accent. Names set in their accents at 11 px fail contrast for red and purple.
- **One window:** five numbered points on the header, plus typed values and resizing as two unnumbered ones.
- **Mockup stand-ins:** Manual and What's new link to the PDF and the releases page on GitHub, since the Guide and What's new pages aren't drawn. Tone and Space tiles aren't links yet. The "Hear it" player is shown disabled, with a note, so its design can be judged.

## 12. As built

Built on 16 November 2026. `python tools/site/build.py [out]` writes the 23 pages to `site/` (git-ignored); `.github/workflows/pages.yml` runs it on `ubuntu-latest` and publishes to GitHub Pages when the site's sources change. `build.yml` ignores `site-src/**`, `tools/site/**` and `pages.yml`, so site work never starts the plugin build.

**Sources.** `site-src/data/plugins.json` (each plugin's text, views, hotspots, lead crop, details and pairs, every fact from its manual), `site-src/data/release.json` (downloads, dates, the test count and the releases before 1.3.0), `site-src/assets/site.css` and `site.js`. What's new from 1.3.0 on is read from `README.md`; the groups, their descriptions and the guide's text come from `tools/manual/content.py`; the newest `docs/manual/Fancy-Plugs-Manual-*.pdf` is copied to `manual.pdf`, and each plugin's "Manual chapter" link opens its page, read from the PDF's bookmarks. The logo is inlined from `design/website/make-brand.py`.

**At each release:** update `release.json`, write the README's What's new as usual, and copy new screenshots to `docs/images/`. If a plugin's layout changed, move its hotspots (x and y are percentages of the image).

**Checks run on the build:** every text colour pair at 4.5:1 or more (the build stops otherwise, 55 pairs); then, locally, all 2,208 links and assets resolve, every `#anchor` exists, each page has one `h1`, `lang`, a title, the four landmarks and `alt` on every image, no em dash, and no console errors on any page. A scripted click-through in headless Edge passed 30 of 30: the home roster (click and arrow keys), the Plugins menu (open, Esc, click outside), the phone sheet, explorer tabs (click and arrow keys), hotspot and list highlighting both ways, the manual link, single-view pages without tabs, the strip colours (blue, and the lighter purple), the Copy button's feedback, What's new opening and closing, and the guide's section list at mid-page and at the end. Renders at 1440 and 390 px wide were looked at for every page type.

**Size.** 18.2 MB in all, 10 MB of it the manual. The home page's first load is 207 KB of files before compression (237 KB on a 2x screen): HTML 34 KB, CSS 31 KB, JS 10 KB, three fonts 104 KB (subset to Latin-1 and the symbols the pages use) and the hero at 28 KB (58 KB at 2x), under the 300 KB target. Lighthouse and Web Vitals are not measured.

**Differences from the plan:**

- Screenshots are WebP only (480 px, 1x and 2x), without the PNG fallback; every current browser reads WebP.
- **Every view of every plugin** (user, 16 November 2026): all 55 plugin screenshots from the build's `screens-<sha>` artifact (run 43812185484, commit `a027396`; everything but the component gallery) are in `docs/images/`, 34 of them added for the site. Each is a tab on its plugin's page with its own hotspots: Comp, EQ and Verb 7, FIR 6, Kicker 4, MultiComp, Gate, DS, DS-Mini, Sature and Delay 3, Limit, Exciter and Room 2. Every hotspot was checked by drawing it on its screenshot. When a release changes a plugin's UI, copy the new `screens-<sha>` @2x files over these and check the hotspots of the views that changed.
- EQ's, MultiComp's and FIR's title strips keep one colour, because their screenshots show a fixed title colour.
- A plugin with one view would have no tab row; none has one now. On a phone the tab row wraps.
- Social preview images are drawn with Pillow (`og/<page>.png`), so the build needs no browser.
- The Mac quarantine command is a prefix you paste and then drag a plugin onto, because a wildcard fails in zsh when nothing matches.
- The phone menu lists the three groups and their plugins as well as the pages.

**Not yet tested:** the Before and After player (no clips exist, so no page shows it), the ring drawing and the view transitions by eye in a real browser, and the Copy button's "Copied" (a scripted click can't use the clipboard, so the test saw the fallback). The site is published on its first green `pages.yml` run.
