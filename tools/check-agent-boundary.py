#!/usr/bin/env python3
"""Keep CLI binary names in the adapter, outside UI and terminal code."""
import re
import sys
from pathlib import Path

violations = []
for path in Path(sys.argv[1]).rglob('*'):
    if path.suffix not in {'.c', '.h', '.ui'} or path.name == 'termac-agent.c':
        continue
    for number, line in enumerate(path.read_text().splitlines(), 1):
        if re.search(r'"(?:claude|codex)"', line) or (
            path.name not in {'termac-attachments.c', 'termac-attachments.h'}
            and re.search(r'termac_attachments_quote\s*\(', line)
        ):
            violations.append(f'{path}:{number}: CLI name belongs in termac-agent.c')
if violations:
    sys.exit('\n'.join(violations))
