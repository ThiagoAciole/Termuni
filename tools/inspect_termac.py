#!/usr/bin/env python3
"""Heuristic read-only audit for Termac. This is NOT proof of linkage or ABI."""
import argparse, json, subprocess, os, re
from pathlib import Path

IGNORE={'.git','.cache','.zig-cache','zig-cache','zig-out','node_modules','target','build','dist','vendor','.venv','__pycache__','.agents','.claude','.flatpak-builder','.superpowers','tools','prompts','references'}
# Audit the Termac frontend, not the fetched Ghostty application or build caches.
# Keep subprojects/packagefiles/ghostty: it is our Meson integration.
DEPENDENCY_PATHS={Path('subprojects/ghostty'), Path('subprojects/packagecache')}
SIGNALS={
    'libghostty-internal': [r'include/ghostty\.h',r'ghostty_app_new',r'ghostty_surface_new',r'libghostty-internal'],
    'libghostty-vt': [r'ghostty/vt\.h',r'libghostty-vt',r'emit-lib-vt'],
    'ghostty-gtk': [r'src/apprt/gtk',r'@import\("apprt/gtk',r'ghostty\.gtk'],
    'VTE legado': [r'VteTerminal',r'vte-2\.91',r'vte-2\.91-gtk4',r'\bvte_terminal_'],
    'GTK4': [r'gtk4',r'gtk/gtk\.h',r'@cImport'],
    'libadwaita': [r'libadwaita',r'adw_application',r'adw-1'],
    'Zig': [r'build\.zig',r'build\.zig\.zon'],
    'Meson': [r'meson\.build'],
    'Flatpak': [r'flatpak',r'finish-args'],
}
NEEDLES={k:[re.compile(p,re.I) for p in pats] for k,pats in SIGNALS.items()}
SCAN_EXT={'.zig','.zon','.c','.h','.cpp','.hpp','.toml','.json','.yml','.yaml','.xml','.ui','.blp','.sh','.in','.meson'}
SCAN_NAMES={'meson.build','meson_options.txt','CMakeLists.txt','Makefile'}

def version(cmd):
    try:
        p=subprocess.run(cmd,capture_output=True,text=True,timeout=5)
        return p.stdout.strip() if p.returncode==0 else None
    except (OSError,subprocess.TimeoutExpired):
        return None

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',type=Path,default=Path('.'))
    ap.add_argument('--json',action='store_true')
    args=ap.parse_args(); root=args.root.resolve()
    findings={k:[] for k in SIGNALS}; files=0; errors=[]
    for base,dirs,names in os.walk(root):
        dirs[:]=[d for d in dirs if d not in IGNORE and not d.startswith('.git')
                 and not d.startswith('build-')
                 and (Path(base)/d).relative_to(root) not in DEPENDENCY_PATHS]
        for name in names:
            if name in ('SOURCES.md','SKILLS_INDEX.md','AGENTS.md'): continue
            f=Path(base)/name
            if f.suffix.lower() not in SCAN_EXT and name not in SCAN_NAMES: continue
            if f.is_symlink(): continue
            try:
                if f.stat().st_size>400_000: continue
                content=f.read_text('utf-8',errors='replace')
            except OSError as e:
                errors.append(f'{f}: {e}'); continue
            files+=1
            rel=f.relative_to(root).as_posix()
            haystack=rel+'\n'+content
            for label,patterns in NEEDLES.items():
                if any(p.search(haystack) for p in patterns):
                    if len(findings[label])<25: findings[label].append(rel)
    git_revision=version(['git','-C',str(root),'rev-parse','--short','HEAD'])
    info={'root':str(root),'files_scanned':files,'signals':findings,'versions':{
        'gtk4':version(['pkg-config','--modversion','gtk4']),
        'libadwaita-1':version(['pkg-config','--modversion','libadwaita-1']),
        'zig':version(['zig','version']),
        'meson':version(['meson','--version']),
        'flatpak':version(['flatpak','--version'])},'git_revision':git_revision,'errors':errors[:10],
        'warning':'Heuristic content scan only; verify ABI, linked objects, real source paths and pinned revision manually. No files changed.'}
    if args.json: print(json.dumps(info,ensure_ascii=False,indent=2))
    else:
        print(f'\nTERM-ENGINE AUDIT: {root}\nScanned {files} source files; git {git_revision or "unknown"}')
        for k,v in findings.items(): print(f'  {k}: {len(v)} matching files'+(f' (examples: {", ".join(v[:4])})' if v else ''))
        print('Versions:',info['versions']); print('\n',info['warning'])

if __name__=='__main__': main()
