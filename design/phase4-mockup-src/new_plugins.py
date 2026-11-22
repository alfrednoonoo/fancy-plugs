"""Renders the Phase 4 new-plugin mockups (transient.html, chorus.html) to reference/phase4/ at 2x.

    python design/phase4-mockup-src/new_plugins.py
"""
import pathlib
import subprocess

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "design" / "phase4-mockup-src"
OUT = ROOT / "reference" / "phase4"
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

for name, number in (("transient", 50), ("chorus", 51)):
    png = OUT / f"{number}__{name}_mockup.png"
    subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=2",
                    "--window-size=820,1200", f"--screenshot={png}", (SRC / f"{name}.html").as_uri()], capture_output=True)
    image = Image.open(png).convert("RGB")
    pixels = image.load()
    bottom = image.height - 1
    while bottom > 0 and all(pixels[x, bottom] == (18, 18, 18) for x in range(0, image.width, 7)):
        bottom -= 1
    image.crop((0, 0, image.width, min(image.height, bottom + 40))).save(png)
    print("wrote", png.relative_to(ROOT))
