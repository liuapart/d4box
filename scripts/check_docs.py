#!/usr/bin/env python3
"""Check this repository's Markdown links and help screenshots, without network access.

Supports the inline links, ATX headings and explicit HTML anchors used in our docs.
Historical upstream README snapshots are intentionally excluded.
"""

from pathlib import Path
import re
import struct
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / "docs/guide"
IMAGES = GUIDE / "images"
LINK = re.compile(r'(!?)\[[^\]\n]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)')


def prose(path: Path) -> str:
    text = path.read_text(encoding="utf-8-sig")
    return re.sub(r"(?ms)^```[^\n]*\n.*?^```[^\n]*$", "", text)


def anchors(text: str) -> set[str]:
    result = set(re.findall(r'<a\s+(?:id|name)="([^"]+)"', text))
    used: set[str] = set()
    for heading in re.findall(r"(?m)^#{1,6}\s+(.+?)\s*#*\s*$", text):
        base = re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")
        slug, suffix = base, 0
        while slug in used:
            suffix += 1
            slug = f"{base}-{suffix}"
        used.add(slug)
        result.add(slug)
    return result


def exact_path(path: Path) -> None:
    parent = ROOT
    for part in path.relative_to(ROOT).parts:
        if part not in {entry.name for entry in parent.iterdir()}:
            raise ValueError(f"Missing path or incorrect casing: {path.relative_to(ROOT)}")
        parent /= part


def main() -> int:
    docs = sorted(ROOT.glob("*.md")) + sorted(
        path for path in (ROOT / "docs").rglob("*.md")
        if "upstream" not in path.relative_to(ROOT / "docs").parts
    )
    content = {path: prose(path) for path in docs}
    embedded: set[Path] = set()
    checked = 0
    errors = []
    for source, text in content.items():
        for image, target in LINK.findall(text):
            parts = urlsplit(target.strip("<>"))
            if parts.scheme or parts.netloc:
                continue
            path = (source.parent / unquote(parts.path)).resolve() if parts.path else source
            try:
                exact_path(path)
                if parts.fragment and path.suffix.lower() == ".md":
                    if unquote(parts.fragment) not in anchors(content.get(path, "") or prose(path)):
                        raise ValueError(f"Missing anchor: {target}")
                if image:
                    if not path.is_file():
                        raise ValueError(f"Image is not a file: {target}")
                    if source.parent == GUIDE:
                        embedded.add(path)
                checked += 1
            except (OSError, ValueError) as error:
                errors.append(f"{source.relative_to(ROOT)}: {error}")

    screenshots = sorted(IMAGES.glob("*.png"))
    if not screenshots:
        errors.append("No help screenshots found")
    for path in screenshots:
        header = path.read_bytes()[:24]
        if (len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n"
                or header[12:16] != b"IHDR" or 0 in struct.unpack(">II", header[16:24])):
            errors.append(f"Invalid PNG header: {path.name}")
        if path not in embedded:
            errors.append(f"Screenshot not embedded in a guide chapter: {path.name}")
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {len(docs)} Markdown files, {checked} local links and anchors")
    print(f"PASS: {len(screenshots)} PNG screenshots, all embedded in guide chapters")
    print("Scope: local documentation checks; external links and game behavior are not tested.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
