# Fancy Plugs design system

Phase 3 Q5, step 1. These rules come from the nine finished plugins as built, read from `fancy-core` and each editor at commit `d0ab806`. A new plugin (Fancy Limiter, Fancy De-esser, Fancy Saturator) follows them, so it reads as part of the suite. Where the existing plugins disagree, the rule names the one to follow and lists the others as exceptions.

Precedence is unchanged: `fancy-core` sets exact styling, except for pop-ups, where `reference/phase2_1/16__menus_mockup.png` and §S.2 of the Phase 2.1 brief do. A new plugin never draws its own version of a shared control. A control `fancy-core` doesn't have is added to `fancy-core` and to the gallery in `tools/screens/Main.cpp` first (Q5).

All sizes are in 100 % units. Fonts are Inter (Regular, Medium, SemiBold), from `fancy::ui::font (Weight, height)`.

## 1. The window

From top to bottom, every window has:

| Part | Height | Source |
|---|---|---|
| Title strip | 20 px | `TitleStrip`, added by `ShellEditor` |
| Header | 53 px | `ShellEditor`, `HeaderStyle::full` or `compact` |
| Body | the plugin's own | `ShellEditor::getBody()` |

A plugin gives `ShellEditor` its width and its header-plus-body height. The title strip is added on top, so the window is 20 px taller than that.

**Resizing.** Every window scales as a whole from 100 % to 200 % (dragging the edge, or the size menu). Only Fancy EQ reflows, with `enableResizing`. A new plugin scales and doesn't reflow, unless its design note says why it should.

### Window sizes

| Plugin | Header | Width × header + body | Window with title strip | Right column |
|---|---|---|---|---|
| Kicker | compact | 392 × 472 | 392 × 492 | In/Out meter (`InOutMeter`) |
| Exciter | compact | 392 × 432 | 392 × 452 | In/Out meter (`InOutMeter`) |
| Comp | full | 934 × 527 | 934 × 547 | Threshold over input, GR, Out, Wet, Dry |
| EQ | full | 802 × 625, reflows 696 × 464 to 1800 × 1300 | 802 × 645 | Gain fader |
| Verb | full | 866 × 677 | 866 × 697 | Wet, Dry |
| Delay | full | 760 × 428 | 760 × 448 | Out, Wet, Dry, 104 px wide |
| MultiComp | full | 760 × 589 | 760 × 609 | In, GR, Out, Wet, Dry, 128 px wide |
| FIR | full | 760 × 445 | 760 × 465 | Out, Gain, 100 px wide |
| Gate | full | 740 × 421 | 740 × 441 | Out, Wet, Dry, Noise, 150 px wide |

**Rule for a new plugin:** use the Phase 2 size class. That is 740 to 760 px wide, with a 53 px header and a body of about 370 to 420 px, which gives a window of about 760 × 445. Don't make it narrower than 740, or the full header's workflow controls need short labels (`setShortHeaderLabels`). Kicker and Exciter's 392 px compact class is only for a plugin with at most four main controls.

## 2. Colours

Everything is in `fancy::ui::colours` (`fancy-core/include/fancy/ui/Style.h`).

| Role | Colour | Name |
|---|---|---|
| Title strip | `#141414`, 1 px `#262626` line at its bottom | `bgWell`, `bgSection` |
| Header | `#1E1E1E` | `bgHeader` |
| Body (page) | `#303030` | `bgBody` |
| Cards and the right column | `#262626` | `bgSection` |
| Wells: meters, graphs, fields | `#141414` | `bgWell` |
| Dropdowns and buttons | `#303030`, radius 3 | `bgBody` |
| Knob face | `#4F4F4F` to `#454545`, 1 px black 20 % border | `knobTop`, `knobBottom` |
| Text | `#FFFFFF` | `textPrimary` |
| Captions, units, card titles, the title strip's "FANCY" | `#8D8D8D` | `textSecondary` |
| Meter fill | gradient `#FFCF07` to `#00FF96`, red clip cap `#E74D3D` | drawn by the meters |

Verb's module rack uses its own `bgVerb` `#292929` and `bgVerbAlt` `#1F1F1F`. A new plugin doesn't use them.

### Colour meanings across the suite

These hold in every plugin, so a new plugin keeps them:

- **Teal `#00A985` (`uiTeal`)**: the header's ON pill and output knob, the Wet, Dry and Gain faders in the right column, and checkboxes in Comp. Never a plugin's main accent, apart from Comp's.
- **Blue `#3598DB` (`uiBlue`)**: Lowpass and Highpass knobs on a detector or a tap (Comp, Delay, Gate). A filter-frequency knob is blue whatever the plugin's accent.
- **The plugin's accent**: its main knobs, its checkboxes, its segment switch's lit segment, the menu's hover bar and check marks, the typed-value field's border and caret, and the name in the title strip.
- **Red `#E74D3D`**: clip caps and destructive dialog buttons (`destructive` `#E84E3C`). As an accent, only Kicker and Exciter use it.

### Accent of each plugin

`getAccentColour()` on the editor sets it, and `setTitleColour` changes the title strip's for a plugin whose accent follows a selection.

| Plugin | Accent |
|---|---|
| Kicker, Exciter | red `#E74D3D` (`uiRed`) |
| Comp | teal `#00A985` (the default, `uiTeal`) |
| EQ | the selected band's colour: red, purple, blue, teal, green, yellow, orange, repeating (`bandColour`) |
| Verb | purple `#9A59B5` (`uiPurple`) |
| Delay | yellow `#F1C315` (`accentYellow`); each tap has its own colour as well |
| MultiComp | the selected band's colour (`multiBandColour`) |
| FIR | the current mode's colour: purple, yellow, teal, red, blue (`modeColour`) |
| Gate | yellow `#F1C315` (`accentYellow`) |

The palette has seven accents (`accentYellow`, `accentOrange`, `accentRed`, `accentPurple`, `accentBlue`, `accentTeal`, `accentGreen`). Only **orange `#E77E23`** and **green `#2DCC70`** aren't yet a whole plugin's accent. Section 10 proposes one for each new plugin.

## 3. Header

Built by `ShellEditor::layoutHeader()`, from the Figma Header/Small and Header/Big components and panel 1 of `reference/phase2_1/14__phase2_1_controls.png`. A plugin never lays out these items itself.

- Items are 24 px tall at y = 13, 8 px apart, from x = 16.
- **Full header**, left to right: preset pill (246 px, with ‹ › arrows inside it, opening the preset browser) · `+ New` (63) · `Parameter` (86) · I/O pill (80) | undo, redo | `A B` switch, copy → | `≈ Compare` (84), with 1 px separators 6 px either side. At the right: the output knob (28 px, `Knob::headerStyle`, teal) and the `⏻ ON` pill (56).
- **Compact header** (Kicker and Exciter): preset pill (110) · `+` (28) · `Prm` (43) · `A B`. Undo, redo, copy and Compare are in the `Prm` menu. ON pill 48.
- **Narrow windows:** `setShortHeaderLabels (true)` turns `+ New`, `Parameter` and the I/O pill into `+`, `Prm` and `4/2`, as Delay, Gate, MultiComp and FIR do. `setPresetDropdownWidth` narrows the preset pill.
- **The plugin's own header items** go between `getHeaderFreeLeft()` and `getHeaderFreeRight()`, in `textSecondary` 14 px, such as Delay's "120.0 BPM (host)". Use this space for read-outs only, not for controls.
- **Title strip:** "FANCY" in `textSecondary` and the plugin's name in its accent, SemiBold 10 px, kerning 0.08, from x = 16.

## 4. Body layout

The Phase 2 plugins share one layout, and a new plugin follows it:

```
┌───────────────────────────────────────────────┬──────────┐
│ display card (graph, map, history)            │ Out  Wet │
│                                               │ ▮▮   │   │
├──── optional tab row (22 px) ─────────────────┤ ▮▮   ▬   │
│ ┌──── card ────┐ ┌──────── card ────────────┐ │ ▮▮   │   │
│ │ knobs        │ │ knobs                    │ │ ▮▮   │   │
│ └──────────────┘ └──────────────────────────┘ │          │
│ ☑ footer checkbox   ☑ footer checkbox  ⚙ EDIT │ -6.1 0.0 │
└───────────────────────────────────────────────┴──────────┘
```

| Rule | Value | Source |
|---|---|---|
| Outer margin of the left area | 13 px | Delay, MultiComp, FIR |
| Gap between cards, both ways | 8 px | Delay, FIR, Gate |
| Card fill and radius | `#262626`, radius 6 | all Phase 2 plugins |
| Display card | 610 px wide, about 150 to 200 px tall, at y = 10 | Delay, MultiComp, FIR |
| Small fills inside a card (title pills, the tab row's buttons) | radius 3 | Delay, Gate |
| Card title | uppercase, SemiBold 12 px, kerning 0.06, `textSecondary`, 12 px in from the card's left edge | Delay "TIME" |
| Footer row | 20 px tall, its top 24 to 32 px above the body's bottom, from x = 13 to 14 | Delay, Gate, MultiComp, FIR |
| Right column | `#262626` from the column's x to the window's right edge, the body's full height | Delay, Gate, MultiComp, FIR |

The right column's left edge is wherever its contents need: 104 px wide for Out, Wet and Dry. Its labels sit on a row at the top, the faders start at y = 26, and the read-outs sit at the bottom. Out is first, then the faders, 33 px apart.

The display card, when a plugin has one, draws in the plugin's own component, because each is different (Delay's tap map, MultiComp's band graph, FIR's spectrum, Gate's history). A new display uses `#141414` or `#262626` as its background, the meter gradient for levels, grid lines in `#303030` (`bgBody`), with `#424242` (`grey5`) for the 0 dB line and `#4C4C4C` (`grey6`) for axes and inactive marks. Its axis labels use `textSecondary`.

## 5. Text sizes

| Use | Size |
|---|---|
| Knob and fader labels, footer checkboxes, captions in a Phase 2 window | **12 px**, Regular |
| Knob and fader labels in the Phase 1 Figma frames | 14 px |
| Gate's captions and checkboxes (its mockup) | 10.5 px |
| Header items, dropdowns, buttons | 14 px |
| Card titles | 12 px SemiBold, uppercase |
| Menu rows / hints / section labels | 11.5 / 10.5 / 9.5 px, from §S.2 |
| Title strip | 10 px SemiBold |

**Rule for a new plugin: 12 px** for labels and footer checkboxes (`Knob::tightStyle`, `ParamCheckbox::setLabelHeight (12)`). Larger is allowed only for a plugin in the compact class.

## 6. Which component for which control

| Kind of control | Component | Notes |
|---|---|---|
| Continuous value, main | `Knob`, `tightStyle` (50 px, 12 px text) in a Phase 2 window, `mediumStyle` (72 px) or `largeStyle` (116 px) in a sparse one | Bipolar for values around a centre (Pan, Gain). `setShowDefaultMark` where the default isn't an end |
| Continuous value, secondary, in a crowded card | `Knob::compactStyle` (no label row) | as FIR's Adapt speed |
| Level or mix in the right column | `Fader`, teal | Wet, Dry, Gain |
| Level inside a card | `Fader` in the card's colour | Delay's tap Volume, MultiComp's band Threshold |
| Threshold over an input meter | `ThresholdMeter` | Comp's style, or Gate's with a scale and read-outs |
| Output level | `OutMeter` | no scale in a Phase 2 right column; `withScale` in Comp's |
| Gain reduction | `GainReductionMeter` | stereo with a scale, or a single bar |
| Input and output, compact class | `InOutMeter` | Kicker, Exciter |
| One of 2 to 4 short choices, all visible | `ParamSegments` | grey lit segment (`#434343`) for a unit or view switch (Delay's ms / Note); accent-filled for the plugin's mode (Gate's Gate / Duck, FIR's modes) |
| One of many choices, or long names | `ParamDropdown` | `addSection` for grouped lists, such as a detector input with "Sidechain" |
| On or off | `ParamCheckbox`, in the plugin's accent | in the footer row, or in a card next to what it affects |
| On or off for a whole section or band | `ParamPowerIcon` | as EQ's bands |
| On or off as a short labelled button | `ParamTextToggle` | as the solo `S` in Delay and MultiComp, and EQ's dynamic chip |
| Numbered items (taps, bands) | `ColourTabs`, with `+` | each item's colour |
| Add or remove an item | `IconButton` plus or cross | |
| A non-parameter action | `PillButton` | such as FIR's Learn and Reset curve |
| Extra settings | `EditLink` opening an `EditDrawer` | section 8 |
| Numbers typed in | already in `Knob`, `Fader` and `ThresholdMeter`: click the value, or Ctrl-click the control | anything drawn outside them needs the same behaviour added |

A knob's value text is in the accent colour by default. `setValueColour` and `setValueSuffix` add a second colour where a mockup asks for it.

## 7. Pop-ups

All from Phase 2.1 S, in `fancy-core/src/Popups.cpp`, styled by `16__menus_mockup.png` and §S.2. A plugin builds them with `Menu`, `showDialog` and `setTooltip`, and never with JUCE's own `PopupMenu`, `AlertWindow` or `TooltipWindow`.

- **Menus:** panel `#262626`, 1 px `#434343` border, radius 6, 4 px padding, shadow. Rows 26 px, 11.5 px `#EDEDED`, radius 4. Hover `#383838` with a 2 px bar in the accent. Check mark in the accent. Lists longer than 12 rows scroll (`setScrollingList`).
- **Tooltips:** `#1E1E1E`, at most 250 px wide, after 600 ms, or 300 ms on a locked control. Every control gets one, in plain words, saying what it does and, if it changes latency, `addLatencyNote`.
- **Dialogs:** panel radius 8, title 12.5 px SemiBold. Esc cancels, Enter acts. A destructive action has a red button and goes through `confirmThen`.
- **Typed values:** a `#141414` field with an accent border and caret (`styleField`).
- **Preset browser:** `PresetBrowser`, opened from the preset pill. A new plugin gives each factory preset a category.

Every pop-up a plugin can open is listed in its `getPopupOpeners()`, so the gallery renders it and the tests open it.

## 8. Footer and EDIT drawer

- **Footer row:** two to four `ParamCheckbox`es for switches that change how the main controls behave (Delay's "Filters inside feedback loop", Gate's "Link channels"). A dropdown can sit in the row when it belongs to the checkbox before it (Gate's MIDI note).
- **EDIT link:** `EditLink` (gear and "EDIT" in the accent) at the right end of the footer row, or the bottom caption in the compact class. It opens an `EditDrawer` over the body, titled "EDIT", closed with Done.
- **What goes in the drawer:** settings that are set once per session, or rarely: oversampling, output trims, multipliers, latency and quality modes, MIDI options. Per the working rules, new controls that aren't in an approved mockup go here, as dropdowns, toggles or small knobs. They never change the main layout.
- Gate has no EDIT drawer, and Verb has its own ⚙ SETUP. A new plugin has the drawer, even if it starts with one item, so later additions have a home.

## 9. States and feedback

- **Bypassed:** `ShellEditor` dims the body. A plugin draws nothing for it.
- **Locked control:** `setLocked` with a hint. It's drawn greyed, and its tooltip explains why (Gate's Pre-open "off" while Lookahead is off).
- **Latency read-out:** a grey note beside the switch that adds latency, such as Gate's "(+50 ms latency)".
- **Sidechain:** an optional sidechain bus shows the shared "No sidechain signal" strip when it's chosen but silent. The I/O pill counts it (`getShownInputs`).
- **Meters:** meters run at 30 Hz from `onTick`, with `MeterChannel` ballistics. Rendered screenshots use `show (...)` with fixed levels and `stopUpdates()`.

## 10. Checklist for a new plugin, and the open decisions

Before its mockup is drawn:

1. Its accent, and whether it follows a selection.
2. Its size class: Phase 2 (about 760 × 445) or compact (392 wide).
3. Its control list, each mapped to a row of section 6.
4. What goes in the footer and in the EDIT drawer.
5. Its right column: Out, and which faders.

For the mockup (Q5 step 2), draw the body with the Phase 2 kit (`design/phase2-mockup-src/common.css` and `lib.js`). Approved 10 November 2026: the kit's header is 36 px, not the shared 53 px, and has no title strip, undo, A/B or Compare. So the mockup's top 73 px are a crop of a CI screenshot of a plugin with the same width, as the Q2.1 mockups were drawn over CI screenshots. That way the header in the mockup is the header that gets built.

Then the CI screenshot is compared side by side with the mockup, and with one existing plugin of similar size (Delay or FIR for the Phase 2 class), as Q5 step 3 asks.

### Accents of the new plugins (approved 10 November 2026)

| New plugin | Proposed accent | Why |
|---|---|---|
| Fancy Saturator | orange `#E77E23` | unused as a plugin accent, and warm, which suits saturation |
| Fancy De-esser | green `#2DCC70` | unused as a plugin accent, and far from Gate's yellow and Comp's teal on the same track |
| Fancy Limiter | red `#E74D3D` | Kicker and Exciter use it, but a limiter is about the ceiling and clipping, which the suite already marks in red. Red chosen over blue, which means a filter knob elsewhere |
