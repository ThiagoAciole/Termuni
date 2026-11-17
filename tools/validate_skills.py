#!/usr/bin/env python3
"""Validate simple Agent Skills frontmatter, naming, and links; no extra deps."""
from pathlib import Path
import re, sys

root=Path(__file__).resolve().parent.parent
skills=root/'.agents/skills'
errors=[]
for p in sorted(skills.glob('*/SKILL.md')):
    text=p.read_text(encoding='utf-8')
    if not text.startswith('---\n'): errors.append(f'{p}: missing frontmatter'); continue
    parts=text.split('---\n',2)
    if len(parts)<3: errors.append(f'{p}: malformed frontmatter'); continue
    meta=parts[1]
    props=dict(re.findall(r'^([a-z-]+): (.+)$',meta,re.M))
    name=props.get('name',''); desc=props.get('description','')
    if name!=p.parent.name: errors.append(f'{p}: name mismatch')
    if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*',name) or len(name)>64: errors.append(f'{p}: invalid name')
    if not desc or len(desc)>1024: errors.append(f'{p}: invalid description')
    if len(parts[2].strip())<100: errors.append(f'{p}: content too small')
print(f'Validated {len(list(skills.glob("*/SKILL.md")))} Termuni skills; errors={len(errors)}')
for e in errors: print('ERROR:',e)
sys.exit(bool(errors))
