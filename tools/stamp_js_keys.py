#!/usr/bin/env python3
"""Stamps the JS cache keys: savefile.js's hash into installer.js's import, then
installer.js's hash into index.html. Run by build_web.sh; run it on its own
after editing web/*.js without rebuilding firmware. Why the keys exist and why
the order matters: see the comment above its call in build_web.sh.

  python3 tools/stamp_js_keys.py      (from the repository root)
"""
import hashlib, pathlib, re

def key(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()[:16]

save_key = key('web/savefile.js')
installer = pathlib.Path('web/installer.js')
src = installer.read_bytes().decode('utf-8')
src, n = re.subn(r"(from '\./savefile\.js)(?:\?v=[0-9a-f]+)?'",
                 lambda m: f"{m.group(1)}?v={save_key}'", src)
if n != 1:
    raise SystemExit(f"expected exactly one savefile.js import in installer.js, found {n}")
installer.write_bytes(src.encode('utf-8'))

installer_key = key('web/installer.js')
index = pathlib.Path('web/index.html')
html = index.read_bytes().decode('utf-8')
html, n = re.subn(r'(src="installer\.js)(?:\?v=[0-9a-f]+)?"',
                  lambda m: f'{m.group(1)}?v={installer_key}"', html)
if n != 1:
    raise SystemExit(f"expected exactly one installer.js script tag, found {n}")
index.write_bytes(html.encode('utf-8'))
print(f'js cache keys -> savefile {save_key}, installer {installer_key}')
