# The Fancy Plugs manual

`docs/manual/Fancy-Plugs-Manual-<version>.pdf` is built here, on the PC, from the plugin manuals
(`docs/<plugin>-manual.md`), the design notes' parameter tables and a release build's screenshots.

1. Download the release run's `FancyPlugs-VST3-<sha>` and `screens-<sha>` artifacts.
2. List each plugin's parameters with FancyBench (`FancyBench params "<bundle>.vst3" > params/Fancy <Name>.txt`, in a
   `params` folder under `%TEMP%\manual`), then run `ref.py` and `desc.py` from the repo root: they match the
   parameters to the design notes' ranges and the manuals' descriptions, and write `ref2.json`.
3. `py -3.14 tools/manual/build.py <repo> <out.pdf> <screens folder>`

Every screenshot in `MORE` needs the heading of the section that explains it: there's no "More views" section, and the build stops if one is left over. Each control a screenshot shows must be explained in that plugin's manual (user).

`content.py` holds what's written for the PDF alone: quick starts, the shared chapters, the glossary, and
descriptions for controls the manuals don't table. `manual.css` follows `docs/DESIGN_SYSTEM.md`. The builder
prints with headless Edge, then adds the page backgrounds, the title strip, page numbers, links and bookmarks
with reportlab and pypdf (`pip install markdown pypdf reportlab`).
