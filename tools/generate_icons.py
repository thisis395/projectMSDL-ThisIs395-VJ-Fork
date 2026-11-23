#!/usr/bin/env python3
"""Generate Windows, macOS, and Linux app icons from the supplied logo.

Requires Pillow. Run before configuring the frontend's CMake project.
"""
from __future__ import annotations

import base64
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "branding" / "portrait-original.png"
OUTPUT = ROOT / "src" / "resources" / "icons"
SIZES = (16, 32, 48, 64, 72, 96, 128, 256)


def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Missing icon source: {SOURCE}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with Image.open(SOURCE) as source:
        master = source.convert("RGBA")
    if master.size != (256, 256):
        master = master.resize((256, 256), Image.Resampling.LANCZOS)

    for size in SIZES:
        master.resize((size, size), Image.Resampling.LANCZOS).save(
            OUTPUT / f"icon_{size}x{size}.png", optimize=True
        )
    master.save(
        OUTPUT / "icon.ico",
        format="ICO",
        sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)]
    )
    master.resize((1024, 1024), Image.Resampling.LANCZOS).save(
        OUTPUT / "icon.icns", format="ICNS"
    )
    encoded = base64.b64encode(SOURCE.read_bytes()).decode("ascii")
    (OUTPUT / "icon_scalable.svg").write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">\n'
        f'  <image width="256" height="256" href="data:image/png;base64,{encoded}"/>\n'
        '</svg>\n', encoding="utf-8"
    )
    print(f"Generated Windows, macOS, and Linux icons from {SOURCE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
