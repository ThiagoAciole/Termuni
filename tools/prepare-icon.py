#!/usr/bin/env python3
"""Pad the installed SVG canvas to a square; preserve the source artwork."""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

source = Path(sys.argv[1]).read_text()
root = ET.fromstring(source)
x, y, width, height = map(float, root.attrib['viewBox'].split())
side = max(width, height)
x -= (side - width) / 2
y -= (side - height) / 2
header = re.search(r'<svg\b[^>]*>', source).group()
updated = header
for key, value in [('width', f'{side:g}'), ('height', f'{side:g}'),
                   ('viewBox', f'{x:g} {y:g} {side:g} {side:g}')]:
    updated = re.sub(rf'\b{key}="[^"]*"', f'{key}="{value}"', updated)
Path(sys.argv[2]).write_text(source.replace(header, updated, 1))
