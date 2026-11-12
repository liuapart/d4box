#!/usr/bin/env python3
"""Render the editor's checked-in SVG source to native macOS/Windows icons.

Requires Pillow and CairoSVG at generation time only; release builds use the
committed .ico/.icns assets and do not need either Python package.
"""

from __future__ import annotations

from io import BytesIO
from pathlib import Path
import subprocess
import sys
import tempfile

import cairosvg
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
BRAND = ROOT / "app/editor/resources/brand"
SVG = BRAND / "app-icon.svg"


def render(size: int) -> Image.Image:
    image = cairosvg.svg2png(url=str(SVG), output_width=size, output_height=size)
    return Image.open(BytesIO(image)).convert("RGBA")


def main() -> None:
    sizes = (16, 24, 32, 48, 64, 128, 256)
    base = render(256)
    base.save(BRAND / "app-icon.ico", format="ICO", sizes=[(n, n) for n in sizes])
    if sys.platform == "darwin":
        with tempfile.TemporaryDirectory(prefix="d4box-icon-") as temporary:
            iconset = Path(temporary) / "D4BoxEditor.iconset"
            iconset.mkdir()
            for size in (16, 32, 128, 256, 512):
                render(size).save(iconset / f"icon_{size}x{size}.png")
                render(size * 2).save(iconset / f"icon_{size}x{size}@2x.png")
            subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o",
                            str(BRAND / "app-icon.icns")], check=True)
    print("Generated native icons from", SVG)


if __name__ == "__main__":
    main()
