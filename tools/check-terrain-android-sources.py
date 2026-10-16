"""Check changed terrain/transfer TUs against the configured Android toolchain.

This is a compile check, not an APK build or device/runtime validation.
Usage: python tools/check-terrain-android-sources.py <compile_commands.json>
"""
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

def command_arguments(command):
    if os.name != 'nt':
        return shlex.split(command)
    import ctypes
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    argv = parse(command, ctypes.byref(count))
    if not argv:
        raise ValueError('Invalid Windows compilation command')
    try:
        return [argv[i] for i in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(argv, ctypes.c_void_p))

targets = {
    'terrain_asset.cpp', 'editor_terrain.cpp', 'editor_terrain_screen.cpp',
    'upload_context.cpp', 'dirt_road_resources.cpp', 'instanced_renderer.cpp',
}
commands = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
seen = set()
for entry in commands:
    name = Path(entry['file']).name
    if name not in targets or name in seen:
        continue
    seen.add(name)
    args = entry.get('arguments') or command_arguments(entry['command'])
    cleaned = []
    index = 0
    while index < len(args):
        arg = args[index]
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            index += 2
            continue
        if arg not in ('-c', '-MD', '-MMD'):
            cleaned.append(arg)
        index += 1
    cleaned.append('-fsyntax-only')
    print(f'Android ARM64: {name}', flush=True)
    result = subprocess.run(cleaned, cwd=entry['directory'])
    if result.returncode:
        sys.exit(result.returncode)
missing = targets - seen
if missing:
    raise SystemExit(f'Missing Android compilation entries: {sorted(missing)}')
print(f'{len(seen)}/{len(targets)} Android translation units passed; no APK/runtime claim.', flush=True)
