# REAPER checklist: preset browser, title strip and resizing (Phase 3 Q2.1)

About 15 minutes. Install the build with `.\scripts\get-latest.ps1`, then rescan in REAPER (Options > Preferences > Plug-ins > VST > Re-scan). Use any two plugins, one small (Kicker or Exciter) and one large (Comp or EQ), unless a step says otherwise.

## Title strip

1. Open each of the nine plugins. Each window has a thin dark strip at the top with "FANCY" in grey and the plugin's name in its colour. Kicker and Exciter still have their grey name at the bottom too.
2. In EQ, select another band, and in FIR change the mode: the name's colour stays the same.

## Preset browser

3. Click the preset name. The browser opens under it, with the cursor in the search box. Factory presets are listed with their categories in grey, and ✓ marks the loaded one.
4. Type part of a preset's name: the list narrows as you type, and the matching letters are in bold. Press Down and Enter: that preset loads and the browser closes.
5. Open it again and click a preset once: it loads and the browser stays open, so you can try several. Click outside the browser: it closes. Open it and press Esc: it closes.
6. Change a knob. The name in the header gets a •, and the browser's footer shows Reload. Click Reload: the knob goes back.
7. Choose a category in the dropdown: only that category's presets are listed. Choose All again.

## Favourites

8. Click the ☆ beside two presets: they turn into filled stars in the plugin's colour, and nothing loads. Choose ★ Favourites in the dropdown: only those two are listed.
9. Open a second instance of the same plugin on another track: the same two are starred.
10. Click a star again to remove it.

## Your own presets

11. Press Ctrl+S (or Save as new… in the browser). Type a name, and in Category type a new category, such as "Test". Save. The browser lists it under User, with "Test" beside it, and the category dropdown has "Test".
12. Save another with Category starting "Te": it completes to "Test" as you type.
13. Open the preset folder with the folder icon. There's a "Test" folder holding both presets. Make a new folder there, copy one preset into it, and open the browser again: the new folder is a category.
14. In that folder, make a text file and rename it `Broken.fancypreset`. It doesn't appear in the browser.
15. Load one of your presets, then Rename… and Delete… it from the footer. Delete asks first.

## Arrows

16. Click › and ‹ in the preset box: the next and previous presets load. Past the last one it goes back to the first.
17. Choose a category in the browser, close it, and use the arrows: they only step through that category.
18. Press Ctrl+Z after an arrow: the previous settings come back in one step.

## Resizing

19. Drag the corner of a fixed-layout plugin's window (any plugin but EQ). The whole layout grows, keeping its shape, from its normal size up to twice that, and no smaller than normal.
20. Close and reopen the window: it opens at the size you left it.
21. In Parameter > UI size, choose 150 %: the window jumps to it and the menu ticks 150 %. Drag it a little: no size is ticked.
22. Drag EQ's window: its graph grows and the controls rearrange, as before.
23. With a window at 200 %, open the browser and a menu: both are drawn at the same size, inside the window.

## Projects

24. Open a project saved with v1.2.0. Every plugin sounds the same, and shows the preset name it had.
