# Phase 3, item Q2.1: preset browser and free resizing

Brief: §Q2.1 of `docs/BUILD_PROMPT_PHASE3.md`. Both features are built once, in `fancy-core`, for all nine plugins.

Q2.1 adds no host parameter and no DSP. Nothing on the audio thread changes, so saved projects load and render bit-identical, and CPU doesn't change. The saved state gains nothing a project needs: favourites and categories live in the preset folder, and the window size uses the existing `uiScale`, `uiWidth` and `uiHeight` properties. `stateVersion` stays at its current value.

**Status:** the user accepted every recommended decision in §5 on 9 November 2026 and asked for a mockup first. The same day the user added the plugin's name at the top of every window, and chose a title strip (§3.2, option 2 of 3). Mockups: `reference/phase3/17a__preset_browser_kicker.png`, `17b__preset_browser_comp.png`, `17c__preset_browser_closeups.png` and `18__title_strip.png`, source `design/phase3-mockup-src/preset-browser.html`. **The user approved the mockups on 9 November 2026.**

## 1. Where the plugins stand

- **Presets:** `PresetManager` keeps factory presets in code (`FactoryPreset`: a name, values and optional data) and user presets as `.fancypreset` XML files in `%APPDATA%\Fancy Plugs\<plugin>\Presets`. Clicking the preset pill opens a styled menu, with sections for Factory and User, and then Save, Reload, Rename, Delete and Open preset folder. A preset load is one undo step (`UserEdit "Load preset"`). A user preset that doesn't parse is skipped when it's loaded, but it's still listed.
- **Factory presets:** 5 to 7 per plugin, 52 in all, with no categories.
- **Window size:** every plugin has the size menu (100, 125, 150 and 200 %), which scales the root with a transform. Only Fancy EQ calls `enableResizing`: its layout reflows between 696 × 464 and 1800 × 1300, in 100 % units, and the size is saved. The other eight have fixed layouts drawn for one size.
- **Host scaling (H.3):** the host's scale goes on top of ours through `setScaleFactor`, and a test already checks that every component stays inside the window at host scales 1.25, 1.5 and 2.0.

## 2. Preset browser

### 2.1 Opening it

Clicking the preset pill opens the browser in place of today's menu. It's a pop-up on the `PopupHost` layer, like the menus and dialogs from Phase 2.1 S, with the same panel colour, radius, border, shadow, row height, highlight and fonts as the menus mockup (`reference/phase2_1/16__menus_mockup.png`, values in §S.2). It closes on Esc, on a click outside it, and after a preset loads with Enter or a double-click.

### 2.2 Layout

One layout works for every window, from Kicker (392 wide) up:

```
┌──────────────────────────────────────────────┐
│ [🔍 Search presets          ] [All        ⇕] │  search field, category dropdown
├──────────────────────────────────────────────┤
│   FACTORY                                    │  section labels, as in the menu
│   Default                                 ☆  │
│ ✓ Gentle Glue                     Bus     ★  │  tick, name, category (grey), star
│   Drum Punch                      Drums   ☆  │
│   USER                                       │
│   My vocal chain                  Vocals  ☆  │
│                    …                         │  scrolls, like a long menu
├──────────────────────────────────────────────┤
│ Save as new…  Reload  Rename…  Delete…   📁  │  footer actions
└──────────────────────────────────────────────┘
```

- **Width:** 420 px, or the window width minus 16 px if that's smaller. **Height:** from below the header to 8 px above the window's bottom edge, or shorter if the list is short.
- **Category dropdown:** All, Favourites, the factory categories in the order in §2.4, then User and each user category. It's the shared `PillDropdown` with its styled menu.
- **Rows:** as menu rows (§S.2): the ✓ for the loaded preset is at the left, and Factory and User have section labels. Sections with nothing to show are hidden. The star, at the right end, toggles a favourite without loading: an outline in `#5A5A5A`, filled in the accent colour when on. The name loads on click, the way today's menu does. The category is shown in grey on the right when the dropdown is set to All or Favourites.
- **Footer:** the actions that are in today's menu: Save as new… (Ctrl+S, shown in its tooltip), Reload when the settings have changed, Rename…, Delete…, and a folder icon at the right that opens the preset folder. They're `PillButton`s, so five fit in Kicker's 376 px. Rename and Delete are greyed unless a user preset is loaded, as now. They open the existing styled dialogs.
- **Empty states:** "No favourites yet. Click ☆ beside a preset to add it here." and "No presets in <category> match “<text>”.", with a link to search all categories.
- **Search matches** are drawn in semibold in the name and the category.
- **Keyboard:** the search field takes focus when the browser opens, so typing filters at once. Up and Down move the highlight, Enter loads, Esc closes. When the browser closes, focus goes back to nowhere, so keys reach the host again as H requires.

### 2.3 Search

The search matches the starts of words in the name and the category, case-insensitive, in any order. In Comp, "drum" finds Drum Punch and Parallel Crush, because both are in Drums, and "crush par" finds Parallel Crush. It filters the list within the chosen category.

### 2.4 Categories

**Factory presets** get a `category` field in `FactoryPreset`. Default has none, and it stays first in All. The proposed categories:

| Plugin | Categories and presets |
|---|---|
| Kicker | Kick: Tight Kick, Safe Boom. Bass: Deep Sub, Bass Lift |
| Exciter | Mix: Gentle Air, Warm Top. Vocals: Vocal Presence. Drums: Drum Snap |
| Comp | Bus: Gentle Glue. Vocals: Vocal Control. Drums: Drum Punch, Parallel Crush. Limiting: Brickwall |
| EQ | Clean-up: Low Cut. Vocals: Vocal Presence. Tone: Smile, Air. Effects: Telephone. Mastering: Linear Master |
| Verb | Rooms: Small Room. Plates: Plate. Halls: Hall, Long Tail. Echo: Slapback. Empty stays first, with no category, like Default |
| Delay | Short: Slapback, Haas widener. Tempo: 1/8 dotted, 1/4 ping-pong, Triplet echo. Character: Lo-fi tape |
| MultiComp | Bus: Gentle Glue. Drums: Drum Bus. Vocals: Vocal Smoother. Mastering: Master Linear, Multiband Limiter. Character: Big squash |
| FIR | Tone: Gentle Tilt, De-mud. Restoration: Noise Reduction, Spectral Gate. Dynamics: Spectral Compressor |
| Gate | Clean-up: Vocal clean-up, Gentle expander. Drums: Kick tight, Snare (with spill) · lookahead, Tom (with spill) · lookahead. Sidechain: Duck under kick, Drum trigger to MIDI |

Names and categories are what Q2.1 adds. New factory presets come with each plugin's own Phase 3 item.

**User presets** are categorised by folder: a preset in `Presets\Vocals\` is in the user category "Vocals", and one directly in `Presets\` is just "User". The Save dialog gets a second field, Category (optional), which starts on the loaded user preset's category, suggests existing user categories as you type, and can be left empty. A folder the user makes in Explorer appears as a category. Only one level of folders is read.

Factory and user names can now repeat across categories, so a user preset is identified by its path relative to `Presets\`, such as `Vocals/My chain`. The project keeps storing the preset's name as now, and from Q2.1 the relative path, so an old project's name still finds its file.

### 2.5 Favourites

Favourites are stored per plugin in `%APPDATA%\Fancy Plugs\<plugin>\favourites.xml`, as a list of keys: `factory:<name>` or `user:<relative path>`. They aren't in projects or presets, so they're the same in every project. Renaming or moving a user preset in the browser carries its star. A key whose preset no longer exists is dropped when the file is next written. A favourites file that doesn't parse counts as empty, and it's overwritten only when a star is next changed.

### 2.6 Next and previous arrows

Two small arrows, `‹` and `›`, sit inside the preset pill, at its left and right ends, so the header's layout and widths don't change. They replace the pill's up/down chevron. Each is 18 px wide and turns `#424242` with a white arrow on hover. The name is centred between them and still opens the browser.

- They step through the list the browser would show with its current category and search, and wrap around at the ends. With no filter set, that's every preset: Default and the factory presets in category order, then the user presets.
- Each step is one preset load, so it's one undo step, as W made preset loads.
- If the current settings are modified, stepping away discards them without asking, as choosing a preset from today's menu does. Undo brings them back.
- They have tooltips: "Previous preset" and "Next preset".

### 2.7 Damaged and odd files

The browser lists only user presets that parse as a `.fancypreset` of the right plugin. A file that's empty, truncated, isn't XML, has the wrong root, or comes from another plugin is left out of the list, and it can't be reached by the arrows. The list is read when the browser opens and when the arrows are used, so a file added in Explorer appears without reopening the plugin. Loading never applies part of a damaged file, because `applyState` already checks before it changes anything.

## 3. Free resizing

### 3.1 How it works

- **Fixed-layout plugins** (all except EQ): the window resizes from its host frame's edges and corners, with the aspect ratio locked, between 100 % and 200 % of its base size. The layout scales as a whole, the same transform the size menu uses now, so nothing reflows and nothing is redrawn as a bitmap.
- **EQ** keeps what it does now: its layout reflows between its minimum and maximum sizes, and the size menu scales the result.
- **The size menu stays.** Its four sizes are shortcuts, and the one that matches the current size is ticked. Between them, none is ticked.
- **Saved size:** the scale is saved in the existing `uiScale` property, now any value from 1.0 to 2.0, rounded to 0.01. A build from before Q2.1 that reads a scale it doesn't list opens at 100 %, which is harmless.
- **Host scaling:** our scale and the host's multiply, as now. The resize limits are given to the host in its own pixels, so at 150 % Windows scaling a Kicker can be dragged from 588 to 1176 px wide.

### 3.2 Title strip

Added by the user on 9 November 2026: every window shows the plugin's name at the top. The header row has no room for it on most plugins, so it gets its own strip above the header, the same on all nine and on the new plugins.

- 20 px tall, across the full width, background `#141414` (the well colour), with a 1 px `#262626` line under it.
- The name 16 px from the left, aligned with the preset pill, in the section-title style: Inter semibold 10 px, caps, 0.08 em letter spacing. "FANCY" in the secondary grey `#8D8D8D` and the plugin's name in its accent colour (Kicker red, Comp teal, Gate yellow, and so on, as in §S.2).
- It's drawn by `ShellEditor`, so `headerHeight` stays 53 and a new `titleHeight = 20` sits above it. Every window grows by 20 px, nothing else moves, and pop-ups open from the same places relative to the header.
- It's not a control: clicking it does nothing, and the host can still drag the window by its own title bar.
- Kicker and Exciter keep their grey caption at the bottom for now (§5, decision 6).
- The CI screenshots will differ from the `reference/` images by the 20 px strip. The comparison crops it off before comparing, so the rest must still match.

### 3.3 Window sizes, in 100 % units

With the 20 px title strip included.

| Plugin | 100 % (the minimum) | 200 % |
|---|---|---|
| Kicker | 392 × 492 | 784 × 984 |
| Exciter | 392 × 452 | 784 × 904 |
| Comp | 934 × 547 | 1868 × 1094 |
| Verb | 866 × 697 | 1732 × 1394 |
| Delay | 760 × 448 | 1520 × 896 |
| MultiComp | 760 × 609 | 1520 × 1218 |
| FIR | 760 × 465 | 1520 × 930 |
| Gate | 740 × 441 | 1480 × 882 |
| EQ | reflows 696 × 484 to 1800 × 1320 | then × the size menu |


## 4. Tests

All in CI, as part of the existing test run:

- **Round-trip:** save a user preset with a category, list it, load it, and check every parameter and non-parameter child is bit-identical to what was saved. Rename and delete carry or drop its favourite.
- **Damaged presets:** an empty file, a truncated one, random bytes, a non-XML text file, XML with the wrong root, and another plugin's preset are all left out of the list. Loading each by path changes nothing and doesn't crash.
- **Damaged favourites:** an unreadable `favourites.xml` counts as empty and doesn't crash.
- **Search and categories:** fixed lists filter as §2.3 and §2.4 say.
- **Arrows:** they step and wrap through a filtered and an unfiltered list. Each step is exactly one undo step.
- **Resizing:** `fancy-screens` renders all nine plugins at their minimum and at 200 %, and EQ at its minimum and maximum, into the screens artifact. H.3's check that every component stays inside the window runs on each. I compare the renders by eye and note what I see in `docs/PROGRESS.md`.
- **Browser renders:** the gallery gets the browser open in Kicker (the narrowest window) and in Comp, with a search, a favourite and a user category showing, so it can be compared with the browser mockup (§5, decision 1).
- **State:** the v1.1.0 and v1.2.0 state files load and render bit-identical, as for every item.

The REAPER checklist, `docs/presets-checklist.md`, covers the browser, the arrows, favourites across two projects, a preset folder made in Explorer, and dragging each window between its limits.

## 5. Decisions for the user

1. **A browser mockup first?** The browser is a new kind of pop-up that isn't in the menus mockup. I recommend drawing it as HTML in `design/phase3-mockup-src/preset-browser.html`, with the S styling, for you to approve before any code, as Q5 does for new plugins.
2. **The arrows inside the preset pill** (§2.6), so the header doesn't change. The alternative is separate arrow buttons beside the pill, which needs the pill narrowed on every plugin, and Kicker and Exciter's compact header has no room to spare.
3. **The browser replaces the menu** (§2.1). The alternative keeps today's menu and adds "Browse…" to it, which costs an extra click every time.
4. **Scaling, not reflowing, for the eight fixed layouts** (§3.1). Reflowing them the way EQ does would change eight layouts and need eight new designs. Scaling is what "between its minimum and 200 %" reads as.
5. **The factory categories** in §2.4.
6. **Kicker and Exciter's bottom caption.** With the name in the title strip, their grey "Fancy Kicker" / "Fancy Exciter" caption from the Figma design says it twice. Keep it, as the Figma design has it, or remove it. **Decided 9 November 2026: keep it.**
