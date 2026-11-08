# Phase 2.1, item S: styled pop-ups

Brief: §S of `docs/BUILD_PROMPT_PHASE2_1.md`. Target: `reference/phase2_1/16__menus_mockup.png`, source `design/phase2_1-mockup-src/menus.html`. Unlike the other mockups, this one sets the exact styling, so its values win over `fancy-core`'s for these elements.

S changes no parameter, no DSP, no latency and no saved audio state. `stateVersion` stays at 1. Two new properties are added to the saved tree, and older versions ignore both (see "State").

## 1. Audit

Every pop-up, dropdown list, tooltip, value editor and dialog in the nine plugins, as of commit `5943b08`. "JUCE" means it's drawn by JUCE's default `PopupMenu` or `AlertWindow`, in its own top-level window, which is what the brief's screenshot shows. Each row is ticked off once it's converted and shows in the `screens-popups-<sha>` gallery.

### Shared (`fancy-core`), all nine plugins

| # | Element | Where | Today | Becomes | Done |
|---|---|---|---|---|---|
| C1 | Preset menu | `ShellEditor::showPresetMenu` | JUCE menu, one flat list, no sections or actions | `Menu`, as §S.3: Factory and User sections, ✓, Save as new preset…, Reload, Rename…, Delete…, Open preset folder | |
| C2 | Preset dropdown text | `ShellEditor::refreshHeader` | "No preset" by default, no changed mark | "Default" at defaults, " •" once changed | |
| C3 | Save preset prompt (`+ New`, `+`) | `ShellEditor::showSavePrompt` | JUCE `AlertWindow` with a text box | `Dialog` with a text field, action "Save" | |
| C4 | Rename preset prompt | new | none | `Dialog` with a text field, action "Rename" | |
| C5 | Delete preset confirm | new | none | `Dialog`, destructive "Delete" | |
| C6 | Parameter / Prm menu | `ShellEditor::showParameterMenu` | JUCE menu, with a UI size submenu and About | `Menu`, with a "UI size" submenu | |
| C7 | About box | `ShellEditor::showAbout` | JUCE `AlertWindow` | `Dialog`, one "Close" button in the accent colour | |
| C8 | Tooltips | `ShellEditor::tooltips` (`juce::TooltipWindow`, 600 ms) | JUCE default look, and not scaled with the UI size | `Tooltips` overlay: 600 ms, 300 ms on locked controls | |
| C9 | Click-shown tips | `ShellEditor::showTip` (Gate's Pre-open) | the same `TooltipWindow` | the same `Tooltips` overlay, shown at once | |
| C10 | Typed value | `ParamControl::showValueEditor`: every `Knob`, `Fader` and `ThresholdMeter` | `#141414` box, grey outline, teal focus, white caret, 12 px | §S.2 field: `#141414`, 1 px accent border and caret, radius 4, 10.5 px | |
| C11 | Parameter dropdown | `ParamDropdown::clicked`, used by every plugin below | JUCE menu | `Menu`, ✓ on the value, 12 rows then a scrollbar | |
| C12 | Host's parameter menu (right-click a control, or a Parameter menu item) | `ParamControl::showHostMenu` | the host's own menu | unchanged: it's the host's, §S.4 says leave it | n/a |

### Per plugin

| # | Plugin | Element | Where | Today | Done |
|---|---|---|---|---|---|
| K1 | Kicker | none of its own: header (C1-C12) only | | | n/a |
| X1 | Exciter | Oversampling dropdown (EDIT drawer) | `ParamDropdown` | JUCE menu | |
| M1 | Comp | Detector input dropdown | `ParamDropdown` | JUCE menu | |
| M2 | Comp | Oversampling, Phase, Release curve (EDIT drawer) | `ParamDropdown` | JUCE menu | |
| M3 | Comp | Attack tooltip | `setTooltip` | JUCE tooltip | |
| E1 | EQ | Band type dropdown | `BandEditor::TypeDropdown::clicked` | JUCE menu | |
| E2 | EQ | Band handle right-click menu (type, slope submenu, On, Delete band) | `EqGraph::showBandMenu` | JUCE menu at the mouse | |
| E3 | EQ | Delete band (× button and the menu) | `EqEditor::removeSelectedBand`, `EqGraph` | deletes at once | |
| E4 | EQ | Slope dropdowns, Phase, FIR length, Oversampling, Range, Analyzer, FFT, Window, Overlap | `ParamDropdown` | JUCE menu | |
| V1 | Verb | FILE module file list | `ModuleCard::showFileMenu` | JUCE menu, can be long | |
| V2 | Verb | FILE module SRC-gain list | `ModuleCard::showGainMenu` | JUCE menu | |
| V3 | Verb | Latency, Max FFT, IR storage (SETUP drawer) | `ParamDropdown` | JUCE menu | |
| V4 | Verb | Delete module (card ×) | `ModuleCard::mouseUp` | deletes at once | |
| V5 | Verb | Add-module panel | `ModuleRack::showAddPanel` | an inline card in the rack, not a pop-up | n/a, already styled |
| V6 | Verb | Browse… and IR folder | `FileChooser` | native file dialog | n/a, §S.4 keeps it native |
| D1 | Delay | Note dropdown, per tap | `ParamDropdown` | JUCE menu | |
| D2 | Delay | Multiplier dropdowns (EDIT drawer) | `ParamDropdown` | JUCE menu | |
| D3 | Delay | Delete tap (×) | `DelayEditor` | deletes at once | |
| D4 | Delay | Feedback and Pan tooltips | `setTooltip` | JUCE tooltip | |
| D5 | Delay | Typed "1/8." and "1/8T" on the note | `parseNote` | sets 1/8 and ignores the dot | |
| P1 | MultiComp | Detector, Crossover dropdowns | `ParamDropdown` | JUCE menu | |
| P2 | MultiComp | Delete band (×) | `MultiCompEditor` | deletes at once | |
| P3 | MultiComp | Ratio, Attack, Gain, Link tooltips | `setTooltip` | JUCE tooltip | |
| F1 | FIR | Edit dropdown, FFT size dropdown | `ParamDropdown` | JUCE menu | |
| F2 | FIR | Point value prompt (right-click a point) | `FirGraph::typePoint` | JUCE `AlertWindow`, two text boxes | |
| F3 | FIR | Graph top and Analysis floor prompts (double-click the axis ends) | `FirGraph::typeAxis` | JUCE `AlertWindow` | |
| F4 | FIR | Clear learned noise profile | `FirEditor` Clear button | clears at once | |
| F5 | FIR | Low latency, Ratio tooltips | `setTooltip` | JUCE tooltip | |
| G1 | Gate | Detector input dropdown | `ParamDropdown` | JUCE menu, flat | |
| G2 | Gate | MIDI dropdown: note, channel and velocity window | `MidiDropdown::mouseUp` | JUCE menu, eleven octave submenus | |
| G3 | Gate | History, Hysteresis, Range, Hold, Preview, Link, MIDI, Noise, Pre-open, Velocity tooltips | `setTooltip` | JUCE tooltip | |
| G4 | Gate | Pre-open locked hint (dimmed, 300 ms) | `ParamControl::setLocked`, `showTip` | JUCE tooltip | |

The shared `ParamSegments`, `ColourTabs`, `ParamCheckbox` and the pill buttons open nothing, so they aren't listed. The Delay and FIR mode switches are segment controls, not pop-ups.

## 2. Design

### 2.1 One overlay layer, drawn by us

A menu, tooltip or dialog is a component added to the editor's root, the component that holds the layout and carries the UI-size transform. That gives everything §S.4 asks for in one move:

- it's drawn inside the plugin window, with our fonts, never in a top-level window that can fall back to default styling, appear behind the host or open at the wrong DPI;
- it follows the plugin's 100-200 % scale, and later the host's scale (H.3), because it's under the same transform;
- the screenshot tool can render it with a plain component snapshot, so every pop-up can go into the CI gallery.

JUCE's `PopupMenu` can't do the 12-row list with a thin scrollbar, the accent bar or scrolling the current value into view, so it isn't used at all. A source scan in the tests fails the build if `juce::PopupMenu`, `AlertWindow` or `NativeMessageBox` appears in `fancy-core/` or `plugins/` outside the host's parameter menu, which JUCE hands over as a `HostProvidedContextMenu`.

The small windows (Kicker 392 × 472, Exciter) fit every menu they have: the preset menu with the five actions, at 26 px rows, needs about 420 px with 8 presets. A menu taller than the room left below its dropdown opens above it; if neither fits, it takes the taller side and scrolls. No top-level fallback is needed, so none is built.

New files in `fancy-core`: `ui/Popups.h` and `src/Popups.cpp`, holding:

- **`Menu`**: a list of items built like JUCE's, `addItem (id, text, enabled, ticked, hint)`, `addSection`, `addSeparator`, `addSubMenu`, an optional icon per item, and `show (target, accent, callback)`. It returns the chosen id, or 0.
- **`Tooltips`**: the editor's tooltip overlay.
- **`Dialog`**: a confirm or prompt panel, with up to two text fields.

### 2.2 Menu

Values from §S.2, at 100 %:

| Part | Value |
|---|---|
| Panel | `#262626`, 1 px `#434343` border, radius 6, 4 px padding, shadow 0 / 10 / 28 px at black 55 % |
| Position | 4 px below the target, left-aligned, at least the target's width. Above it if there's no room below |
| Row | 26 px, Inter 11.5 `#EDEDED`, text from 26 px, 10 px right padding, radius 4 |
| Hover or keyboard focus | `#383838`, and a 2 px rounded bar at the left edge, 5 px from the row's top and bottom, in the accent |
| Ticked | Check mark in the accent, 10 px from the left |
| Disabled | `#5A5A5A`, hint `#4C4C4C`, not hoverable |
| Hint | right-aligned, 10.5 `#909090` |
| Section label | uppercase, 9.5, 0.08 em tracking, `#909090`, semibold, padding 6 / 10 / 3 / 26, 21 px |
| Separator | 1 px `#3A3A3A`, 6 px inset, 4 px above and below |
| Submenu | `›` at the right in `#909090`. Opens right of the row, top-aligned, 2 px overlap, or left if there's no room. Opens after 200 ms hover, or with → |
| Long lists | more than 12 rows: 12 rows high, with a 4 px `#4C4C4C` thumb on a `#1E1E1E` track. The ticked row scrolls into view on opening |
| Dropdown while open | the dropdown itself is drawn `#383838` |

The 12-row limit applies to value lists: parameter dropdowns, the Verb file list and the Gate's note list. The preset menu and right-click menus show every row, as the mockup draws the preset menu with 15 of them, and scroll only when the window is too short.

Keyboard: ↑ and ↓ move the highlight over enabled rows, Enter or Space chooses, Esc closes one level, → opens a submenu and ← closes it. The menu takes keyboard focus while it's open, and gives it back when it closes (H.2).

Clicking outside closes the menu and chooses nothing; the click isn't passed on. Mouse wheel scrolls a long list.

**Accent colour.** `ShellEditor::getAccentColour()`, which a plugin overrides: Kicker and Exciter red, Comp teal, Verb purple, Delay and Gate yellow, EQ and MultiComp the selected band's colour, FIR the current mode's colour. The value used is read when a menu opens.

### 2.3 Preset menu (§S.3)

```
FACTORY
  ✓ Default
    …
USER
    …
─────────
  Save as new preset…        Ctrl+S
  Reload "<name>"     discard changes      (only when changed)
  Rename…                                  (only for a loaded user preset)
  Delete…                                  (only for a loaded user preset; asks first)
  Open preset folder
```

The User section is left out when there are no user presets. Open preset folder creates the folder first, then shows it in Explorer.

**Changed mark.** `PresetManager` keeps the normalised value of every parameter except bypass, taken when a preset loads, and compares it at the header's 30 Hz refresh. Plugins with state outside the parameters (Verb's chain and IR, FIR's curves and noise profile) call `FancyProcessor::noteEdited()`, which bumps a counter the snapshot also holds. With no preset loaded, the snapshot is every parameter's default, so a fresh instance reads "Default" and any change makes it "Default •".

**State.** Two properties are saved with the plugin: `presetModified`, so a project saved with a changed preset reopens with the mark, and the existing `presetName`. A project from v1.1.0 has neither the flag nor, unless it loaded a preset, a name: it reads "Default" if its values are the defaults, and "Default •" if they aren't. That's right, because those projects were all made from the defaults. Neither property touches the audio.

**Ctrl+S** works when the plugin window has keyboard focus. Every other key is left for the host (H.2).

**Rename and delete** act on the loaded user preset's file. Rename refuses a name that's empty or already used, and says so in the dialog.

### 2.4 Dialog

An overlay on the root that dims the window to black 40 % and centres a panel: `#262626`, 1 px `#434343`, radius 8, 16 px padding, 300 px wide, shadow 0 / 14 / 36 px at black 60 %. Title 12.5 semibold, body 11 `#909090` with 1.5 line height, buttons radius 4 with 6 / 14 padding at 11 px: Cancel `#383838`, and the action in red `#E84E3C` when it's destructive, or the accent. Esc is Cancel and Enter is the action. A click on the dimmed area is Cancel.

A prompt adds labelled text fields, drawn like the typed-value field. Used for:

| Use | Kind |
|---|---|
| Save as new preset, `+ New` | prompt, "Save" |
| Rename preset | prompt, "Rename" |
| Delete user preset | confirm, destructive "Delete" |
| Delete an EQ or MultiComp band, a Delay tap, a Verb module | confirm, destructive, only when it has non-default settings (below) |
| Clear the learned noise profile (FIR) | confirm, destructive "Clear" |
| FIR point frequency and gain, graph top, analysis floor | prompt, "Set" |
| About | message, one "Close" button in the accent |

**Non-default, for a delete confirm:** a band or tap is non-default if any of its parameters differs from the parameter's default, apart from frequency and gain on an EQ band, which are always set where you double-clicked. A Verb module is non-default if any of its settings differs from a new module's, or it's a FILE module with a file chosen. Deleting a fresh band, tap or module needs no confirm.

### 2.5 Tooltips

`Tooltips` replaces `juce::TooltipWindow`. Every 50 ms it looks at the component under the mouse; when the mouse has rested on one with a tooltip for 600 ms, or 300 ms if it's a locked `ParamControl` such as the Gate's dimmed Pre-open, it shows the tip. A click, a key, or moving to another component hides it.

Look: `#1E1E1E`, 1 px `#434343`, radius 5, 7 / 10 px padding, Inter 10.5 `#EDEDED`, 1.45 line height, at most 250 px wide with wrapping, shadow 0 / 6 / 18 px at black 50 %. It sits 4 px below the control, left-aligned with it, and moves above or left to stay inside the window.

### 2.6 Typed values

The look changes to §S.2: `#141414`, a 1 px border and caret in the control's accent, radius 4, Inter 10.5, text left-aligned with 8 px padding, at least 74 px wide. `ParamControl` gains `getAccentColour()`, which `Knob`, `Fader` and `ThresholdMeter` already have a colour for.

What opens it stays as in v1.0.1: a click or double-click on the value, or Ctrl-click on the control. The brief asks for a double-click on the value; the single click is kept as well, because the user has used it since v1.0.1 and a double-click is still a click first.

Parsing gains two things the §S.5 table needs:

- **Times:** `parseMs` reads "120", "120ms" and "120 ms" as milliseconds and "0.12 s" or "0.12s" as seconds. Every millisecond parameter switches to it.
- **Delay note:** "1/8." or "1/8d" picks dotted, and "1/8t" triplet, as well as the note.

"abc" and "" leave the parameter alone, as now. Out-of-range values clamp.

### 2.7 Dropdown changes

`ParamDropdown`, `PillDropdown`, `paintDropdownBox` and EQ's type dropdown draw `#383838` while their menu is open. Their closed look stays `fancy-core`'s.

Two dropdowns change their lists to match the mockup:

- **EQ band type:** the five gain types, a separator, then Low pass and High pass with a slope submenu each, then Band pass, All pass and Tilt. Each row shows the type's icon, which the dropdown already draws.
- **Gate MIDI:** three submenus, Note, Channel and Velocity window. Note is one scrolling list of all 128 notes as "A4 · 69", with the current one ticked and scrolled into view, where today it's eleven octave submenus.

The EQ right-click menu becomes the mockup's: a title row "BAND 2 · BELL · 487.7 HZ", Set frequency…, Set gain…, Type ›, Slope › (for the pass types), Enabled ✓, a separator, Reset band (hint "Double-click") and Delete band. Set frequency… and Set gain… open the band editor's typed-value field. "Dynamic" comes with item D. "Del" isn't shown as a hint, because the graph doesn't take keyboard focus (H.2), so the key wouldn't work.

## 3. Tests

1. **Gallery (§S.5.1):** each editor lists its pop-ups through `ShellEditor::getPopupOpeners()`, a name and a function that opens it. `fancy-screens popups <dir>` opens each one in turn, snapshots the editor at 100 % and 200 %, and CI uploads them as `screens-popups-<sha>`. The opener list is the audit's: a pop-up that isn't in it isn't converted.
2. **Attached and styled (§S.5.2):** for every plugin and every opener, the test opens it and checks that an overlay `Menu`, `Dialog` or tip is on the editor's root, and that `juce::PopupMenu::dismissAllActiveMenus()` finds no JUCE menu open. A source scan fails on `juce::PopupMenu`, `AlertWindow` or `NativeMessageBox` in our code.
3. **Keyboard (§S.5.3):** on a menu with a disabled row and a submenu, ↓ skips the disabled row, Enter chooses, → opens the submenu, ← closes it, Esc closes the menu and chooses nothing.
4. **Typed values (§S.5.4):** "120", "120ms", "0.12 s", "2k", "-6", "-6 dB", "1/8.", "abc" and "" through the real parameters' text parsers, checking the value or that nothing changed.
5. **Presets:** the changed mark appears after a change and goes after Reload; "Default" shows on a fresh instance; save, rename and delete act on the files; a project saved with the mark reopens with it.
6. **No change to the sound:** every existing test still passes, including the bit-identical state and automation tests. S touches no DSP.
7. **REAPER (§S.5.5):** one line per plugin on its checklist: open the preset menu and one other dropdown, hover a knob for its tooltip, type a value.

## 4. CPU and latency

None on the audio thread. The changed-mark check compares about 30 to 300 floats at 30 Hz on the message thread.
